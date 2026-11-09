// =============================================================================
//  api.mts - API del Sistema Cine en Netlify Functions.
//  Es la traduccion de backend/src/main.cpp: mismas rutas, mismas validaciones
//  y mismas respuestas JSON (ver docs/rutas-api.md). En vez de SQLite usa
//  Netlify Database (Postgres) y los posters se guardan en Netlify Blobs.
// =============================================================================
import type { Config } from "@netlify/functions";
import { getStore } from "@netlify/blobs";
import { createHash, randomBytes, timingSafeEqual } from "node:crypto";
import { and, asc, count, eq, gt, inArray, lt, sql } from "drizzle-orm";
import { db } from "../../db/index.js";
import {
  administradores, butacas, clientes, funciones, peliculas, reservaciones,
  reservasTemporales, salas, sesionesAdmin,
} from "../../db/schema.js";

// ------------------------------------------------------------------ Constantes
const MAX_AGRUPACIONES = 10;
const MAX_BUTACAS = 60;
const MAX_POR_RESERVA = 10;
const MAX_IMAGEN = 3 * 1024 * 1024;
const MINUTOS_RESERVA = 20;
const HORAS_SESION = 8;
const ADMIN_INICIAL = "admin";
const CLAVE_INICIAL = "imperium2026";
const ZONA_HORARIA = "America/Costa_Rica";

const DISPONIBLE = 0, OCUPADA = 1, DANADA = 2, SELECCIONADA = 3;

class ErrorApi extends Error {
  constructor(public estado: number, mensaje: string, public extra?: Record<string, unknown>) {
    super(mensaje);
  }
}
const fallo = (estado: number, mensaje: string): never => { throw new ErrorApi(estado, mensaje); };
const json = (datos: unknown, estado = 200) => Response.json(datos, { status: estado });

// ------------------------------------------------------------------ Ayudantes
function fechaHoy(): string {
  return new Intl.DateTimeFormat("en-CA", {
    timeZone: ZONA_HORARIA, year: "numeric", month: "2-digit", day: "2-digit",
  }).format(new Date());
}

const recortar = (t: string) => t.replace(/^[ \t]+|[ \t]+$/g, "");
const esEntero = (v: unknown): v is number => typeof v === "number" && Number.isInteger(v);
const etiquetaButaca = (a: number, n: number) => String.fromCharCode(65 + a) + (n + 1);

async function leerCuerpo(req: Request): Promise<Record<string, any>> {
  try {
    const j = await req.json();
    if (j && typeof j === "object" && !Array.isArray(j)) return j;
  } catch { /* cae abajo */ }
  return fallo(400, "JSON invalido");
}

function fechaValida(f: string): boolean {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(f)) return false;
  const [anio, mes, dia] = f.split("-").map(Number);
  if (anio < 2000 || anio > 2100 || mes < 1 || mes > 12 || dia < 1) return false;
  const dias = [31, (anio % 4 === 0 && anio % 100 !== 0) || anio % 400 === 0 ? 29 : 28,
    31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  return dia <= dias[mes - 1];
}
const horaValida = (h: string) => /^([01]\d|2[0-3]):[0-5]\d$/.test(h);

function validarHabilitada(j: Record<string, any>): number {
  if (!esEntero(j.habilitada)) fallo(400, "Falta el campo habilitada (0 o 1)");
  if (j.habilitada !== 0 && j.habilitada !== 1) fallo(400, "habilitada solo puede ser 0 o 1");
  return j.habilitada;
}

// ------------------------------------------------------------------ Seguridad
const sha256 = (t: string) => createHash("sha256").update(t, "utf8").digest("hex");
function hashClave(sal: string, clave: string): string {
  let h = sha256(sal + clave);
  for (let i = 0; i < 10000; i++) h = sha256(sal + h);
  return h;
}
function igualesSeguro(a: string, b: string): boolean {
  const x = Buffer.from(a), y = Buffer.from(b);
  return x.length === y.length && timingSafeEqual(x, y);
}
const textoAleatorio = (bytes = 16) => randomBytes(bytes).toString("hex");

function tokenDe(req: Request): string {
  const h = req.headers.get("authorization") ?? "";
  return h.startsWith("Bearer ") ? h.slice(7) : "";
}

async function usuarioDeSesion(req: Request): Promise<string> {
  const token = tokenDe(req);
  if (!token) return "";
  const [s] = await db.select().from(sesionesAdmin)
    .where(and(eq(sesionesAdmin.token, token), gt(sesionesAdmin.vence, new Date())));
  return s?.usuario ?? "";
}

async function exigirAdmin(req: Request) {
  if (!(await usuarioDeSesion(req))) fallo(401, "Debe iniciar sesion como administrador");
}

// Primer arranque: si no hay administradores se crea el inicial.
async function asegurarAdminInicial() {
  const [{ total }] = await db.select({ total: count() }).from(administradores);
  if (total === 0) {
    const sal = textoAleatorio(16);
    await db.insert(administradores)
      .values({ usuario: ADMIN_INICIAL, sal, claveHash: hashClave(sal, CLAVE_INICIAL) })
      .onConflictDoNothing();
  }
}

// ------------------------------------------------------------------ Consultas
const columnasSala = {
  id: salas.id, nombre: salas.nombre, agrupaciones: salas.agrupaciones,
  butacas_por_agrupacion: salas.butacasPorAgrupacion, habilitada: salas.habilitada,
};

async function obtenerSala(id: number) {
  const [s] = await db.select(columnasSala).from(salas).where(eq(salas.id, id));
  return s;
}

async function listarPeliculas(soloCartelera: boolean) {
  const hoy = fechaHoy();
  const filtro = soloCartelera
    ? and(sql`${hoy} BETWEEN ${peliculas.fechaInicio} AND ${peliculas.fechaFin}`, eq(salas.habilitada, 1))
    : undefined;
  const lista = await db.select({
    id: peliculas.id, nombre: peliculas.nombre, fecha_inicio: peliculas.fechaInicio,
    fecha_fin: peliculas.fechaFin, precio: peliculas.precio, sala_id: peliculas.salaId,
    sala: salas.nombre, imagen: peliculas.imagen,
  }).from(peliculas).innerJoin(salas, eq(salas.id, peliculas.salaId))
    .where(filtro).orderBy(asc(peliculas.nombre));

  const ids = lista.map((p) => p.id);
  const horarios = ids.length
    ? await db.select().from(funciones).where(inArray(funciones.peliculaId, ids)).orderBy(asc(funciones.hora))
    : [];
  return lista.map((p) => ({
    ...p,
    precio: Number(p.precio),
    horarios: horarios.filter((h) => h.peliculaId === p.id).map((h) => ({ funcion_id: h.id, hora: h.hora })),
  }));
}

async function obtenerFuncion(funcionId: number) {
  const hoy = fechaHoy();
  const [f] = await db.select({
    funcion_id: funciones.id, hora: funciones.hora, pelicula: peliculas.nombre,
    precio: peliculas.precio, sala_id: salas.id, sala: salas.nombre,
    agrupaciones: salas.agrupaciones, butacas_por_agrupacion: salas.butacasPorAgrupacion,
    sala_habilitada: salas.habilitada,
    en_cartelera: sql<boolean>`(${hoy} BETWEEN ${peliculas.fechaInicio} AND ${peliculas.fechaFin})`,
  }).from(funciones)
    .innerJoin(peliculas, eq(peliculas.id, funciones.peliculaId))
    .innerJoin(salas, eq(salas.id, peliculas.salaId))
    .where(eq(funciones.id, funcionId));
  return f ? { ...f, precio: Number(f.precio) } : undefined;
}
type InfoFuncion = NonNullable<Awaited<ReturnType<typeof obtenerFuncion>>>;

function validarFuncionReservable(f: InfoFuncion | undefined): InfoFuncion {
  if (!f) return fallo(404, "La funcion no existe");
  if (f.sala_habilitada === 0) fallo(400, "La sala de esta funcion esta inhabilitada");
  if (!f.en_cartelera) fallo(400, "Esta pelicula no esta en exhibicion hoy");
  return f;
}

// Carga la matriz de la sala con las reservas ya guardadas para esa fecha.
async function cargarMatriz(f: InfoFuncion, fecha: string, conexion: any = db) {
  const matriz = Array.from({ length: f.agrupaciones }, () => Array(f.butacas_por_agrupacion).fill(DISPONIBLE));
  const ids = Array.from({ length: f.agrupaciones }, () => Array(f.butacas_por_agrupacion).fill(0));
  const filas = await conexion.select({
    id: butacas.id, a: butacas.agrupacion, n: butacas.numero, habilitada: butacas.habilitada,
    reserva: reservaciones.id,
  }).from(butacas).leftJoin(reservaciones, and(
    eq(reservaciones.butacaId, butacas.id), eq(reservaciones.funcionId, f.funcion_id), eq(reservaciones.fecha, fecha),
  )).where(eq(butacas.salaId, f.sala_id));

  for (const b of filas) {
    if (b.a < 0 || b.a >= f.agrupaciones || b.n < 0 || b.n >= f.butacas_por_agrupacion) continue;
    ids[b.a][b.n] = b.id;
    if (b.habilitada === 0) matriz[b.a][b.n] = DANADA;
    else if (b.reserva !== null) matriz[b.a][b.n] = OCUPADA;
  }
  return { matriz: matriz as number[][], ids: ids as number[][] };
}

const contarDisponibles = (m: number[][]) => m.flat().filter((e) => e === DISPONIBLE).length;
function etiquetasNuevas(m: number[][]): string[] {
  const r: string[] = [];
  m.forEach((fila, a) => fila.forEach((e, n) => { if (e === SELECCIONADA) r.push(etiquetaButaca(a, n)); }));
  return r;
}

async function reservaVigente(token: string, conexion: any = db, bloquear = false) {
  const limite = new Date(Date.now() - MINUTOS_RESERVA * 60_000);
  let q = conexion.select().from(reservasTemporales)
    .where(and(eq(reservasTemporales.token, token), gt(reservasTemporales.creada, limite)));
  if (bloquear) q = q.for("update");
  const [r] = await q;
  if (!r) fallo(404, "La reservacion expiro. Empiece de nuevo");
  return r as typeof reservasTemporales.$inferSelect;
}

// ------------------------------------------------------------------ Posters
async function guardarPoster(dataUrl: string): Promise<string> {
  let extension: string, prefijo: string, tipo: string;
  if (dataUrl.startsWith("data:image/jpeg;base64,")) { prefijo = "data:image/jpeg;base64,"; extension = ".jpg"; tipo = "image/jpeg"; }
  else if (dataUrl.startsWith("data:image/png;base64,")) { prefijo = "data:image/png;base64,"; extension = ".png"; tipo = "image/png"; }
  else return fallo(400, "La imagen debe ser JPG o PNG");
  if (dataUrl.length > MAX_IMAGEN) fallo(400, "La imagen es demasiado grande");

  const base64 = dataUrl.slice(prefijo.length);
  if (!/^[A-Za-z0-9+/]+=*$/.test(base64)) fallo(400, "La imagen no es valida");
  const bytes = Buffer.from(base64, "base64");
  if (bytes.length === 0) fallo(400, "La imagen no es valida");

  const nombre = `pelicula-${Date.now()}${extension}`;
  const ab = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;
  await getStore("posters").set(nombre, ab, { metadata: { tipo } });
  return `img/peliculas/${nombre}`;
}

// ------------------------------------------------------------------ Rutas
async function rutear(req: Request): Promise<Response> {
  const ruta = new URL(req.url).pathname.replace(/\/+$/, "");
  const m = req.method;
  let p: RegExpMatchArray | null;

  if (m === "GET" && ruta === "/api/ping") return json({ mensaje: "pong" });

  // ---------------------------------------------------------- Login
  if (m === "POST" && ruta === "/api/login") {
    const j = await leerCuerpo(req);
    if (typeof j.usuario !== "string" || typeof j.clave !== "string")
      fallo(400, "Escriba el usuario y la contrasena");
    await asegurarAdminInicial();
    const usuario = recortar(j.usuario);
    const [admin] = await db.select().from(administradores).where(eq(administradores.usuario, usuario));
    const correcto = !!admin && igualesSeguro(hashClave(admin.sal, j.clave), admin.claveHash);
    if (!correcto) {
      await new Promise((r) => setTimeout(r, 600));
      fallo(401, "Usuario o contrasena incorrectos");
    }
    await db.delete(sesionesAdmin).where(lt(sesionesAdmin.vence, new Date()));
    const token = textoAleatorio(32);
    await db.insert(sesionesAdmin).values({
      token, usuario: admin.usuario, vence: new Date(Date.now() + HORAS_SESION * 3600_000),
    });
    return json({ ok: true, token, usuario: admin.usuario });
  }

  if (m === "GET" && ruta === "/api/sesion") {
    const usuario = await usuarioDeSesion(req);
    if (!usuario) fallo(401, "Sesion vencida o inexistente");
    return json({ ok: true, usuario });
  }

  if (m === "POST" && ruta === "/api/logout") {
    const token = tokenDe(req);
    if (token) await db.delete(sesionesAdmin).where(eq(sesionesAdmin.token, token));
    return json({ ok: true });
  }

  // ---------------------------------------------------------- Salas
  if (m === "GET" && ruta === "/api/salas") {
    return json(await db.select(columnasSala).from(salas).orderBy(asc(salas.nombre)));
  }

  if (m === "POST" && ruta === "/api/salas") {
    await exigirAdmin(req);
    const j = await leerCuerpo(req);
    if (typeof j.nombre !== "string") fallo(400, "Falta el nombre de la sala");
    if (!esEntero(j.agrupaciones)) fallo(400, "La cantidad de agrupaciones debe ser un numero entero");
    if (!esEntero(j.butacas_por_agrupacion)) fallo(400, "La cantidad de butacas por agrupacion debe ser un numero entero");
    const nombre = recortar(j.nombre);
    const agr: number = j.agrupaciones, por: number = j.butacas_por_agrupacion;
    if (!nombre) fallo(400, "El nombre de la sala no puede estar vacio");
    if (agr < 1 || agr > MAX_AGRUPACIONES) fallo(400, `Las agrupaciones deben estar entre 1 y ${MAX_AGRUPACIONES}`);
    if (por < 1 || por > MAX_BUTACAS) fallo(400, `Las butacas por agrupacion deben estar entre 1 y ${MAX_BUTACAS}`);

    const [existe] = await db.select({ id: salas.id }).from(salas).where(eq(salas.nombre, nombre));
    if (existe) fallo(400, "Ya existe una sala llamada " + nombre);

    const id = await db.transaction(async (tx) => {
      const [sala] = await tx.insert(salas)
        .values({ nombre, agrupaciones: agr, butacasPorAgrupacion: por }).returning({ id: salas.id });
      const filas = [];
      for (let a = 0; a < agr; a++) for (let n = 0; n < por; n++) filas.push({ salaId: sala.id, agrupacion: a, numero: n });
      await tx.insert(butacas).values(filas);
      return sala.id;
    });
    return json({ ok: true, id }, 201);
  }

  if (m === "PUT" && (p = ruta.match(/^\/api\/salas\/(\d+)\/habilitada$/))) {
    await exigirAdmin(req);
    const habilitada = validarHabilitada(await leerCuerpo(req));
    const r = await db.update(salas).set({ habilitada }).where(eq(salas.id, Number(p[1]))).returning({ id: salas.id });
    if (!r.length) fallo(404, "Sala no encontrada");
    return json({ ok: true });
  }

  if (m === "GET" && (p = ruta.match(/^\/api\/salas\/(\d+)\/butacas$/))) {
    const id = Number(p[1]);
    const sala = await obtenerSala(id);
    if (!sala) fallo(404, "Sala no encontrada");
    const lista = await db.select({
      id: butacas.id, sala_id: butacas.salaId, agrupacion: butacas.agrupacion,
      numero: butacas.numero, habilitada: butacas.habilitada,
    }).from(butacas).where(eq(butacas.salaId, id)).orderBy(asc(butacas.agrupacion), asc(butacas.numero));
    return json({ agrupaciones: sala!.agrupaciones, butacas_por_agrupacion: sala!.butacas_por_agrupacion, butacas: lista });
  }

  if (m === "PUT" && (p = ruta.match(/^\/api\/butacas\/(\d+)\/habilitada$/))) {
    await exigirAdmin(req);
    const habilitada = validarHabilitada(await leerCuerpo(req));
    const r = await db.update(butacas).set({ habilitada }).where(eq(butacas.id, Number(p[1]))).returning({ id: butacas.id });
    if (!r.length) fallo(404, "Butaca no encontrada");
    return json({ ok: true });
  }

  // ---------------------------------------------------------- Peliculas
  if (m === "GET" && ruta === "/api/peliculas") return json(await listarPeliculas(false));
  if (m === "GET" && ruta === "/api/cartelera") return json(await listarPeliculas(true));

  if (m === "POST" && ruta === "/api/peliculas") {
    await exigirAdmin(req);
    const j = await leerCuerpo(req);
    if (typeof j.nombre !== "string") fallo(400, "Falta el nombre de la pelicula");
    if (typeof j.fecha_inicio !== "string" || typeof j.fecha_fin !== "string") fallo(400, "Faltan las fechas de exhibicion");
    if (typeof j.precio !== "number" || !Number.isFinite(j.precio)) fallo(400, "El precio debe ser un numero");
    if (!esEntero(j.sala_id)) fallo(400, "Debe seleccionar una sala");
    if (!Array.isArray(j.horarios)) fallo(400, "Faltan los horarios");

    const nombre = recortar(j.nombre);
    if (!nombre) fallo(400, "El nombre de la pelicula no puede estar vacio");
    if (!fechaValida(j.fecha_inicio)) fallo(400, "La fecha de inicio no es valida (use AAAA-MM-DD)");
    if (!fechaValida(j.fecha_fin)) fallo(400, "La fecha de finalizacion no es valida (use AAAA-MM-DD)");
    if (j.fecha_fin < j.fecha_inicio) fallo(400, "La fecha de finalizacion no puede ser antes de la fecha de inicio");
    if (j.precio <= 0) fallo(400, "El precio debe ser mayor que 0");
    if (j.horarios.length === 0) fallo(400, "Debe agregar al menos un horario");
    const horarios: string[] = [];
    for (const h of j.horarios) {
      if (typeof h !== "string" || !horaValida(h)) fallo(400, "Hay un horario que no es valido (use HH:MM, por ejemplo 19:30)");
      if (horarios.includes(h)) fallo(400, `El horario ${h} esta repetido`);
      horarios.push(h);
    }

    const sala = await obtenerSala(j.sala_id);
    if (!sala) fallo(400, "La sala seleccionada no existe");
    if (sala!.habilitada === 0) fallo(400, "La sala seleccionada esta inhabilitada");

    let imagen = "";
    if (typeof j.imagen === "string" && j.imagen) imagen = await guardarPoster(j.imagen);

    try {
      const id = await db.transaction(async (tx) => {
        const [peli] = await tx.insert(peliculas).values({
          nombre, fechaInicio: j.fecha_inicio, fechaFin: j.fecha_fin, precio: j.precio, salaId: j.sala_id, imagen,
        }).returning({ id: peliculas.id });
        await tx.insert(funciones).values(horarios.map((hora) => ({ peliculaId: peli.id, hora })));
        return peli.id;
      });
      return json({ ok: true, id, imagen }, 201);
    } catch (e) {
      if (imagen) await getStore("posters").delete(imagen.split("/").pop()!).catch(() => {});
      throw e;
    }
  }

  // ---------------------------------------------------------- Reservacion
  if (m === "POST" && ruta === "/api/clientes") {
    const j = await leerCuerpo(req);
    if (typeof j.nombre !== "string") fallo(400, "Falta el nombre");
    if (typeof j.telefono !== "string") fallo(400, "Falta el numero de telefono");
    const nombre = recortar(j.nombre);
    if (nombre.length < 3) fallo(400, "Escriba el nombre completo");
    if (nombre.length > 60) fallo(400, "El nombre es demasiado largo");
    const telefono = j.telefono.replace(/[ -]/g, "");
    if (!/^\d*$/.test(telefono)) fallo(400, "El telefono solo puede tener numeros");
    if (telefono.length !== 8) fallo(400, "Ingrese un numero de 8 digitos");
    const [c] = await db.insert(clientes).values({ nombre, telefono }).returning({ id: clientes.id });
    return json({ ok: true, cliente_id: c.id }, 201);
  }

  if (m === "GET" && (p = ruta.match(/^\/api\/funciones\/(\d+)\/disponibles$/))) {
    const f = validarFuncionReservable(await obtenerFuncion(Number(p[1])));
    const { matriz } = await cargarMatriz(f, fechaHoy());
    return json({ disponibles: contarDisponibles(matriz) });
  }

  if (m === "POST" && ruta === "/api/reserva/iniciar") {
    const j = await leerCuerpo(req);
    if (!esEntero(j.cliente_id) || !esEntero(j.funcion_id) || !esEntero(j.cantidad))
      fallo(400, "Faltan cliente_id, funcion_id o cantidad");
    const [cli] = await db.select({ id: clientes.id }).from(clientes).where(eq(clientes.id, j.cliente_id));
    if (!cli) fallo(404, "Cliente no encontrado. Vuelva al paso 1");
    const f = validarFuncionReservable(await obtenerFuncion(j.funcion_id));
    const cantidad: number = j.cantidad;
    if (cantidad < 1 || cantidad > MAX_POR_RESERVA) fallo(400, `Puede reservar de 1 a ${MAX_POR_RESERVA} butacas`);

    const fecha = fechaHoy();
    const { matriz, ids } = await cargarMatriz(f, fecha);
    const disponibles = contarDisponibles(matriz);
    if (cantidad > disponibles) fallo(400, `Solo quedan ${disponibles} butacas disponibles en esta funcion`);

    // Limpia reservaciones temporales abandonadas
    await db.delete(reservasTemporales)
      .where(lt(reservasTemporales.creada, new Date(Date.now() - MINUTOS_RESERVA * 60_000)));
    const token = "r" + textoAleatorio(12);
    await db.insert(reservasTemporales).values({
      token, clienteId: j.cliente_id, funcionId: f.funcion_id, fecha, cantidad, matriz, ids,
    });
    return json({
      ok: true, token, cantidad, matriz,
      agrupaciones: f.agrupaciones, butacas_por_agrupacion: f.butacas_por_agrupacion,
    }, 201);
  }

  if (m === "POST" && (p = ruta.match(/^\/api\/reserva\/(\w+)\/marcar$/))) {
    const token = p[1];
    const j = await leerCuerpo(req);
    if (!esEntero(j.agrupacion) || !esEntero(j.numero)) fallo(400, "Faltan agrupacion y numero");
    const a: number = j.agrupacion, n: number = j.numero;

    return await db.transaction(async (tx) => {
      const r = await reservaVigente(token, tx, true);
      const matriz = r.matriz;
      if (a < 0 || a >= matriz.length || n < 0 || n >= matriz[a].length) fallo(400, "Esa butaca no existe en la sala");
      const estado = matriz[a][n];
      if (estado === OCUPADA || estado === DANADA) fallo(409, `La butaca ${etiquetaButaca(a, n)} no esta disponible`);
      let seleccionadas = r.seleccionadas;
      if (estado === DISPONIBLE) {
        if (seleccionadas >= r.cantidad) fallo(400, `Ya eligio las ${r.cantidad} butacas. Desmarque una para cambiarla`);
        matriz[a][n] = SELECCIONADA; seleccionadas++;
      } else {
        matriz[a][n] = DISPONIBLE; seleccionadas--;
      }
      await tx.update(reservasTemporales).set({ matriz, seleccionadas }).where(eq(reservasTemporales.token, token));
      return json({ ok: true, estado: matriz[a][n], seleccionadas, butacas: etiquetasNuevas(matriz) });
    });
  }

  if (m === "POST" && (p = ruta.match(/^\/api\/reserva\/(\w+)\/confirmar$/))) {
    const token = p[1];
    const r = await reservaVigente(token);
    if (r.seleccionadas !== r.cantidad) fallo(400, `Debe elegir ${r.cantidad} butacas`);
    const f = await obtenerFuncion(r.funcionId);
    if (!f) fallo(404, "La funcion no existe");

    const nuevos: number[] = [];
    r.matriz.forEach((fila, a) => fila.forEach((e, n) => { if (e === SELECCIONADA) nuevos.push(r.ids[a][n]); }));

    // Guarda todo o nada. Si otro cliente gano una butaca, la restriccion
    // UNIQUE o la revision previa hacen que no se guarde ninguna.
    let conflicto = false;
    try {
      await db.transaction(async (tx) => {
        const revisar = await tx.select({ id: butacas.id, habilitada: butacas.habilitada, reserva: reservaciones.id })
          .from(butacas).leftJoin(reservaciones, and(
            eq(reservaciones.butacaId, butacas.id), eq(reservaciones.funcionId, r.funcionId), eq(reservaciones.fecha, r.fecha),
          )).where(inArray(butacas.id, nuevos));
        if (revisar.length !== nuevos.length || revisar.some((b) => b.habilitada === 0 || b.reserva !== null)) {
          throw new ErrorApi(409, "conflicto");
        }
        await tx.insert(reservaciones).values(nuevos.map((butacaId) => ({
          funcionId: r.funcionId, butacaId, clienteId: r.clienteId, fecha: r.fecha,
        })));
        await tx.delete(reservasTemporales).where(eq(reservasTemporales.token, token));
      });
    } catch (e: any) {
      const unico = e?.code === "23505" || e?.cause?.code === "23505";
      if (!(e instanceof ErrorApi) && !unico) throw e;
      conflicto = true;
    }

    if (conflicto) {
      const { matriz: nueva, ids } = await cargarMatriz(f!, r.fecha);
      let seleccionadas = 0;
      nueva.forEach((fila, a) => fila.forEach((e, n) => {
        if (r.matriz[a]?.[n] === SELECCIONADA && e === DISPONIBLE) { nueva[a][n] = SELECCIONADA; seleccionadas++; }
      }));
      await db.update(reservasTemporales).set({ matriz: nueva, ids, seleccionadas }).where(eq(reservasTemporales.token, token));
      return json({
        error: "Otra persona acaba de reservar alguna de sus butacas. Elija otra.",
        matriz: nueva, seleccionadas, butacas: etiquetasNuevas(nueva),
      }, 409);
    }

    return json({
      ok: true, total: f!.precio * r.cantidad, butacas: etiquetasNuevas(r.matriz),
      pelicula: f!.pelicula, sala: f!.sala, hora: f!.hora,
    }, 201);
  }

  if (m === "POST" && (p = ruta.match(/^\/api\/reserva\/(\w+)\/cancelar$/))) {
    await db.delete(reservasTemporales).where(eq(reservasTemporales.token, p[1]));
    return json({ ok: true });
  }

  return json({ error: "Ruta no encontrada" }, 404);
}

export default async (req: Request) => {
  try {
    return await rutear(req);
  } catch (e) {
    if (e instanceof ErrorApi) return json({ error: e.message, ...e.extra }, e.estado);
    console.error("[ERROR]", e);
    return json({ error: "Error interno del servidor" }, 500);
  }
};

export const config: Config = {
  path: "/api/*",
};

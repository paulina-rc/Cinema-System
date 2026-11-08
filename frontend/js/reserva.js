// =============================================================================
//  reserva.js - Flujo de reservación del cliente (pasos 1 a 5)
//  -----------------------------------------------------------------------------
//  Toda la página es UN solo HTML con 5 <section>. Este archivo muestra una a
//  la vez y habla con el backend en C++ usando fetch (ver docs/rutas-api.md).
//
//  IMPORTANTE para la defensa: el color de una butaca NO lo decide JavaScript.
//  Al hacer clic se le pregunta al backend; él cambia la matriz en memoria y
//  responde el estado nuevo. Solo entonces se pinta la butaca.
// =============================================================================

// Estados de la matriz: los mismos números que en ReservaTemporal.h
const DISPONIBLE = 0, OCUPADA = 1, DANADA = 2, SELECCIONADA = 3;
const MAX_POR_RESERVA = 10;   // igual que en main.cpp
const SEGUNDOS_REGRESO = 10;  // tiempo antes de volver a la cartelera al terminar

// -----------------------------------------------------------------------------
//  Datos de la reservación mientras el cliente avanza por los pasos.
//  Viven solo en esta variable (no se guardan en el navegador).
// -----------------------------------------------------------------------------
const reserva = {
    cliente_id: null,
    nombre: "",
    telefono: "",
    pelicula: null,       // objeto que vino de /api/cartelera
    funcion: null,        // { funcion_id, hora }
    disponibles: 0,
    cantidad: 1,
    token: null,          // identifica la matriz en memoria del backend
    matriz: [],
    seleccionadas: 0,
    butacas: []           // nombres de las butacas elegidas, ej. ["A5", "B2"]
};

let pasoActual = 1;
let cartelera = [];
const $ = (id) => document.getElementById(id);


// =============================================================================
//  Ayudantes
// =============================================================================

// Llama a la API. Si el backend responde con error, lanza un Error con su
// mensaje. error.estado = código HTTP, error.datos = JSON completo.
async function llamarApi(ruta, metodo = "GET", cuerpo = null) {
    const opciones = { method: metodo, headers: {} };
    if (cuerpo !== null) {
        opciones.headers["Content-Type"] = "application/json";
        opciones.body = JSON.stringify(cuerpo);
    }

    let respuesta;
    try {
        respuesta = await fetch(ruta, opciones);
    } catch {
        throw new Error("No hay conexión con el servidor. ¿Está corriendo iniciar.bat?");
    }

    const datos = await respuesta.json().catch(() => ({}));
    if (!respuesta.ok) {
        const error = new Error(datos.error || "Ocurrió un error (" + respuesta.status + ")");
        error.estado = respuesta.status;
        error.datos = datos;
        throw error;
    }
    return datos;
}

// Mensaje flotante de error (desaparece solo).
let temporizadorToast;
function mostrarError(mensaje) {
    const toast = $("toast");
    toast.textContent = mensaje;
    toast.hidden = false;
    clearTimeout(temporizadorToast);
    temporizadorToast = setTimeout(() => (toast.hidden = true), 4500);
}

// "19:30" -> "7:30 p.m."
function formatoHora(hora) {
    let [h, m] = hora.split(":").map(Number);
    const sufijo = h >= 12 ? "p.m." : "a.m.";
    h = h % 12 || 12;
    return `${h}:${String(m).padStart(2, "0")} ${sufijo}`;
}

// 3500 -> "₡3 500"
function formatoColones(monto) {
    return "₡" + Number(monto).toLocaleString("es-CR", { maximumFractionDigits: 0 });
}

// "2026-12-31" -> "31/12/2026"
function formatoFecha(fecha) {
    const [a, m, d] = fecha.split("-");
    return `${d}/${m}/${a}`;
}

// Mismo nombre que en C++: agrupación 0 -> "A", número 4 -> "5"  =>  "A5"
function etiquetaButaca(agrupacion, numero) {
    return String.fromCharCode(65 + agrupacion) + (numero + 1);
}

// Para escribir texto del usuario dentro de innerHTML sin riesgo.
function escaparHtml(texto) {
    const div = document.createElement("div");
    div.textContent = texto;
    return div.innerHTML;
}

function textoFuncion() {
    return `${reserva.pelicula.sala} · ${formatoHora(reserva.funcion.hora)}`;
}


// =============================================================================
//  Navegación entre pasos
// =============================================================================
function irAPaso(numero) {
    pasoActual = numero;
    $("toast").hidden = true;   // el mensaje de error es del paso anterior

    for (let i = 1; i <= 5; i++) $("paso" + i).hidden = i !== numero;

    document.querySelectorAll("#barraPasos li").forEach((li) => {
        const n = Number(li.dataset.paso);
        li.classList.toggle("hecho", n < numero);
        li.classList.toggle("actual", n === numero);
    });

    window.scrollTo({ top: 0, behavior: "smooth" });
}

// Botones "Atrás" que tienen data-ir="N"
document.querySelectorAll("[data-ir]").forEach((boton) => {
    boton.addEventListener("click", () => irAPaso(Number(boton.dataset.ir)));
});

// Borra la matriz temporal en el backend (sin guardar nada).
function cancelarReservaTemporal() {
    if (!reserva.token) return;
    llamarApi(`/api/reserva/${reserva.token}/cancelar`, "POST").catch(() => {});
    reserva.token = null;
}

$("btnCancelar").addEventListener("click", () => {
    cancelarReservaTemporal();
    window.location.href = "cartelera.html";
});

// Si el cliente cierra la pestaña a medio proceso, se libera la matriz.
window.addEventListener("pagehide", () => {
    if (reserva.token) navigator.sendBeacon(`/api/reserva/${reserva.token}/cancelar`);
});


// =============================================================================
//  PASO 1 - Datos del cliente  (POST /api/clientes)
// =============================================================================
function validarPaso1() {
    const nombre = $("nombre").value.trim();
    const telefono = $("telefono").value.replace(/[\s-]/g, "");
    let valido = true;

    // Se valida aquí para avisar rápido, pero el backend vuelve a validar.
    if (nombre.length < 3) {
        $("errorNombre").textContent = "Escribe tu nombre completo.";
        $("nombre").classList.add("invalido");
        valido = false;
    } else {
        $("errorNombre").textContent = "";
        $("nombre").classList.remove("invalido");
    }

    if (!/^\d{8}$/.test(telefono)) {
        $("errorTelefono").textContent = "Ingresa un número de 8 dígitos.";
        $("telefono").classList.add("invalido");
        valido = false;
    } else {
        $("errorTelefono").textContent = "";
        $("telefono").classList.remove("invalido");
    }

    return valido ? { nombre, telefono } : null;
}

$("formCliente").addEventListener("submit", async (evento) => {
    evento.preventDefault();
    const datos = validarPaso1();
    if (!datos) return;

    const boton = evento.submitter || evento.target.querySelector("[type=submit]");
    boton.disabled = true;
    try {
        // Si volvió con "Atrás" y no cambió nada, se reutiliza el mismo cliente
        // (así no se crea un cliente repetido en la base de datos).
        const mismosDatos = reserva.cliente_id &&
            reserva.nombre === datos.nombre && reserva.telefono === datos.telefono;

        if (!mismosDatos) {
            const r = await llamarApi("/api/clientes", "POST", datos);
            reserva.cliente_id = r.cliente_id;
            reserva.nombre = datos.nombre;
            reserva.telefono = datos.telefono;
        }
        await cargarCartelera();
        irAPaso(2);
    } catch (e) {
        mostrarError(e.message);
    } finally {
        boton.disabled = false;
    }
});


// =============================================================================
//  PASO 2 - Película y horario  (GET /api/cartelera)
// =============================================================================
async function cargarCartelera() {
    cartelera = await llamarApi("/api/cartelera");

    // Si la función que tenía elegida ya no está (otra película, otro día),
    // se borra la selección para no continuar con datos viejos.
    if (reserva.funcion) {
        const pelicula = cartelera.find((p) => p.id === reserva.pelicula.id);
        const sigue = pelicula && pelicula.horarios.some((h) => h.funcion_id === reserva.funcion.funcion_id);
        if (sigue) {
            reserva.pelicula = pelicula;
        } else {
            reserva.pelicula = null;
            reserva.funcion = null;
        }
    }
    dibujarCartelera();
}

function dibujarCartelera() {
    const lista = $("listaPeliculas");

    if (cartelera.length === 0) {
        lista.innerHTML = `<p class="vacio">No hay películas en exhibición hoy.</p>`;
        $("btnPaso2").disabled = true;
        return;
    }

    lista.innerHTML = cartelera.map((p) => {
        const elegida = reserva.pelicula && reserva.pelicula.id === p.id;
        const horarios = p.horarios.map((h) => {
            const activo = reserva.funcion && reserva.funcion.funcion_id === h.funcion_id;
            return `<button type="button" class="horario ${activo ? "elegido" : ""}"
                        data-pelicula="${p.id}" data-funcion="${h.funcion_id}">
                        ${formatoHora(h.hora)}
                    </button>`;
        }).join("");

        return `
            <article class="pelicula ${elegida ? "elegida" : ""}">
                ${p.imagen
                    ? `<img class="poster" src="${escaparHtml(p.imagen)}" alt="Póster de ${escaparHtml(p.nombre)}">`
                    : `<div class="poster">${escaparHtml(p.nombre.charAt(0).toUpperCase())}</div>`}
                <div>
                    <h3>${escaparHtml(p.nombre)}</h3>
                    <p class="gris">${escaparHtml(p.sala)} · ${formatoColones(p.precio)} · hasta ${formatoFecha(p.fecha_fin)}</p>
                    <div class="horarios">${horarios}</div>
                </div>
            </article>`;
    }).join("");

    $("btnPaso2").disabled = !reserva.funcion;
}

// Un solo listener para todos los botones de horario (delegación de eventos).
$("listaPeliculas").addEventListener("click", (evento) => {
    const boton = evento.target.closest(".horario");
    if (!boton) return;

    const pelicula = cartelera.find((p) => p.id === Number(boton.dataset.pelicula));
    reserva.pelicula = pelicula;
    reserva.funcion = pelicula.horarios.find((h) => h.funcion_id === Number(boton.dataset.funcion));
    dibujarCartelera();
});

$("btnPaso2").addEventListener("click", async () => {
    if (!reserva.funcion) return;
    const boton = $("btnPaso2");
    boton.disabled = true;   // evita doble clic mientras responde el backend
    try {
        await abrirPaso3();
    } catch (e) {
        mostrarError(e.message);
    } finally {
        boton.disabled = !reserva.funcion;
    }
});


// =============================================================================
//  PASO 3 - Cantidad de butacas
// =============================================================================
function maximoPermitido() {
    return Math.min(MAX_POR_RESERVA, reserva.disponibles);
}

// Consulta cuántas butacas quedan HOY en la función y la ajusta al contador.
// (GET /api/funciones/{id}/disponibles)
async function actualizarDisponibles() {
    const r = await llamarApi(`/api/funciones/${reserva.funcion.funcion_id}/disponibles`);
    reserva.disponibles = r.disponibles;
    // La cantidad queda entre 1 y el máximo permitido
    reserva.cantidad = Math.min(Math.max(reserva.cantidad, 1), Math.max(maximoPermitido(), 1));
    prepararPaso3();
}

// Entrada al paso 3 (desde el paso 2, desde "Cambiar cantidad" o cuando la
// reservación temporal expiró): siempre con el número de disponibles al día.
async function abrirPaso3() {
    await actualizarDisponibles();
    irAPaso(3);
}

function prepararPaso3() {
    $("detalleFuncion3").textContent = `${reserva.pelicula.nombre} · ${textoFuncion()}`;

    if (reserva.disponibles === 0) {
        $("textoDisponibles").textContent = "Esta función ya no tiene butacas disponibles. Elige otro horario.";
    } else {
        $("textoDisponibles").textContent =
            `Quedan ${reserva.disponibles} butacas disponibles en esta función.` +
            (reserva.disponibles > MAX_POR_RESERVA ? ` Máximo ${MAX_POR_RESERVA} por reservación.` : "");
    }
    actualizarContador();
}

function actualizarContador() {
    $("cantidad").textContent = reserva.cantidad;
    $("btnMenos").disabled = reserva.cantidad <= 1;
    $("btnMas").disabled = reserva.cantidad >= maximoPermitido();
    $("btnPaso3").disabled = reserva.disponibles === 0;
}

function cambiarCantidad(delta) {
    const nueva = reserva.cantidad + delta;
    if (nueva < 1 || nueva > maximoPermitido()) return;
    reserva.cantidad = nueva;
    actualizarContador();
}

$("btnMenos").addEventListener("click", () => cambiarCantidad(-1));
$("btnMas").addEventListener("click", () => cambiarCantidad(1));

// Atajos de teclado en el paso 3: + / − o flechas arriba / abajo
document.addEventListener("keydown", (e) => {
    if (pasoActual !== 3) return;
    if (e.key === "+" || e.key === "ArrowUp")   { e.preventDefault(); cambiarCantidad(1); }
    if (e.key === "-" || e.key === "ArrowDown") { e.preventDefault(); cambiarCantidad(-1); }
});

// Pide al backend que cargue la matriz en memoria  (POST /api/reserva/iniciar)
$("btnPaso3").addEventListener("click", async () => {
    cancelarReservaTemporal();   // si venía de "Cambiar cantidad", se suelta la anterior

    $("btnPaso3").disabled = true;
    try {
        const r = await llamarApi("/api/reserva/iniciar", "POST", {
            cliente_id: reserva.cliente_id,
            funcion_id: reserva.funcion.funcion_id,
            cantidad: reserva.cantidad
        });
        reserva.token = r.token;
        reserva.matriz = r.matriz;
        reserva.seleccionadas = 0;
        reserva.butacas = [];

        dibujarMapa();
        actualizarResumen();
        irAPaso(4);
    } catch (e) {
        mostrarError(e.message);
        // Puede que otra persona haya reservado mientras tanto:
        // se vuelve a consultar para que el contador muestre lo real.
        await actualizarDisponibles().catch(() => {});
    } finally {
        $("btnPaso3").disabled = reserva.disponibles === 0;
    }
});


// =============================================================================
//  PASO 4 - Mapa de butacas  (POST /api/reserva/{token}/marcar)
// =============================================================================

// Cuántas columnas tiene cada bloque en pantalla (solo es diseño:
// en la matriz cada agrupación es una fila con todas sus butacas).
function columnasDelBloque(butacas) {
    if (butacas <= 10) return butacas;
    if (butacas <= 48) return 6;
    return 10;
}

// Dibuja un bloque por cada fila de la matriz (cada agrupación).
function dibujarMapa() {
    const mapa = $("mapa");
    mapa.innerHTML = "";

    reserva.matriz.forEach((fila, agrupacion) => {
        const contenedor = document.createElement("div");
        contenedor.innerHTML = `<p class="bloque-titulo">AGRUPACIÓN ${String.fromCharCode(65 + agrupacion)}</p>`;

        const bloque = document.createElement("div");
        bloque.className = "bloque";
        bloque.style.setProperty("--columnas", columnasDelBloque(fila.length));

        fila.forEach((estado, numero) => {
            const boton = document.createElement("button");
            boton.type = "button";
            boton.dataset.agrupacion = agrupacion;
            boton.dataset.numero = numero;
            pintarButaca(boton, estado);
            bloque.appendChild(boton);
        });

        contenedor.appendChild(bloque);
        mapa.appendChild(contenedor);
    });
}

function pintarButaca(boton, estado) {
    const nombre = etiquetaButaca(Number(boton.dataset.agrupacion), Number(boton.dataset.numero));
    const descripcion = ["disponible", "ocupada", "fuera de servicio", "seleccionada"][estado];

    boton.className = "butaca estado-" + estado;
    boton.textContent = estado === SELECCIONADA ? "✓" : estado === OCUPADA ? "×" : "";
    boton.title = `${nombre} · ${descripcion}`;
    boton.setAttribute("aria-label", boton.title);
    boton.disabled = estado === OCUPADA || estado === DANADA;
}

// Clic en una butaca: se le pide al backend que cambie la matriz.
$("mapa").addEventListener("click", async (evento) => {
    const boton = evento.target.closest(".butaca");
    if (!boton || boton.disabled) return;

    const agrupacion = Number(boton.dataset.agrupacion);
    const numero = Number(boton.dataset.numero);

    // Aviso sin ir al servidor: ya eligió todas y quiere marcar otra.
    if (reserva.matriz[agrupacion][numero] === DISPONIBLE &&
        reserva.seleccionadas >= reserva.cantidad) {
        mostrarError(`Ya elegiste ${reserva.cantidad}. Desmarca una butaca amarilla para cambiarla.`);
        return;
    }

    $("mapa").classList.add("ocupado");   // evita dobles clics mientras responde
    try {
        const r = await llamarApi(`/api/reserva/${reserva.token}/marcar`, "POST", { agrupacion, numero });

        // El backend ya cambió su matriz; aquí se copia el resultado.
        reserva.matriz[agrupacion][numero] = r.estado;
        reserva.seleccionadas = r.seleccionadas;
        reserva.butacas = r.butacas;
        pintarButaca(boton, r.estado);
        actualizarResumen();
    } catch (e) {
        if (e.estado === 404) volverAlInicioDeButacas(e.message);
        else mostrarError(e.message);
    } finally {
        $("mapa").classList.remove("ocupado");
    }
});

function actualizarResumen() {
    const faltan = reserva.cantidad - reserva.seleccionadas;

    $("resumenPelicula").textContent = reserva.pelicula.nombre;
    $("resumenFuncion").textContent = `${reserva.pelicula.sala} · Hoy ${formatoHora(reserva.funcion.hora)}`;
    $("resumenSeleccionadas").textContent = reserva.seleccionadas;
    $("resumenCantidad").textContent = reserva.cantidad;
    $("resumenButacas").textContent = reserva.butacas.length ? reserva.butacas.join(" · ") : "—";
    $("resumenTotal").textContent = formatoColones(reserva.pelicula.precio * reserva.cantidad);

    const aviso = $("resumenAviso");
    if (faltan > 0) {
        aviso.textContent = `Te falta${faltan === 1 ? "" : "n"} ${faltan} butaca${faltan === 1 ? "" : "s"} por elegir.`;
        aviso.classList.remove("listo");
    } else {
        aviso.textContent = "¡Listo! Ya puedes continuar al pago.";
        aviso.classList.add("listo");
    }

    // Solo se puede seguir si la cantidad elegida coincide con la pedida.
    $("btnPaso4").disabled = faltan !== 0;
}

// La reservación temporal expiró: se vuelve a cargar desde el paso 3.
// El mensaje se muestra DESPUÉS de cambiar de paso (irAPaso oculta el anterior).
async function volverAlInicioDeButacas(mensaje) {
    reserva.token = null;
    try {
        await abrirPaso3();
        if (mensaje) mostrarError(mensaje);
    } catch (e) {
        irAPaso(3);
        mostrarError(e.message);
    }
}

$("btnCambiarCantidad").addEventListener("click", async () => {
    cancelarReservaTemporal();
    try {
        await abrirPaso3();
    } catch (e) {
        mostrarError(e.message);
        irAPaso(3);
    }
});

$("btnPaso4").addEventListener("click", () => {
    $("tqCliente").textContent = reserva.nombre;
    $("tqTelefono").textContent = reserva.telefono.replace(/(\d{4})(\d{4})/, "$1 $2");
    $("tqPelicula").textContent = reserva.pelicula.nombre;
    $("tqFuncion").textContent = textoFuncion();
    $("tqButacas").textContent = reserva.butacas.join(" · ");
    $("tqCantidad").textContent = `${reserva.cantidad} × ${formatoColones(reserva.pelicula.precio)}`;
    $("tqTotal").textContent = formatoColones(reserva.pelicula.precio * reserva.cantidad);
    irAPaso(5);
});


// =============================================================================
//  PASO 5 - Confirmar y simular pago  (POST /api/reserva/{token}/confirmar)
// =============================================================================
$("btnPagar").addEventListener("click", async () => {
    $("btnPagar").disabled = true;
    try {
        const r = await llamarApi(`/api/reserva/${reserva.token}/confirmar`, "POST");
        reserva.token = null;   // el backend ya borró la matriz temporal
        mostrarExito(r);
    } catch (e) {
        if (e.estado === 409 && e.datos.matriz) {
            // Otra persona ganó una butaca: el backend manda la matriz al día.
            reserva.matriz = e.datos.matriz;
            reserva.seleccionadas = e.datos.seleccionadas;
            reserva.butacas = e.datos.butacas;
            dibujarMapa();
            actualizarResumen();
            irAPaso(4);
            mostrarError(e.message);   // después de cambiar de paso, para que se vea
        } else if (e.estado === 404) {
            volverAlInicioDeButacas(e.message);
        } else {
            mostrarError(e.message);
        }
    } finally {
        // Si ya se pagó (token borrado), el botón queda bloqueado para no pagar dos veces
        $("btnPagar").disabled = reserva.token === null;
    }
});

function mostrarExito(r) {
    const butacas = r.butacas.length > 1
        ? r.butacas.slice(0, -1).join(", ") + " y " + r.butacas.at(-1)
        : r.butacas[0];

    const inicio = r.butacas.length > 1
        ? `Tus butacas ${butacas} quedaron reservadas`
        : `Tu butaca ${butacas} quedó reservada`;

    $("textoExito").textContent =
        `${inicio} para ${r.pelicula}, ` +
        `hoy a las ${formatoHora(r.hora)} en ${r.sala}. Total pagado: ${formatoColones(r.total)}.`;

    $("bloqueConfirmar").hidden = true;
    $("bloqueExito").hidden = false;
    $("btnCancelar").hidden = true;
    document.querySelectorAll("#barraPasos li").forEach((li) => li.classList.add("hecho"));

    // El examen pide regresar a la página principal después de confirmar.
    let segundos = SEGUNDOS_REGRESO;
    const actualizar = () => ($("textoRegreso").textContent = `Volviendo a la cartelera en ${segundos} s…`);
    actualizar();
    const intervalo = setInterval(() => {
        segundos--;
        actualizar();
        if (segundos <= 0) {
            clearInterval(intervalo);
            window.location.href = "cartelera.html";
        }
    }, 1000);
}


// Arranca en el paso 1
irAPaso(1);

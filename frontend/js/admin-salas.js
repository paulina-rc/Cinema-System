// =============================================================================
//  admin-salas.js - Administración de salas y butacas
//  -----------------------------------------------------------------------------
//  Rutas que usa (ver docs/rutas-api.md):
//    GET  /api/salas                    -> lista de salas
//    POST /api/salas                    -> crea la sala y el backend genera las butacas
//    PUT  /api/salas/{id}/habilitada    -> habilita / inhabilita una sala
//    GET  /api/salas/{id}/butacas       -> mapa de la sala
//    PUT  /api/butacas/{id}/habilitada  -> marca una butaca fuera de servicio o la repara
//
//  Igual que en la reservación, el JS no decide nada por su cuenta: primero
//  se le pregunta al backend y solo si responde bien se cambia la pantalla.
// =============================================================================

// Mismos límites que main.cpp (MAX_AGRUPACIONES y MAX_BUTACAS)
const MAX_AGRUPACIONES = 10;
const MAX_BUTACAS = 60;

let salas = [];             // lo que vino de GET /api/salas
let salaSeleccionada = null; // id de la sala cuyo mapa se está mostrando

const $ = (id) => document.getElementById(id);


// =============================================================================
//  Ayudantes
// =============================================================================

// Llama a la API. Si el backend responde con error, lanza un Error con su mensaje.
async function llamarApi(ruta, metodo = "GET", cuerpo = null) {
    // El token de la sesión (admin-sesion.js) va en cada petición
    const opciones = { method: metodo, headers: { Authorization: "Bearer " + tokenAdmin() } };
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

    // Sesión vencida o cerrada en otro lado: volver al login
    if (respuesta.status === 401) {
        irAlLogin();
        throw new Error("Tu sesión venció. Inicia sesión de nuevo.");
    }

    const datos = await respuesta.json().catch(() => ({}));
    if (!respuesta.ok) {
        throw new Error(datos.error || "Ocurrió un error (" + respuesta.status + ")");
    }
    return datos;
}

// Mensaje flotante (desaparece solo). tipo: "error" o "exito"
let temporizadorToast;
function mostrarMensaje(mensaje, tipo = "error") {
    const toast = $("toast");
    toast.textContent = mensaje;
    toast.className = "toast " + tipo;
    toast.hidden = false;
    clearTimeout(temporizadorToast);
    temporizadorToast = setTimeout(() => (toast.hidden = true), 4000);
}

function escaparHtml(texto) {
    const div = document.createElement("div");
    div.textContent = texto;
    return div.innerHTML;
}

// (0, 4) -> "A5": letra de la agrupación + número desde 1
function etiquetaButaca(agrupacion, numero) {
    return String.fromCharCode(65 + agrupacion) + (numero + 1);
}

// Cuántas columnas tiene cada bloque en pantalla (solo es diseño,
// igual que en reserva.js)
function columnasDelBloque(butacas) {
    if (butacas <= 10) return butacas;
    if (butacas <= 48) return 6;
    return 10;
}


// =============================================================================
//  Lista de salas
// =============================================================================

async function cargarSalas() {
    try {
        salas = await llamarApi("/api/salas");
    } catch (e) {
        $("listaSalas").innerHTML = `<p class="vacio">${escaparHtml(e.message)}</p>`;
        return;
    }

    // Si no hay ninguna seleccionada, se muestra la primera
    if (salaSeleccionada === null && salas.length > 0) {
        salaSeleccionada = salas[0].id;
    }

    dibujarSalas();
    if (salaSeleccionada !== null) cargarMapa(salaSeleccionada);
}

function dibujarSalas() {
    const lista = $("listaSalas");
    lista.innerHTML = "";

    if (salas.length === 0) {
        lista.innerHTML = `<p class="vacio">Todavía no hay salas. Crea la primera con el formulario.</p>`;
        $("panelMapa").hidden = true;
        return;
    }

    salas.forEach((sala) => {
        const activa = sala.habilitada === 1;

        const tarjeta = document.createElement("article");
        tarjeta.className = "tarjeta sala"
            + (sala.id === salaSeleccionada ? " seleccionada" : "")
            + (activa ? "" : " inhabilitada");
        tarjeta.tabIndex = 0;
        tarjeta.innerHTML = `
            <div class="sala-info">
                <p class="sala-nombre">${escaparHtml(sala.nombre)}</p>
                <p class="sala-detalle">${sala.agrupaciones} agrupaciones × ${sala.butacas_por_agrupacion} butacas</p>
            </div>
            <span class="estado ${activa ? "activa" : "inactiva"}">${activa ? "Activa" : "Inhabilitada"}</span>
            <button type="button" class="btn-secundario">${activa ? "Inhabilitar" : "Habilitar"}</button>
        `;

        // Clic en la tarjeta: ver su mapa
        tarjeta.addEventListener("click", () => seleccionarSala(sala.id));
        tarjeta.addEventListener("keydown", (e) => {
            if (e.target === tarjeta && (e.key === "Enter" || e.key === " ")) {
                e.preventDefault();
                seleccionarSala(sala.id);
            }
        });

        // Clic en el botón: habilitar / inhabilitar (sin cambiar la selección)
        const boton = tarjeta.querySelector("button");
        boton.addEventListener("click", (e) => {
            e.stopPropagation();
            cambiarEstadoSala(sala, boton);
        });

        lista.appendChild(tarjeta);
    });
}

function seleccionarSala(id) {
    if (id === salaSeleccionada) return;
    salaSeleccionada = id;
    dibujarSalas();
    cargarMapa(id);
}

async function cambiarEstadoSala(sala, boton) {
    const nuevo = sala.habilitada === 1 ? 0 : 1;
    boton.disabled = true;
    try {
        await llamarApi(`/api/salas/${sala.id}/habilitada`, "PUT", { habilitada: nuevo });
        sala.habilitada = nuevo;
        dibujarSalas();
        mostrarMensaje(`${sala.nombre} ${nuevo ? "habilitada" : "inhabilitada"}`, "exito");
    } catch (e) {
        boton.disabled = false;
        mostrarMensaje(e.message);
    }
}


// =============================================================================
//  Formulario: nueva sala
// =============================================================================

// Devuelve el mensaje de error o "" si todo está bien
function validarFormulario(nombre, agrupaciones, porAgrupacion) {
    if (nombre === "") return { campo: "nombreSala", mensaje: "Escribe el nombre de la sala." };
    if (salas.some((s) => s.nombre.toLowerCase() === nombre.toLowerCase()))
        return { campo: "nombreSala", mensaje: "Ya existe una sala llamada " + nombre + "." };

    if (!Number.isInteger(agrupaciones) || agrupaciones < 1 || agrupaciones > MAX_AGRUPACIONES)
        return { campo: "agrupaciones", mensaje: `Las agrupaciones deben estar entre 1 y ${MAX_AGRUPACIONES}.` };

    if (!Number.isInteger(porAgrupacion) || porAgrupacion < 1 || porAgrupacion > MAX_BUTACAS)
        return { campo: "porAgrupacion", mensaje: `Las butacas por agrupación deben estar entre 1 y ${MAX_BUTACAS}.` };

    return null;
}

function mostrarErrorForm(error) {
    ["nombreSala", "agrupaciones", "porAgrupacion"].forEach((id) => $(id).classList.remove("invalido"));
    $("errorForm").hidden = !error;
    if (!error) return;

    $("errorForm").textContent = error.mensaje;
    if (error.campo) {
        $(error.campo).classList.add("invalido");
        $(error.campo).focus();
    }
}

async function crearSala(evento) {
    evento.preventDefault();

    const nombre = $("nombreSala").value.trim();
    const agrupaciones = Number($("agrupaciones").value);
    const porAgrupacion = Number($("porAgrupacion").value);

    const error = validarFormulario(nombre, agrupaciones, porAgrupacion);
    mostrarErrorForm(error);
    if (error) return;

    const boton = $("btnCrear");
    boton.disabled = true;
    boton.textContent = "Creando…";

    try {
        const r = await llamarApi("/api/salas", "POST", {
            nombre,
            agrupaciones,
            butacas_por_agrupacion: porAgrupacion
        });
        $("formSala").reset();
        mostrarMensaje(`${nombre} creada con ${agrupaciones * porAgrupacion} butacas`, "exito");

        salaSeleccionada = r.id;   // mostrar el mapa de la sala nueva
        await cargarSalas();
    } catch (e) {
        mostrarErrorForm({ campo: null, mensaje: e.message });
    } finally {
        boton.disabled = false;
        boton.textContent = "Crear sala y generar butacas";
    }
}


// =============================================================================
//  Mapa de butacas
// =============================================================================

async function cargarMapa(id) {
    const sala = salas.find((s) => s.id === id);
    if (!sala) return;

    $("panelMapa").hidden = false;
    $("tituloMapa").textContent = "Mapa de " + sala.nombre;
    $("mapa").innerHTML = `<p class="vacio">Cargando butacas…</p>`;

    let datos;
    try {
        datos = await llamarApi(`/api/salas/${id}/butacas`);
    } catch (e) {
        $("mapa").innerHTML = `<p class="vacio">${escaparHtml(e.message)}</p>`;
        return;
    }

    // Si mientras cargaba se eligió otra sala, no se dibuja esta
    if (id !== salaSeleccionada) return;
    dibujarMapa(datos);
}

// Arma matriz[agrupacion][numero] con las butacas que mandó el backend
// y dibuja un bloque por agrupación.
function dibujarMapa(datos) {
    const matriz = [];
    for (let a = 0; a < datos.agrupaciones; a++) {
        matriz.push(new Array(datos.butacas_por_agrupacion).fill(null));
    }
    datos.butacas.forEach((b) => {
        if (matriz[b.agrupacion]) matriz[b.agrupacion][b.numero] = b;
    });

    const mapa = $("mapa");
    mapa.innerHTML = "";

    matriz.forEach((fila, agrupacion) => {
        const contenedor = document.createElement("div");
        contenedor.innerHTML = `<p class="bloque-titulo">AGRUPACIÓN ${String.fromCharCode(65 + agrupacion)}</p>`;

        const bloque = document.createElement("div");
        bloque.className = "bloque";
        bloque.style.setProperty("--columnas", columnasDelBloque(fila.length));

        fila.forEach((butaca, numero) => {
            const boton = document.createElement("button");
            boton.type = "button";
            boton.className = "butaca";
            boton.textContent = etiquetaButaca(agrupacion, numero);

            if (butaca) {
                pintarButaca(boton, butaca);
                boton.addEventListener("click", () => cambiarEstadoButaca(butaca, boton));
            } else {
                boton.disabled = true;   // no existe en la base de datos
            }
            bloque.appendChild(boton);
        });

        contenedor.appendChild(bloque);
        mapa.appendChild(contenedor);
    });
}

function pintarButaca(boton, butaca) {
    const fuera = butaca.habilitada === 0;
    const nombre = etiquetaButaca(butaca.agrupacion, butaca.numero);
    boton.classList.toggle("fuera", fuera);
    boton.title = nombre + (fuera ? " · fuera de servicio (toca para habilitar)" : " · habilitada");
    boton.setAttribute("aria-label", boton.title);
}

async function cambiarEstadoButaca(butaca, boton) {
    const nuevo = butaca.habilitada === 1 ? 0 : 1;
    const mapa = $("mapa");
    mapa.classList.add("ocupado");   // evita clics dobles mientras responde

    try {
        await llamarApi(`/api/butacas/${butaca.id}/habilitada`, "PUT", { habilitada: nuevo });
        butaca.habilitada = nuevo;
        pintarButaca(boton, butaca);
    } catch (e) {
        mostrarMensaje(e.message);
    } finally {
        mapa.classList.remove("ocupado");
    }
}


// =============================================================================
//  Inicio
// =============================================================================
$("formSala").addEventListener("submit", crearSala);
cargarSalas();

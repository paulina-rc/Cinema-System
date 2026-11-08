// =============================================================================
//  admin-peliculas.js - Registro de películas (con póster)
//  -----------------------------------------------------------------------------
//  Rutas que usa (ver docs/rutas-api.md):
//    GET  /api/salas      -> llena la lista de salas (solo las habilitadas)
//    GET  /api/peliculas  -> tabla "Películas registradas"
//    POST /api/peliculas  -> guarda la película, sus horarios y su póster
//
//  El póster: el navegador lo achica (máx. 600 x 900) y lo convierte a texto
//  base64 ("data URL") para mandarlo dentro del JSON. El backend lo guarda
//  como archivo en frontend/img/peliculas/ y en la BD queda solo la ruta.
//  Las películas que están en fechas de exhibición salen solas en la cartelera.
// =============================================================================

const POSTER_ANCHO = 600;
const POSTER_ALTO = 900;
const MAX_MB_ORIGINAL = 10;   // tamaño máximo del archivo que elige el usuario

let horarios = [];      // ["15:15", "19:30"] (formato HH:MM de 24 horas)
let imagenDataUrl = ""; // póster ya achicado, listo para enviar ("" = sin póster)

const $ = (id) => document.getElementById(id);


// =============================================================================
//  Ayudantes
// =============================================================================

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

// "19:30" -> "7:30 p.m."
function formatoHora(hora) {
    let [h, m] = hora.split(":").map(Number);
    const sufijo = h >= 12 ? "p.m." : "a.m.";
    h = h % 12 || 12;
    return `${h}:${String(m).padStart(2, "0")} ${sufijo}`;
}

// 3500 -> "₡3 500"
function formatoColones(monto) {
    return "₡" + Number(monto).toLocaleString("es-CR", { maximumFractionDigits: 2 });
}

// "2026-11-07" -> "07/11/2026"
function formatoFecha(fecha) {
    const [a, m, d] = fecha.split("-");
    return `${d}/${m}/${a}`;
}

// Fecha de hoy en formato AAAA-MM-DD (hora local)
function hoy() {
    const f = new Date();
    return `${f.getFullYear()}-${String(f.getMonth() + 1).padStart(2, "0")}-${String(f.getDate()).padStart(2, "0")}`;
}


// =============================================================================
//  Errores del formulario (mensaje rojo debajo de cada campo)
// =============================================================================

function ponerError(campo, idError, mensaje) {
    $(idError).textContent = mensaje;
    if (campo) campo.classList.toggle("invalido", mensaje !== "");
}

function limpiarErrores() {
    ponerError($("nombre"), "errorNombre", "");
    ponerError($("fechaInicio"), "errorFechaInicio", "");
    ponerError($("fechaFin"), "errorFechaFin", "");
    ponerError($("precio"), "errorPrecio", "");
    ponerError($("sala"), "errorSala", "");
    ponerError(document.querySelector(".horarios-editor"), "errorHorarios", "");
    ponerError($("zonaPoster"), "errorImagen", "");
}

// Revisa la fecha final cada vez que cambia alguna de las dos fechas
function revisarFechas() {
    const inicio = $("fechaInicio").value;
    const fin = $("fechaFin").value;
    if (inicio && fin && fin < inicio) {
        ponerError($("fechaFin"), "errorFechaFin", "No puede ser antes de la fecha de inicio.");
        return false;
    }
    ponerError($("fechaFin"), "errorFechaFin", "");
    return true;
}


// =============================================================================
//  Salas (lista desplegable)
// =============================================================================

async function cargarSalas() {
    const select = $("sala");
    try {
        const salas = await llamarApi("/api/salas");
        const habilitadas = salas.filter((s) => s.habilitada === 1);

        if (habilitadas.length === 0) {
            select.innerHTML = `<option value="">No hay salas habilitadas</option>`;
            return;
        }
        select.innerHTML = habilitadas
            .map((s) => `<option value="${s.id}">${escaparHtml(s.nombre)}</option>`)
            .join("");
    } catch (e) {
        select.innerHTML = `<option value="">No se pudieron cargar</option>`;
        mostrarMensaje(e.message);
    }
}


// =============================================================================
//  Horarios (chips)
// =============================================================================

function dibujarHorarios() {
    const contenedor = $("chipsHorarios");
    contenedor.innerHTML = "";

    horarios.forEach((hora) => {
        const chip = document.createElement("span");
        chip.className = "chip";
        chip.innerHTML = `${formatoHora(hora)} <button type="button" aria-label="Quitar ${formatoHora(hora)}">×</button>`;
        chip.querySelector("button").addEventListener("click", () => {
            horarios = horarios.filter((h) => h !== hora);
            dibujarHorarios();
        });
        contenedor.appendChild(chip);
    });
}

function agregarHorario() {
    const hora = $("nuevaHora").value;   // "HH:MM" o "" si está vacío
    const editor = document.querySelector(".horarios-editor");

    if (!hora) {
        ponerError(editor, "errorHorarios", "Elige una hora antes de agregarla.");
        return;
    }
    if (horarios.includes(hora)) {
        ponerError(editor, "errorHorarios", `El horario ${formatoHora(hora)} ya está agregado.`);
        return;
    }

    horarios.push(hora);
    horarios.sort();   // "HH:MM" se ordena bien como texto
    $("nuevaHora").value = "";
    ponerError(editor, "errorHorarios", "");
    dibujarHorarios();
}


// =============================================================================
//  Póster
// =============================================================================

// Achica la imagen con un <canvas> y la devuelve como data URL JPEG
function achicarImagen(archivo) {
    return new Promise((resolver, rechazar) => {
        const url = URL.createObjectURL(archivo);
        const img = new Image();

        img.onload = () => {
            const escala = Math.min(1, POSTER_ANCHO / img.width, POSTER_ALTO / img.height);
            const canvas = document.createElement("canvas");
            canvas.width = Math.round(img.width * escala);
            canvas.height = Math.round(img.height * escala);

            const ctx = canvas.getContext("2d");
            ctx.fillStyle = "#000";                    // fondo para PNG transparentes
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            ctx.drawImage(img, 0, 0, canvas.width, canvas.height);

            URL.revokeObjectURL(url);
            resolver(canvas.toDataURL("image/jpeg", 0.85));
        };
        img.onerror = () => {
            URL.revokeObjectURL(url);
            rechazar(new Error("No se pudo leer la imagen."));
        };
        img.src = url;
    });
}

async function elegirImagen(archivo) {
    if (!archivo) return;

    if (!["image/jpeg", "image/png", "image/webp"].includes(archivo.type)) {
        ponerError($("zonaPoster"), "errorImagen", "Solo JPG, PNG o WEBP.");
        return;
    }
    if (archivo.size > MAX_MB_ORIGINAL * 1024 * 1024) {
        ponerError($("zonaPoster"), "errorImagen", `Máximo ${MAX_MB_ORIGINAL} MB.`);
        return;
    }

    try {
        imagenDataUrl = await achicarImagen(archivo);
    } catch (e) {
        ponerError($("zonaPoster"), "errorImagen", e.message);
        return;
    }

    ponerError($("zonaPoster"), "errorImagen", "");
    $("vistaPoster").src = imagenDataUrl;
    $("vistaPoster").hidden = false;
    $("textoPoster").hidden = true;
    $("btnQuitarImagen").hidden = false;
}

function quitarImagen() {
    imagenDataUrl = "";
    $("imagen").value = "";
    $("vistaPoster").removeAttribute("src");
    $("vistaPoster").hidden = true;
    $("textoPoster").hidden = false;
    $("btnQuitarImagen").hidden = true;
}


// =============================================================================
//  Guardar película
// =============================================================================

// Revisa todo el formulario. Devuelve los datos listos para enviar o null.
function leerFormulario() {
    limpiarErrores();
    let valido = true;

    const nombre = $("nombre").value.trim();
    const fechaInicio = $("fechaInicio").value;
    const fechaFin = $("fechaFin").value;
    const precio = Number($("precio").value);
    const salaId = Number($("sala").value);

    if (nombre === "") {
        ponerError($("nombre"), "errorNombre", "Escribe el nombre de la película.");
        valido = false;
    }
    if (!fechaInicio) {
        ponerError($("fechaInicio"), "errorFechaInicio", "Elige la fecha de inicio.");
        valido = false;
    }
    if (!fechaFin) {
        ponerError($("fechaFin"), "errorFechaFin", "Elige la fecha final.");
        valido = false;
    } else if (!revisarFechas()) {
        valido = false;
    }
    if ($("precio").value === "" || !(precio > 0)) {
        ponerError($("precio"), "errorPrecio", "El precio debe ser mayor que 0.");
        valido = false;
    }
    if (!salaId) {
        ponerError($("sala"), "errorSala", "Elige una sala habilitada.");
        valido = false;
    }
    if (horarios.length === 0) {
        ponerError(document.querySelector(".horarios-editor"), "errorHorarios", "Agrega al menos un horario.");
        valido = false;
    }

    if (!valido) {
        const primero = document.querySelector(".form-pelicula .invalido");
        if (primero && primero.focus) primero.focus();
        return null;
    }

    return {
        nombre,
        fecha_inicio: fechaInicio,
        fecha_fin: fechaFin,
        precio,
        sala_id: salaId,
        horarios: [...horarios],
        imagen: imagenDataUrl
    };
}

async function guardarPelicula(evento) {
    evento.preventDefault();

    const datos = leerFormulario();
    if (!datos) return;

    const boton = $("btnGuardar");
    boton.disabled = true;
    boton.textContent = "Guardando…";

    try {
        await llamarApi("/api/peliculas", "POST", datos);

        const enCartelera = datos.fecha_inicio <= hoy() && hoy() <= datos.fecha_fin;
        mostrarMensaje(
            `"${datos.nombre}" guardada` + (enCartelera ? " y ya aparece en la cartelera" : ""),
            "exito"
        );

        // Limpiar el formulario (la sala se deja como estaba)
        const sala = $("sala").value;
        $("formPelicula").reset();
        $("sala").value = sala;
        horarios = [];
        dibujarHorarios();
        quitarImagen();

        cargarPeliculas();
    } catch (e) {
        mostrarMensaje(e.message);
    } finally {
        boton.disabled = false;
        boton.textContent = "Guardar película";
    }
}


// =============================================================================
//  Tabla de películas registradas
// =============================================================================

// En qué estado está la película respecto a la fecha de hoy
function etiquetaEstado(p) {
    const h = hoy();
    if (h < p.fecha_inicio) return `<span class="etiqueta-estado proxima">Próximamente</span>`;
    if (h > p.fecha_fin)    return `<span class="etiqueta-estado vencida">Finalizada</span>`;
    return `<span class="etiqueta-estado hoy">En cartelera</span>`;
}

async function cargarPeliculas() {
    const cuerpo = $("tablaPeliculas");
    let peliculas;
    try {
        peliculas = await llamarApi("/api/peliculas");
    } catch (e) {
        cuerpo.innerHTML = `<tr><td colspan="5" class="vacio">${escaparHtml(e.message)}</td></tr>`;
        return;
    }

    if (peliculas.length === 0) {
        cuerpo.innerHTML = `<tr><td colspan="5" class="vacio">Todavía no hay películas registradas.</td></tr>`;
        return;
    }

    cuerpo.innerHTML = peliculas.map((p) => {
        const poster = p.imagen
            ? `<img class="mini-poster" src="${escaparHtml(p.imagen)}" alt="">`
            : `<div class="mini-poster">${escaparHtml(p.nombre.charAt(0).toUpperCase())}</div>`;

        const horas = p.horarios.map((h) => formatoHora(h.hora)).join(" · ");

        return `
            <tr>
                <td>
                    <div class="celda-pelicula">
                        ${poster}
                        <div>
                            <div>${escaparHtml(p.nombre)}</div>
                            ${etiquetaEstado(p)}
                        </div>
                    </div>
                </td>
                <td class="gris">${formatoFecha(p.fecha_inicio)} – ${formatoFecha(p.fecha_fin)}</td>
                <td>${horas}</td>
                <td>${formatoColones(p.precio)}</td>
                <td>${escaparHtml(p.sala)}</td>
            </tr>`;
    }).join("");
}


// =============================================================================
//  Inicio
// =============================================================================

$("formPelicula").addEventListener("submit", guardarPelicula);
$("fechaInicio").addEventListener("change", revisarFechas);
$("fechaFin").addEventListener("change", revisarFechas);

$("btnAgregarHora").addEventListener("click", agregarHorario);
$("nuevaHora").addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
        e.preventDefault();   // que Enter agregue el horario y no envíe el formulario
        agregarHorario();
    }
});

$("imagen").addEventListener("change", (e) => elegirImagen(e.target.files[0]));
$("btnQuitarImagen").addEventListener("click", quitarImagen);

// Arrastrar y soltar una imagen sobre el recuadro del póster
const zona = $("zonaPoster");
zona.addEventListener("dragover", (e) => { e.preventDefault(); zona.classList.add("arrastrando"); });
zona.addEventListener("dragleave", () => zona.classList.remove("arrastrando"));
zona.addEventListener("drop", (e) => {
    e.preventDefault();
    zona.classList.remove("arrastrando");
    elegirImagen(e.dataTransfer.files[0]);
});

cargarSalas();
cargarPeliculas();

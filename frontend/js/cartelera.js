// =============================================================================
//  cartelera.js - Página pública de la cartelera
//  -----------------------------------------------------------------------------
//  Pide al backend las películas que se exhiben HOY (GET /api/cartelera) y
//  arma el carrusel y las tarjetas. Así, cada película que se registra en
//  admin-peliculas.html aparece aquí sola mientras esté en sus fechas.
// =============================================================================

let slideActual = 0;   // Guarda la imagen seleccionada
let totalSlides = 0;
let temporizadorCarrusel;

// Películas que vinieron de la API: el modal las busca aquí por id
// para no volver a llamar al backend.
let peliculasCartelera = [];

const carrusel = document.getElementById("carruselImagenes");
const modal = document.getElementById("modalPelicula");

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

function formatoColones(monto) {
    return "₡" + Number(monto).toLocaleString("es-CR", { maximumFractionDigits: 2 });
}

const MESES = ["ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"];

// "2026-11-07" -> { dia: 7, mes: "nov", anio: 2026 }
// Se parte a mano: new Date("2026-11-07") lo toma como UTC y en
// Costa Rica mostraría el día anterior.
function partesFecha(fecha) {
    const [anio, mes, dia] = String(fecha).slice(0, 10).split("-").map(Number);
    return { dia, mes: MESES[mes - 1], anio };
}

// -> "del 7 nov al 31 dic 2026" (el año del inicio solo si es distinto)
function formatoPeriodo(inicio, fin) {
    const a = partesFecha(inicio);
    const b = partesFecha(fin);
    const anioInicio = a.anio !== b.anio ? ` ${a.anio}` : "";
    return `del ${a.dia} ${a.mes}${anioInicio} al ${b.dia} ${b.mes} ${b.anio}`;
}

function chipsHorarios(horarios) {
    return horarios.map((h) => `<span>${formatoHora(h.hora)}</span>`).join("");
}


// =============================================================================
//  Carrusel
// =============================================================================

// Cambia la imagen del carrusel
function mostrarSlide(numero) {
    if (totalSlides === 0) return;
    slideActual = (numero + totalSlides) % totalSlides;

    carrusel.style.transform = `translateX(-${slideActual * 100}%)`;

    // Actualiza el punto amarillo
    document.querySelectorAll(".punto").forEach((punto, index) => {
        punto.classList.toggle("activo", index === slideActual);
    });
}

// Avanza o retrocede
function moverCarrusel(direccion) {
    mostrarSlide(slideActual + direccion);
    reiniciarAutoplay();
}

// Selecciona una imagen directamente
function irSlide(numero) {
    mostrarSlide(numero);
    reiniciarAutoplay();
}

// Cambia de imagen sola cada 6 segundos
function reiniciarAutoplay() {
    clearInterval(temporizadorCarrusel);
    if (totalSlides > 1) {
        temporizadorCarrusel = setInterval(() => mostrarSlide(slideActual + 1), 6000);
    }
}

function dibujarCarrusel(peliculas) {
    const conPoster = peliculas.filter((p) => p.imagen);
    totalSlides = conPoster.length;

    if (totalSlides === 0) {
        document.getElementById("carrusel").hidden = true;
        return;
    }

    // Fondo: el mismo póster desenfocado (llena el ancho sin importar la
    // proporción). Encima: el póster completo, sin recortes, y la info.
    carrusel.innerHTML = conPoster.map((p) => `
        <div class="slide">
            <img class="slide-fondo" src="${escaparHtml(p.imagen)}" alt="" aria-hidden="true">
            <div class="slide-contenido">
                <img class="slide-poster" src="${escaparHtml(p.imagen)}" alt="Póster de ${escaparHtml(p.nombre)}">
                <div class="slide-info">
                    <h2 class="slide-titulo">${escaparHtml(p.nombre)}</h2>
                    <p class="slide-detalle">${escaparHtml(p.sala)} · ${formatoColones(p.precio)}</p>
                    <div class="horarios">${chipsHorarios(p.horarios)}</div>
                    <button type="button" class="btn-detalles" onclick="abrirDetalle(${Number(p.id)})">Ver detalles</button>
                </div>
            </div>
        </div>`).join("");

    document.getElementById("indicadores").innerHTML = conPoster
        .map((_, i) => `<span class="punto${i === 0 ? " activo" : ""}" onclick="irSlide(${i})"></span>`)
        .join("");

    // Con una sola imagen no hacen falta flechas ni puntos
    document.querySelectorAll(".carrusel-btn, .indicadores").forEach((el) => {
        el.hidden = totalSlides < 2;
    });

    document.getElementById("carrusel").hidden = false;
    mostrarSlide(0);
    reiniciarAutoplay();
}


// =============================================================================
//  Tarjetas de películas
// =============================================================================

function dibujarTarjetas(peliculas) {
    const contenedor = document.getElementById("cartelera");

    if (peliculas.length === 0) {
        contenedor.innerHTML = `<p class="cartelera-vacia">No hay películas en exhibición hoy.</p>`;
        return;
    }

    // tabindex + role: la tarjeta se puede enfocar y abrir con el teclado
    contenedor.innerHTML = peliculas.map((p) => `
            <div class="pelicula" data-id="${Number(p.id)}" tabindex="0" role="button"
                 aria-label="Ver detalles de ${escaparHtml(p.nombre)}">
                ${htmlPoster(p)}
                <div class="pelicula-info">
                    <h3>${escaparHtml(p.nombre)}</h3>
                    <p class="pelicula-detalle">${escaparHtml(p.sala)} · ${formatoColones(p.precio)}</p>
                    <div class="horarios">${chipsHorarios(p.horarios)}</div>
                </div>
            </div>`).join("");
}

// Póster o, si no hay imagen, la inicial del nombre
function htmlPoster(p) {
    return p.imagen
        ? `<img src="${escaparHtml(p.imagen)}" alt="Póster de ${escaparHtml(p.nombre)}">`
        : `<div class="poster-reemplazo">${escaparHtml(p.nombre.charAt(0).toUpperCase())}</div>`;
}

// Un solo listener en el contenedor sirve para todas las tarjetas,
// aunque se vuelvan a dibujar.
const contenedorTarjetas = document.getElementById("cartelera");

contenedorTarjetas.addEventListener("click", (e) => {
    const tarjeta = e.target.closest(".pelicula");
    if (tarjeta) abrirDetalle(Number(tarjeta.dataset.id));
});

contenedorTarjetas.addEventListener("keydown", (e) => {
    const tarjeta = e.target.closest(".pelicula");
    if (tarjeta && e.key === "Enter") abrirDetalle(Number(tarjeta.dataset.id));
});


// =============================================================================
//  Pop-up de detalle
// =============================================================================

function abrirDetalle(id) {
    const p = peliculasCartelera.find((peli) => Number(peli.id) === id);
    if (!p) return;

    document.getElementById("modalContenido").innerHTML = `
        <div class="modal-poster">${htmlPoster(p)}</div>
        <div class="modal-info">
            <h2 class="modal-titulo">${escaparHtml(p.nombre)}</h2>
            <dl class="modal-datos">
                <dt>Sala</dt>
                <dd>${escaparHtml(p.sala)}</dd>
                <dt>Precio</dt>
                <dd>${formatoColones(p.precio)}</dd>
                <dt>En exhibición</dt>
                <dd>${formatoPeriodo(p.fecha_inicio, p.fecha_fin)}</dd>
            </dl>
            <p class="modal-subtitulo">Horarios</p>
            <div class="horarios">${chipsHorarios(p.horarios)}</div>
            <a href="reserva.html" class="btn-reservar modal-reservar">Reservar butacas</a>
        </div>`;

    // Mientras el modal está abierto, el carrusel no se mueve detrás
    clearInterval(temporizadorCarrusel);
    modal.showModal();
}

function cerrarDetalle() {
    modal.close();
}

// Clic fuera del cuadro: el clic cae en el propio <dialog> (su ::backdrop),
// no en el contenido de adentro.
modal.addEventListener("click", (e) => {
    if (e.target === modal) cerrarDetalle();
});

// "close" se dispara con ✕, con Esc y con clic afuera
modal.addEventListener("close", reiniciarAutoplay);


// =============================================================================
//  Inicio
// =============================================================================

async function cargarCartelera() {
    try {
        const respuesta = await fetch("/api/cartelera");
        if (!respuesta.ok) throw new Error();
        const peliculas = await respuesta.json();
        peliculasCartelera = peliculas;
        dibujarCarrusel(peliculas);
        dibujarTarjetas(peliculas);
    } catch {
        document.getElementById("cartelera").innerHTML =
            `<p class="cartelera-vacia">No se pudo cargar la cartelera. ¿Está corriendo el servidor?</p>`;
    }
}

cargarCartelera();

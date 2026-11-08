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

const carrusel = document.getElementById("carruselImagenes");

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

    carrusel.innerHTML = conPoster.map((p) => `
        <div class="slide">
            <img src="${escaparHtml(p.imagen)}" alt="${escaparHtml(p.nombre)}">
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

    contenedor.innerHTML = peliculas.map((p) => {
        const poster = p.imagen
            ? `<img src="${escaparHtml(p.imagen)}" alt="Póster de ${escaparHtml(p.nombre)}">`
            : `<div class="poster-reemplazo">${escaparHtml(p.nombre.charAt(0).toUpperCase())}</div>`;

        const horarios = p.horarios
            .map((h) => `<span>${formatoHora(h.hora)}</span>`)
            .join("");

        return `
            <div class="pelicula">
                ${poster}
                <div class="pelicula-info">
                    <h3>${escaparHtml(p.nombre)}</h3>
                    <p class="pelicula-detalle">${escaparHtml(p.sala)} · ${formatoColones(p.precio)}</p>
                    <div class="horarios">${horarios}</div>
                </div>
            </div>`;
    }).join("");
}


// =============================================================================
//  Inicio
// =============================================================================

async function cargarCartelera() {
    try {
        const respuesta = await fetch("/api/cartelera");
        if (!respuesta.ok) throw new Error();
        const peliculas = await respuesta.json();
        dibujarCarrusel(peliculas);
        dibujarTarjetas(peliculas);
    } catch {
        document.getElementById("cartelera").innerHTML =
            `<p class="cartelera-vacia">No se pudo cargar la cartelera. ¿Está corriendo el servidor?</p>`;
    }
}

cargarCartelera();

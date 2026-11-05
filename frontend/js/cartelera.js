
let slideActual = 0; // Guarda la imagen seleccionada

const carrusel = document.getElementById("carruselImagenes");
const slides = document.querySelectorAll(".slide");
const puntos = document.querySelectorAll(".punto");

// Cambia la imagen del carrusel
function mostrarSlide(numero) {

    slideActual = (numero + slides.length) % slides.length;

    carrusel.style.transform =
        `translateX(-${slideActual * 100}%)`;

    // Actualiza el punto amarillo
    puntos.forEach((punto, index) => {
        punto.classList.toggle("activo", index === slideActual);
    });
}

// Avanza o retrocede
function moverCarrusel(direccion) {
    mostrarSlide(slideActual + direccion);
}

// Selecciona una imagen directamente
function irSlide(numero) {
    mostrarSlide(numero);
}

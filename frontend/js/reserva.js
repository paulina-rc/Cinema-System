"use strict";

document.addEventListener("DOMContentLoaded", () => {
  const movieId = new URLSearchParams(window.location.search).get("pelicula");
  const content = document.querySelector("#reserva-contenido");
  if (movieId) {
    content.textContent = `Pelicula seleccionada (ID ${movieId}). La API de funciones y reservas esta pendiente de implementacion.`;
  }
});

"use strict";

document.addEventListener("DOMContentLoaded", () => {
  const form = document.querySelector("#form-pelicula");
  const status = document.querySelector("#admin-peliculas-estado");
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = Object.fromEntries(new FormData(form));
    data.precio = Number(data.precio);
    data.sala_id = Number(data.sala_id);
    try {
      window.validarPeriodo(data.fecha_inicio, data.fecha_fin);
      await window.CinemaAPI.crearPelicula(data);
      status.textContent = "Pelicula guardada.";
      form.reset();
    } catch (error) {
      status.textContent = `No se pudo guardar: ${error.message}`;
    }
  });
});

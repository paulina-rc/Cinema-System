"use strict";

document.addEventListener("DOMContentLoaded", () => {
  const form = document.querySelector("#form-sala");
  const status = document.querySelector("#admin-salas-estado");
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = Object.fromEntries(new FormData(form));
    data.agrupaciones = Number(data.agrupaciones);
    data.butacas_por_agrupacion = Number(data.butacas_por_agrupacion);
    try {
      window.validarSala(data);
      await window.CinemaAPI.crearSala(data);
      status.textContent = "Sala creada.";
      form.reset();
    } catch (error) {
      status.textContent = `No se pudo crear la sala: ${error.message}`;
    }
  });
});

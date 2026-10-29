"use strict";

document.addEventListener("DOMContentLoaded", async () => {
  const list = document.querySelector("#cartelera");
  const status = document.querySelector("#cartelera-estado");
  try {
    const peliculas = await window.CinemaAPI.getPeliculas();
    if (!Array.isArray(peliculas)) {
      throw new Error("La API devolvio un formato de peliculas no valido.");
    }
    status.textContent = peliculas.length ? "" : "No hay peliculas disponibles.";
    peliculas.forEach((pelicula) => {
      const card = document.createElement("article");
      card.className = "movie-card";
      const title = document.createElement("h2");
      title.textContent = pelicula.nombre;
      const period = document.createElement("p");
      period.textContent = `${pelicula.fecha_inicio} - ${pelicula.fecha_fin}`;
      const link = document.createElement("a");
      link.className = "button";
      link.href = `reserva.html?pelicula=${encodeURIComponent(pelicula.id)}`;
      link.textContent = "Reservar";
      card.append(title, period, link);
      list.append(card);
    });
  } catch (error) {
    status.textContent = `No se pudo cargar la cartelera: ${error.message}`;
  }
});

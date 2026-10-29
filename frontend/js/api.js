"use strict";

window.CinemaAPI = (() => {
  const baseUrl = "http://localhost:8080";

  async function request(path, options = {}) {
    const response = await fetch(`${baseUrl}${path}`, {
      ...options,
      headers: {
        "Content-Type": "application/json",
        ...options.headers
      }
    });
    const body = await response.text();
    if (!response.ok) {
      throw new Error(body || `Error HTTP ${response.status}`);
    }
    return body ? JSON.parse(body) : null;
  }

  return {
    getPeliculas: () => request("/api/peliculas"),
    crearPelicula: (data) => request("/api/peliculas", {
      method: "POST",
      body: JSON.stringify(data)
    }),
    getSalas: () => request("/api/salas"),
    crearSala: (data) => request("/api/salas", {
      method: "POST",
      body: JSON.stringify(data)
    })
  };
})();

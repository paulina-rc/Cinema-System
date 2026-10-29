"use strict";

window.validarPeriodo = (inicio, fin) => {
  if (!inicio || !fin || fin < inicio) {
    throw new Error("La fecha final debe ser igual o posterior a la inicial.");
  }
};

window.validarSala = (sala) => {
  if (!sala.nombre.trim() || !Number.isInteger(sala.agrupaciones) ||
      sala.agrupaciones < 1 || !Number.isInteger(sala.butacas_por_agrupacion) ||
      sala.butacas_por_agrupacion < 1) {
    throw new Error("Revisa el nombre y la configuracion de butacas de la sala.");
  }
};

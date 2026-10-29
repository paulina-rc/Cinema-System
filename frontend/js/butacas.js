"use strict";

window.dibujarButacas = (contenedor, butacas, onChange) => {
  if (!(contenedor instanceof HTMLElement) || !Array.isArray(butacas)) {
    throw new TypeError("Se requiere un contenedor y una lista de butacas.");
  }
  contenedor.replaceChildren();
  contenedor.classList.add("seat-map");
  butacas.forEach((butaca) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "seat";
    button.textContent = `${butaca.agrupacion + 1}-${butaca.numero + 1}`;
    button.disabled = !butaca.habilitada || butaca.ocupada;
    button.setAttribute("aria-pressed", "false");
    button.addEventListener("click", () => {
      const selected = button.getAttribute("aria-pressed") !== "true";
      button.setAttribute("aria-pressed", String(selected));
      onChange(butaca, selected);
    });
    contenedor.append(button);
  });
};

// =============================================================================
//  admin-sesion.js - Protege las páginas del panel admin
//  -----------------------------------------------------------------------------
//  Se carga ANTES del JS de cada página admin (admin-salas.js, admin-peliculas.js).
//    1. Mientras revisa, la página está oculta (clase "sin-sesion" en <html>).
//    2. Si no hay token o el backend dice que venció -> va al login.
//    3. Da tokenAdmin() para que cada página lo mande en sus peticiones,
//       e irAlLogin() para cuando el backend responda 401.
//    4. Conecta el botón "Cerrar sesión" del menú lateral.
//
//  OJO: esto solo es la parte visual. La protección real está en el backend:
//  las rutas que cambian datos responden 401 si no reciben un token válido.
// =============================================================================

const CLAVE_TOKEN = "tokenAdmin";

function tokenAdmin() {
    try { return sessionStorage.getItem(CLAVE_TOKEN) || ""; } catch { return ""; }
}

function borrarToken() {
    try { sessionStorage.removeItem(CLAVE_TOKEN); } catch { /* nada */ }
}

// Va al login y, al entrar, regresa a esta misma página
function irAlLogin() {
    borrarToken();
    const pagina = location.pathname.split("/").pop();
    location.replace("admin-login.html?volver=" + encodeURIComponent(pagina));
}

async function verificarSesion() {
    const token = tokenAdmin();
    if (!token) return irAlLogin();

    try {
        const r = await fetch("/api/sesion", { headers: { Authorization: "Bearer " + token } });
        if (r.status === 401) return irAlLogin();
        const datos = await r.json().catch(() => ({}));
        const etiqueta = document.getElementById("usuarioAdmin");
        if (etiqueta && datos.usuario) etiqueta.textContent = datos.usuario;
    } catch {
        // Sin conexión: se muestra la página igual y cada petición avisará del error
    }
    document.documentElement.classList.remove("sin-sesion");
}

async function cerrarSesion() {
    try {
        await fetch("/api/logout", { method: "POST", headers: { Authorization: "Bearer " + tokenAdmin() } });
    } catch { /* aunque falle, se borra el token del navegador */ }
    borrarToken();
    location.replace("cartelera.html");
}

document.getElementById("btnCerrarSesion").addEventListener("click", cerrarSesion);
verificarSesion();

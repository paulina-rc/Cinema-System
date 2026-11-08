// =============================================================================
//  admin-login.js - Inicio de sesión del administrador  (POST /api/login)
//  -----------------------------------------------------------------------------
//  El usuario y la contraseña los revisa el BACKEND (la contraseña se compara
//  por su hash, ver backend/src/Seguridad.h). Si son correctos, el backend
//  devuelve un token que se guarda en sessionStorage: dura mientras la pestaña
//  esté abierta y se manda en cada petición del panel admin.
// =============================================================================

const CLAVE_TOKEN = "tokenAdmin";

// Páginas a las que se puede volver después de entrar (?volver=...)
const PAGINAS_ADMIN = ["admin-peliculas.html", "admin-salas.html"];

const $ = (id) => document.getElementById(id);

function destino() {
    const volver = new URLSearchParams(location.search).get("volver");
    return PAGINAS_ADMIN.includes(volver) ? volver : PAGINAS_ADMIN[0];
}

function guardarToken(token) {
    try { sessionStorage.setItem(CLAVE_TOKEN, token); } catch { /* modo privado */ }
}

function leerToken() {
    try { return sessionStorage.getItem(CLAVE_TOKEN) || ""; } catch { return ""; }
}

function mostrarError(mensaje) {
    $("errorLogin").textContent = mensaje;
    $("errorLogin").hidden = false;
}

// Si ya hay una sesión abierta, se entra directo al panel
async function revisarSesionAbierta() {
    const token = leerToken();
    if (!token) return;
    try {
        const r = await fetch("/api/sesion", { headers: { Authorization: "Bearer " + token } });
        if (r.ok) location.replace(destino());
    } catch { /* sin servidor: se queda en el login */ }
}

$("formLogin").addEventListener("submit", async (evento) => {
    evento.preventDefault();

    const usuario = $("usuario").value.trim();
    const clave = $("clave").value;
    $("errorLogin").hidden = true;

    if (!usuario || !clave) {
        mostrarError("Escribe el usuario y la contraseña.");
        (usuario ? $("clave") : $("usuario")).focus();
        return;
    }

    const boton = $("btnEntrar");
    boton.disabled = true;
    boton.textContent = "Entrando…";

    try {
        let respuesta;
        try {
            respuesta = await fetch("/api/login", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ usuario, clave })
            });
        } catch {
            throw new Error("No hay conexión con el servidor. ¿Está corriendo iniciar.bat?");
        }

        const datos = await respuesta.json().catch(() => ({}));
        if (!respuesta.ok) {
            throw new Error(respuesta.status === 401
                ? "Usuario o contraseña incorrectos."
                : (datos.error || "Ocurrió un error."));
        }

        guardarToken(datos.token);
        location.replace(destino());
    } catch (e) {
        mostrarError(e.message);
        $("clave").value = "";
        $("clave").focus();
        boton.disabled = false;
        boton.textContent = "Entrar";
    }
});

// Mostrar / ocultar la contraseña
$("btnVerClave").addEventListener("click", () => {
    const visible = $("clave").type === "text";
    $("clave").type = visible ? "password" : "text";
    $("btnVerClave").textContent = visible ? "Mostrar" : "Ocultar";
    $("btnVerClave").setAttribute("aria-label", visible ? "Mostrar contraseña" : "Ocultar contraseña");
});

revisarSesionAbierta();

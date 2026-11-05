// =============================================================================
//  ReservaTemporal.h
//  -----------------------------------------------------------------------------
//  MATRIZ EN MEMORIA TEMPORAL (Modulo 4 del examen).
//
//  Mientras el cliente esta en el paso 4 eligiendo butacas, NADA se guarda en
//  SQLite. Cada clic cambia un numero en la matriz que vive aqui, en la memoria
//  del servidor. Solo al confirmar (paso 5) se recorre la matriz y se insertan
//  en SQLite las posiciones con estado 3.
//
//  Estados de la matriz (los mismos numeros que usa JavaScript):
//     0 = disponible                    (verde)
//     1 = ocupada, ya estaba en SQLite  (rojo)
//     2 = danada / fuera de servicio    (sin color, no se puede tocar)
//     3 = seleccionada NUEVA del cliente actual (amarillo)
//
//  El estado 3 es la "marca" que pide el examen para distinguir las butacas
//  que ya estaban reservadas (1) de las que este cliente acaba de elegir (3).
//
//  Cada reservacion en proceso se identifica con un TOKEN ("r1", "r2", ...).
//  El navegador manda ese token en cada peticion para que el servidor sepa
//  de cual matriz se trata (puede haber varios clientes reservando a la vez).
// =============================================================================
#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "modelos.h"

// Estados de la matriz con nombre, para no escribir numeros "magicos".
const int DISPONIBLE   = 0;
const int OCUPADA      = 1;
const int DANADA       = 2;
const int SELECCIONADA = 3;

// Nombre de una butaca para mostrarla al cliente:
// agrupacion 0 -> "A", 1 -> "B"...  numero 0 -> 1, 1 -> 2...   (0,4) -> "A5"
inline std::string etiquetaButaca(int agrupacion, int numero) {
    return std::string(1, static_cast<char>('A' + agrupacion)) + std::to_string(numero + 1);
}

// -----------------------------------------------------------------------------
//  Una reservacion que todavia no se ha confirmado.
// -----------------------------------------------------------------------------
struct ReservaTemporal {
    int         cliente_id = 0;
    InfoFuncion funcion;                       // pelicula, sala, precio, hora
    std::string fecha;                         // dia de la funcion (hoy)
    int         cantidad = 0;                  // butacas que pidio en el paso 3
    int         seleccionadas = 0;             // cuantas tienen estado 3 ahora

    std::vector<std::vector<int>> matriz;      // matriz[agrupacion][numero] = estado
    std::vector<std::vector<int>> ids;         // id de la butaca en SQLite en cada posicion

    std::chrono::steady_clock::time_point creada = std::chrono::steady_clock::now();

    // Recorre la matriz y devuelve las posiciones con estado 3.
    // Se usa al confirmar: SOLO estas se insertan en SQLite.
    std::vector<std::pair<int, int>> posicionesNuevas() const {
        std::vector<std::pair<int, int>> nuevas;
        for (size_t a = 0; a < matriz.size(); a++)
            for (size_t n = 0; n < matriz[a].size(); n++)
                if (matriz[a][n] == SELECCIONADA)
                    nuevas.push_back({static_cast<int>(a), static_cast<int>(n)});
        return nuevas;
    }

    // Nombres de las butacas seleccionadas, por ejemplo ["A5", "B2"].
    std::vector<std::string> etiquetasNuevas() const {
        std::vector<std::string> nombres;
        for (const auto& [a, n] : posicionesNuevas()) nombres.push_back(etiquetaButaca(a, n));
        return nombres;
    }
};

// -----------------------------------------------------------------------------
//  Guarda todas las reservaciones en proceso: token -> ReservaTemporal.
//  Usa un mutex porque cpp-httplib atiende cada peticion en un hilo distinto.
// -----------------------------------------------------------------------------
class ReservasEnMemoria {
public:
    // Si el cliente abandona la pagina, su matriz se borra sola despues
    // de este tiempo para no llenar la memoria.
    static constexpr int MINUTOS_MAXIMOS = 20;

    // Guarda una reservacion nueva y devuelve su token.
    std::string crear(ReservaTemporal r) {
        std::lock_guard<std::mutex> lock(mutex_);
        limpiarViejas();
        std::string token = "r" + std::to_string(++contador_);
        reservas_[token] = std::move(r);
        return token;
    }

    // Ejecuta "accion" sobre la reservacion del token (con el mutex tomado).
    // Devuelve false si el token no existe o ya expiro.
    template <typename Accion>
    bool usar(const std::string& token, Accion accion) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reservas_.find(token);
        if (it == reservas_.end()) return false;
        accion(it->second);
        return true;
    }

    // Copia de la reservacion (para confirmar sin tener el mutex tomado
    // mientras se escribe en SQLite).
    std::optional<ReservaTemporal> copia(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reservas_.find(token);
        if (it == reservas_.end()) return std::nullopt;
        return it->second;
    }

    // Borra la reservacion de memoria (al confirmar o al cancelar).
    bool borrar(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        return reservas_.erase(token) > 0;
    }

private:
    std::map<std::string, ReservaTemporal> reservas_;
    std::mutex mutex_;
    int        contador_ = 0;

    void limpiarViejas() {
        auto ahora = std::chrono::steady_clock::now();
        for (auto it = reservas_.begin(); it != reservas_.end();) {
            auto minutos = std::chrono::duration_cast<std::chrono::minutes>(ahora - it->second.creada);
            if (minutos.count() >= MINUTOS_MAXIMOS) it = reservas_.erase(it);
            else ++it;
        }
    }
};

// aqui se guarda la matriz de butacas mientras el cliente esta eligiendo
// no se escribe nada en SQLite hasta que confirma, y ahi solo van las de estado 3.
#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "modelos.h"

// estados de cada butaca, son los mismos numeros que usa el JavaScript
// el 3 sirve para separar las que este cliente acaba de elegir de las que
// ya estaban reservadas (1), porque esas no hay que insertarlas otra vez
const int DISPONIBLE   = 0;
const int OCUPADA      = 1;
const int DANADA       = 2;
const int SELECCIONADA = 3;

// por ejemplo (0,4) -> A5
inline std::string etiquetaButaca(int agrupacion, int numero) {
    return std::string(1, static_cast<char>('A' + agrupacion)) + std::to_string(numero + 1);
}

struct ReservaTemporal {
    int         cliente_id = 0;
    InfoFuncion funcion;
    std::string fecha;
    int         cantidad = 0;                  // las que pidio en el paso 3
    int         seleccionadas = 0;             // las que tienen estado 3 ahora

    std::vector<std::vector<int>> matriz;      // matriz[agrupacion][numero] = estado
    std::vector<std::vector<int>> ids;         // id en SQLite de cada butaca, misma forma que matriz

    std::chrono::steady_clock::time_point creada = std::chrono::steady_clock::now();

    // al confirmar solo se insertan estas, las de estado 1 ya estaban en SQLite
    std::vector<std::pair<int, int>> posicionesNuevas() const {
        std::vector<std::pair<int, int>> nuevas;
        for (size_t a = 0; a < matriz.size(); a++)
            for (size_t n = 0; n < matriz[a].size(); n++)
                if (matriz[a][n] == SELECCIONADA)
                    nuevas.push_back({static_cast<int>(a), static_cast<int>(n)});
        return nuevas;
    }

    std::vector<std::string> etiquetasNuevas() const {
        std::vector<std::string> nombres;
        for (const auto& [a, n] : posicionesNuevas()) nombres.push_back(etiquetaButaca(a, n));
        return nombres;
    }
};

// cada reserva en proceso tiene un token ("r1", "r2"...) que el navegador manda
// en cada peticion, asi se sabe de cual matriz se trata
// lleva mutex porque httplib atiende cada peticion en un hilo distinto y puede
// haber varios clientes reservando al mismo tiempo
class ReservasEnMemoria {
public:
    // si alguien cierra la pagina sin terminar, su reserva se borra despues de
    // este tiempo para que no se quede ocupando memoria.
    static constexpr int MINUTOS_MAXIMOS = 20;

    std::string crear(ReservaTemporal r) {
        std::lock_guard<std::mutex> lock(mutex_);
        limpiarViejas();
        std::string token = "r" + std::to_string(++contador_);
        reservas_[token] = std::move(r);
        return token;
    }

    // la accion corre con el mutex tomado, para que dos peticiones no cambien
    // la misma matriz a la vez. Da false si el token no existe o ya expiro
    template <typename Accion>
    bool usar(const std::string& token, Accion accion) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reservas_.find(token);
        if (it == reservas_.end()) return false;
        accion(it->second);
        return true;
    }

    // devuelve una copia para confirmar sin dejar el mutex tomado mientras se
    // escribe en SQLite. Como mientras tanto otro cliente pudo ganar una butaca,
    // confirmarReserva (Database.cpp) vuelve a revisar antes de guardar
    std::optional<ReservaTemporal> copia(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reservas_.find(token);
        if (it == reservas_.end()) return std::nullopt;
        return it->second;
    }

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

#include "MatrizSala.h"

#include <stdexcept>

MatrizSala::MatrizSala(int agrupaciones, int butacasPorAgrupacion) {
    if (agrupaciones <= 0 || butacasPorAgrupacion <= 0) {
        throw std::invalid_argument("Las dimensiones de la sala deben ser positivas.");
    }
    butacas_.assign(static_cast<std::size_t>(agrupaciones),
                    std::vector<bool>(static_cast<std::size_t>(butacasPorAgrupacion), true));
}

bool MatrizSala::estaDentro(int agrupacion, int numero) const noexcept {
    return agrupacion >= 0 && numero >= 0 &&
           static_cast<std::size_t>(agrupacion) < butacas_.size() &&
           static_cast<std::size_t>(numero) < butacas_[static_cast<std::size_t>(agrupacion)].size();
}

bool MatrizSala::estaHabilitada(int agrupacion, int numero) const {
    if (!estaDentro(agrupacion, numero)) {
        throw std::out_of_range("La butaca solicitada no existe.");
    }
    return butacas_[static_cast<std::size_t>(agrupacion)][static_cast<std::size_t>(numero)];
}

bool MatrizSala::marcar(int agrupacion, int numero, bool habilitada) {
    if (!estaDentro(agrupacion, numero)) {
        return false;
    }
    butacas_[static_cast<std::size_t>(agrupacion)][static_cast<std::size_t>(numero)] = habilitada;
    return true;
}

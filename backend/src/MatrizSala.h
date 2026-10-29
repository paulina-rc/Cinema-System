#pragma once

#include <vector>

class MatrizSala {
public:
    MatrizSala(int agrupaciones, int butacasPorAgrupacion);
    bool estaDentro(int agrupacion, int numero) const noexcept;
    bool estaHabilitada(int agrupacion, int numero) const;
    bool marcar(int agrupacion, int numero, bool habilitada);

private:
    std::vector<std::vector<bool>> butacas_;
};

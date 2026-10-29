#include "Validaciones.h"

#include <cctype>
#include <cmath>

namespace Validaciones {
bool fechaISO(const std::string& fecha) {
    if (fecha.size() != 10 || fecha[4] != '-' || fecha[7] != '-') return false;
    for (std::size_t i = 0; i < fecha.size(); ++i) {
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(fecha[i]))) return false;
    }
    const int mes = std::stoi(fecha.substr(5, 2));
    const int dia = std::stoi(fecha.substr(8, 2));
    return mes >= 1 && mes <= 12 && dia >= 1 && dia <= 31;
}

bool hora24(const std::string& hora) {
    if (hora.size() != 5 || hora[2] != ':') return false;
    if (!std::isdigit(static_cast<unsigned char>(hora[0])) ||
        !std::isdigit(static_cast<unsigned char>(hora[1])) ||
        !std::isdigit(static_cast<unsigned char>(hora[3])) ||
        !std::isdigit(static_cast<unsigned char>(hora[4]))) return false;
    const int horas = std::stoi(hora.substr(0, 2));
    const int minutos = std::stoi(hora.substr(3, 2));
    return horas < 24 && minutos < 60;
}

bool telefono(const std::string& valor) {
    int digitos = 0;
    for (unsigned char c : valor) {
        if (std::isdigit(c)) ++digitos;
        else if (c != '+' && c != '-' && c != ' ' && c != '(' && c != ')') return false;
    }
    return digitos >= 7 && digitos <= 15;
}

bool precio(double valor) {
    return std::isfinite(valor) && valor > 0;
}
}

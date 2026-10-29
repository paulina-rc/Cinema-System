#pragma once

#include <string>

namespace Validaciones {
bool fechaISO(const std::string& fecha);
bool hora24(const std::string& hora);
bool telefono(const std::string& telefono);
bool precio(double precio);
}

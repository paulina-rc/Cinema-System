// =============================================================================
//  Seguridad.h
//  -----------------------------------------------------------------------------
//  Login del administrador.
//
//  1. CONTRASENAS: nunca se guardan tal cual. Se guarda un "hash" SHA-256:
//        hash = SHA256(sal + contrasena), repetido 10 000 veces
//     La "sal" es un texto aleatorio distinto para cada usuario, asi dos
//     usuarios con la misma contrasena no tienen el mismo hash.
//     Para revisar un login se calcula el hash de lo que escribio la persona
//     y se compara con el guardado.
//
//  2. SESIONES: al entrar bien, el servidor crea un TOKEN aleatorio y lo
//     guarda en memoria con su hora de vencimiento (8 horas). El navegador
//     lo manda en cada peticion del panel admin en el encabezado:
//        Authorization: Bearer <token>
//     Si el servidor se reinicia, las sesiones se borran y hay que entrar de nuevo.
// =============================================================================
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <random>
#include <string>

// -----------------------------------------------------------------------------
//  SHA-256 (algoritmo estandar FIPS 180-4). Devuelve el resultado en hexadecimal.
// -----------------------------------------------------------------------------
inline std::string sha256(const std::string& mensaje) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };

    // Relleno: un bit 1, ceros, y el largo original en bits (64 bits, big endian)
    std::string datos = mensaje;
    uint64_t largo_bits = static_cast<uint64_t>(mensaje.size()) * 8;
    datos.push_back(static_cast<char>(0x80));
    while (datos.size() % 64 != 56) datos.push_back('\0');
    for (int i = 7; i >= 0; i--) datos.push_back(static_cast<char>((largo_bits >> (i * 8)) & 0xff));

    // Procesa cada bloque de 64 bytes
    for (size_t bloque = 0; bloque < datos.size(); bloque += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = (uint32_t(uint8_t(datos[bloque + i * 4])) << 24) |
                   (uint32_t(uint8_t(datos[bloque + i * 4 + 1])) << 16) |
                   (uint32_t(uint8_t(datos[bloque + i * 4 + 2])) << 8) |
                   (uint32_t(uint8_t(datos[bloque + i * 4 + 3])));
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    static const char* hex = "0123456789abcdef";
    std::string resultado;
    for (uint32_t valor : h)
        for (int i = 28; i >= 0; i -= 4) resultado.push_back(hex[(valor >> i) & 0xf]);
    return resultado;
}

// Texto hexadecimal aleatorio (para sales y tokens). bytes=16 -> 32 caracteres.
inline std::string textoAleatorio(int bytes = 16) {
    static std::random_device rd;
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    static const char* hex = "0123456789abcdef";
    std::string t;
    for (int i = 0; i < bytes; i++) {
        unsigned v = rd() & 0xff;
        t.push_back(hex[v >> 4]);
        t.push_back(hex[v & 0xf]);
    }
    return t;
}

// Hash de una contrasena con su sal (10 000 vueltas para que sea lento de adivinar).
inline std::string hashClave(const std::string& sal, const std::string& clave) {
    std::string h = sha256(sal + clave);
    for (int i = 0; i < 10000; i++) h = sha256(sal + h);
    return h;
}

// Compara dos textos tardando siempre lo mismo (no "avisa" en que letra fallo).
inline bool igualesSeguro(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char dif = 0;
    for (size_t i = 0; i < a.size(); i++) dif |= static_cast<unsigned char>(a[i] ^ b[i]);
    return dif == 0;
}

// -----------------------------------------------------------------------------
//  Sesiones abiertas del administrador: token -> (usuario, vencimiento)
// -----------------------------------------------------------------------------
class SesionesAdmin {
public:
    using Reloj = std::chrono::steady_clock;
    static constexpr std::chrono::hours DURACION{8};

    // Crea una sesion nueva y devuelve su token.
    std::string crear(const std::string& usuario) {
        std::lock_guard<std::mutex> lock(mutex_);
        limpiarVencidas();
        std::string token = textoAleatorio(32);
        sesiones_[token] = {usuario, Reloj::now() + DURACION};
        return token;
    }

    // Devuelve el usuario de la sesion, o "" si el token no existe o vencio.
    std::string usuario(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sesiones_.find(token);
        if (it == sesiones_.end()) return "";
        if (Reloj::now() > it->second.vence) {
            sesiones_.erase(it);
            return "";
        }
        return it->second.usuario;
    }

    void cerrar(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        sesiones_.erase(token);
    }

private:
    struct Sesion {
        std::string usuario;
        Reloj::time_point vence;
    };
    std::map<std::string, Sesion> sesiones_;
    std::mutex mutex_;

    void limpiarVencidas() {
        auto ahora = Reloj::now();
        for (auto it = sesiones_.begin(); it != sesiones_.end();) {
            if (ahora > it->second.vence) it = sesiones_.erase(it);
            else ++it;
        }
    }
};

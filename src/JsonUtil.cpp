#include "JsonUtil.h"
#include <sstream>

namespace JsonUtil {

std::string extraerString(const std::string& json, const std::string& clave) {
    std::string buscado = "\"" + clave + "\"";
    size_t pos = json.find(buscado);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + buscado.size());
    if (pos == std::string::npos) return "";
    pos++;

    // Saltar espacios en blanco
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        size_t inicio = pos + 1;
        size_t fin = inicio;
        while (fin < json.size() && json[fin] != '"') {
            if (json[fin] == '\\') fin++; // saltar caracter escapado
            fin++;
        }
        return json.substr(inicio, fin - inicio);
    } else {
        // Valor sin comillas (numero, true/false, null) -> lo devolvemos como texto crudo
        size_t fin = pos;
        while (fin < json.size() && json[fin] != ',' && json[fin] != '}' && json[fin] != ']') fin++;
        std::string valor = json.substr(pos, fin - pos);
        // recortar espacios finales
        while (!valor.empty() && (valor.back() == ' ' || valor.back() == '\n' || valor.back() == '\r')) valor.pop_back();
        return valor;
    }
}

double extraerNumero(const std::string& json, const std::string& clave) {
    std::string valor = extraerString(json, clave);
    if (valor.empty()) return 0.0;
    try {
        return std::stod(valor);
    } catch (...) {
        return 0.0;
    }
}

std::string escapar(const std::string& texto) {
    std::string resultado;
    resultado.reserve(texto.size());
    for (char c : texto) {
        if (c == '"' || c == '\\') resultado += '\\';
        resultado += c;
    }
    return resultado;
}

} // namespace JsonUtil

#ifndef JSONUTIL_H
#define JSONUTIL_H

#include <string>

// ============================================================================
// JsonUtil
// ----------------------------------------------------------------------------
// Utilidad minima para extraer valores de un JSON PLANO (sin anidar) recibido
// en el cuerpo de una peticion HTTP proveniente de fetch() en el frontend.
// No es un parser JSON completo (no se usan librerias externas por
// requerimiento del proyecto), solo localiza "clave":"valor" o
// "clave":numero dentro del texto crudo.
// ============================================================================
namespace JsonUtil {
    // Extrae el valor de una clave de tipo string. Retorna "" si no existe.
    std::string extraerString(const std::string& json, const std::string& clave);

    // Extrae el valor de una clave numerica como double. Retorna 0.0 si no existe.
    double extraerNumero(const std::string& json, const std::string& clave);

    // Escapa comillas dobles para incrustar de forma segura texto en un JSON de salida.
    std::string escapar(const std::string& texto);
}

#endif // JSONUTIL_H

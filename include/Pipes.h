#ifndef PIPES_H
#define PIPES_H

#include <string>

// ============================================================================
// Pipes (Tuberias POSIX)
// ----------------------------------------------------------------------------
// Implementa la comunicacion entre el SERVIDOR PRINCIPAL (main.cpp) y el
// PROCESO INDEPENDIENTE DE REPORTES (Process_Reports.cpp), lanzado mediante
// fork()+exec(). El servidor principal escribe en el extremo de escritura
// del pipe las metricas/historiales solicitadas, y el proceso de reportes
// las lee de su entrada estandar (stdin), heredada via dup2 del pipe.
// ============================================================================
class ReportPipe {
public:
    // Lanza el proceso Process_Reports via fork()+exec(), le envia 'payload'
    // por el pipe, espera su finalizacion (waitpid) y retorna la salida
    // generada por el proceso hijo (leida de un segundo pipe, stdout->padre).
    static std::string ejecutarReporte(const std::string& payload);
};

#endif // PIPES_H

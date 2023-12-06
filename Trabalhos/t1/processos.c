#include "processos.h"

#include <stdlib.h>

processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo)
{
    processo* process = malloc(sizeof(processo));
    process->estado_cpu = malloc(sizeof(cpu_state));

    process->estado_cpu->PC = PC;
    process->estado_cpu->A = A;
    process->estado_cpu->X = X;
    process->estado_cpu->erro = erro;
    process->estado_cpu->complemento = complemento;
    process->estado_cpu->modo = modo;

    return process;
}
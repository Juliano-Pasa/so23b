#include "processos.h"

#include <stdlib.h>

processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo, pr_state estado_processo, int pid)
{
    processo* process = malloc(sizeof(processo));
    process->estado_cpu = malloc(sizeof(cpu_state));

    process->estado_cpu->PC = PC;
    process->estado_cpu->A = A;
    process->estado_cpu->X = X;
    process->estado_cpu->erro = erro;
    process->estado_cpu->complemento = complemento;
    process->estado_cpu->modo = modo;

    process->estado_processo = estado_processo;
    process->pid = pid;

    return process;
}

void mata_processo(processo* processo)
{
    free(processo->estado_cpu);
    free(processo);
}
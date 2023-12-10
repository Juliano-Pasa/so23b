#include "processos.h"

#include <stdlib.h>
#include <sys/time.h>


processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo, pr_state estado_processo, int pid, int terminal)
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
    process->terminal = terminal;

    process->pid = pid;
    process->quantum = 0;
    process->prioridade = 0.5f;
    process->exec_inicio = -1;


    return process;
}

void mata_processo(processo* processo)
{
    free(processo->estado_cpu);
    free(processo);
}
#ifndef PROCESSOS_H
#define PROCESSOS_H

#include "cpu.h"

// Valores iniciais arbitrarios
typedef enum
{
    INVALID,
    READY,
    RUNNING,
    WAITING,
    BLOCKED
} pr_state;

typedef struct processo processo;
typedef struct cpu_state cpu_state;

struct cpu_state
{
    int PC;
    int A;
    int X;
    err_t erro;
    int complemento;
    cpu_modo_t modo;
};

struct processo
{
    cpu_state* estado_cpu;
    pr_state estado_processo;
    int pid;
    int terminal;
    int quantum;
};

processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo, pr_state estado_processo, int pid, int terminal);
void mata_processo(processo* processo);

#endif
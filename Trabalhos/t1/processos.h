#ifndef PROCESSOS_H
#define PROCESSOS_H

#include "cpu.h"

// Valores iniciais arbitrarios
typedef enum
{
    INVALID,
    RUNNING,
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
    char c;
};

processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo);

#endif
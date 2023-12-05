#ifndef PROCESSOS_H
#define PROCESSOS_H

// Valores iniciais arbitrarios
typedef enum
{
    INVALID,
    RUNNING,
    BLOCKED
} pr_state;

typedef struct processo processo;
typedef struct cpu_state cpu_state;

#endif
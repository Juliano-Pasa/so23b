#include "processos.h"
#include "cpu.h"

struct cpu_state
{
    int PC;
    int A;
    int X;
    err_t *erro;
    int complemento;
    cpu_modo_t *modo;
};

struct processo
{
    cpu_state* estado_cpu;
};
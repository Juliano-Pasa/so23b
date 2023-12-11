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
typedef struct meta_processo meta_processo;

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
    int exec_inicio;
    double prioridade;   

    //Para calcular o tempo de retorno
    int tempo_nascimento;
    int tempo_morte;

    int num_preemp_proc;
    int num_bloqueado;
    int num_pronto;
    int num_executando;
    int num_esperando;

    int tempo_pronto;
    int tempo_bloqueado;
    int tempo_executando;
    int tempo_esperando;

    // Tempo médio de resposta de cada processo (tempo médio em estado pronto) 
    // = divisão do tempo total / numero de vezes pronto
};


struct meta_processo
{
    int pid;

    //Para calcular o tempo de retorno
    int tempo_retorno; //morte - vida

    int num_preemp_proc;
    int num_bloqueado;
    int num_pronto;
    int num_executando;
    int num_esperando;

    int tempo_pronto;
    int tempo_bloqueado;
    int tempo_executando;
    int tempo_esperando;

    // Tempo médio de resposta de cada processo (tempo médio em estado pronto) 
    // = divisão do tempo total / numero de vezes pronto
};



processo* cria_processo(int PC, int A, int X, err_t erro, int complemento, cpu_modo_t modo, pr_state estado_processo, int pid, int terminal);
meta_processo* cria_meta_processo(processo* p);
void mata_processo(processo* processo);

#endif
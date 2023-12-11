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

    
    process->tempo_nascimento = 0;
    process->tempo_morte = 0;

    process->num_preemp_proc = 0;
    process->num_bloqueado = 0;
    process->num_pronto = 0;
    process->num_executando = 0;
    process->num_esperando = 0;

    process->tempo_pronto = 0;
    process->tempo_bloqueado = 0;
    process->tempo_executando = 0;
    process->tempo_esperando = 0;


    return process;
}

void mata_processo(processo* processo)
{
    free(processo->estado_cpu);
    free(processo);
}

meta_processo* cria_meta_processo(processo* p)
{
    meta_processo* mp = malloc(sizeof(meta_processo));

    mp->pid = p->pid;

    mp->tempo_retorno = p->tempo_nascimento - p->tempo_morte;

    mp->num_preemp_proc = p->num_preemp_proc;
    mp->num_bloqueado   = p->num_bloqueado;
    mp->num_pronto      = p->num_pronto;
    mp->num_executando  = p->num_executando;
    mp->num_esperando   = p->num_esperando;

    mp->tempo_pronto   = p->tempo_pronto;
    mp->tempo_bloqueado   = p->tempo_bloqueado;
    mp->tempo_executando   = p->tempo_executando;
    mp->tempo_esperando   = p->tempo_esperando;

    return mp;    
}
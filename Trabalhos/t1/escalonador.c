#include "so.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "processos.h"
#include "escalonador.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

static no_processo* cria_no(processo* p);
static void libera_no(no_processo* no_p);
static no_processo* fila_acha_fim(fila_processos* f);

escalonador_t* escalonador_cria()
{
    escalonador_t* esc = malloc(sizeof(escalonador_t));
    esc->fila_prontos.raiz = NULL;

    /*
        int i;
        esc->fila_prontos.raiz = NULL;
        for(i = 0; i < numero_processos; i++)
        {
            if(processos[i] != NULL)
            {
                if(processos[i]->estado_processo == READY)
                    escalonador_enfila_processo(f, esc);
            }
        }
    */

    return esc;
}

void escalonador_enfila_processo(processo* p, escalonador_t* esc)
{
    printf("\nBotamo um nó na fila fodase.");
    fila_processos* f = &esc->fila_prontos;
    if(f->raiz == NULL)
        f->raiz = cria_no(p);
    else
    {
        no_processo* ultimo_no = fila_acha_fim(f);
        ultimo_no->proximo_no = cria_no(p);
    }

}

// Pop da fila.

processo* escalonador_desenfila_processo(escalonador_t* esc)
{

    if(esc->fila_prontos.raiz == NULL)
    {
        printf("Escalonador retornou nulo, não havia um nó raiz de processo.");
        return NULL;
    }

    no_processo* no_p = esc->fila_prontos.raiz;
    esc->fila_prontos.raiz = no_p->proximo_no;
    processo* proc = no_p->proc;
    libera_no(no_p);

    return proc;
}

static no_processo* fila_acha_fim(fila_processos* f)
{
    no_processo* no_p = f->raiz;

    if(no_p == NULL)
    {
        printf("\nA fila está vazia e buscaram o fim dela. :C\n");
    }

    while(no_p->proximo_no != NULL)
    {
        no_p = no_p->proximo_no;
    }

    return no_p;
}

static no_processo* cria_no(processo* p)
{
    no_processo* no_p = malloc(sizeof(no_processo));

    no_p->prioridade = 1;
    no_p->proc = p;
    no_p->proximo_no = NULL;

    return no_p;
}

static void libera_no(no_processo* no_p)
{
    free(no_p);
}
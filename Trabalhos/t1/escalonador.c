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
static no_processo* fila_acha_lugar_prioridade(float prioridade, fila_processos* f);

escalonador_t* escalonador_cria()
{
    escalonador_t* esc = malloc(sizeof(escalonador_t));
    esc->fila_prontos = malloc(sizeof(fila_processos));
    esc->fila_prontos->raiz = NULL;

    /*
        int i;
        esc->fila_prontos->raiz = NULL;
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
    
    if(esc->fila_prontos->raiz == NULL)
        esc->fila_prontos->raiz = cria_no(p);
    else
    {
        no_processo* ultimo_no = fila_acha_fim(esc->fila_prontos);
        ultimo_no->proximo_no = cria_no(p);
    }

}

// Pop da fila.
processo* escalonador_desenfila_processo(escalonador_t* esc)
{
    if(esc->fila_prontos->raiz == NULL)
    {
        //printf("Escalonador retornou nulo, não havia um nó raiz de processo.");
        return NULL;
    }

    no_processo* no_p = esc->fila_prontos->raiz;
    esc->fila_prontos->raiz = no_p->proximo_no;
    processo* proc = no_p->proc;
    libera_no(no_p);

    return proc;
}

//Insere processos com maior prioridade primeiro
void escalonador_enfila_processo_prioridade(processo* p, escalonador_t* esc)
{
    //Guard Clause
    //Se a fila está vazia, ele deveria setar como 0.5?
    if(esc->fila_prontos->raiz == NULL)
    {        
        esc->fila_prontos->raiz = cria_no(p);
        return;
    }

    //Se a prioridae for maior que o primeiro nó da fila, atualiza
    if(p->prioridade < esc->fila_prontos->raiz->proc->prioridade)
    {
        no_processo* antigo_primeiro = esc->fila_prontos->raiz;
        no_processo* novo_primeiro = cria_no(p);
        novo_primeiro->proximo_no = antigo_primeiro;
        esc->fila_prontos->raiz = novo_primeiro;
        return;
    }

    //Só restam os casos onde:
    // - O lugar certo da prioridade é no meio da fila.
    // - O lugar certo da prioridade é no final da fila.
    //Ambos são tratados aqui.

    no_processo* no_antecessor_novo = fila_acha_lugar_prioridade(p->prioridade, esc->fila_prontos);
    no_processo* no_sucessor_novo = no_antecessor_novo->proximo_no;
    no_processo* no_novo = cria_no(p);

    no_novo->proximo_no = no_sucessor_novo;
    no_antecessor_novo->proximo_no = no_novo;

}

//Não trata o primeiro o caso onde o primeiro lugar é o certo, isso é tratado na outra função.
static no_processo* fila_acha_lugar_prioridade(float prioridade, fila_processos* f)
{
    no_processo* no_p = f->raiz;

    //Tratamento para listas sem elementos deve ser feito fora da função.
    if(no_p == NULL)
    {
        //printf("\nA fila está vazia, nao eh possivel encontrar o lugar. :C\n");
        return no_p;
    }

    //Valor menor de prioridader indica maior preferencia na execução.
    //Enquanto a prioridade atual for superior ou igual a prioridade buscada,
    //Esse igual garante que os processos com mesma prioridade sejam executados em ordem de chegada, :D
    while(prioridade >= no_p->proc->prioridade)
    {
        //Verifica se o próximo nó existe
        if(no_p->proximo_no)
        {
            //Achou o lugar onde deve ficar, a prioridade é MENOR do que o elemento antecessor e MAIOR do que do sucessor.
            if(no_p->proximo_no->proc->prioridade > prioridade)
            {
                //Retorna o nó atual, que tem o número de prioridade menor do que o nó que deve ser inserido
                return no_p; 
            }            
            else
            {
                no_p = no_p->proximo_no;
            }
        }
        else
        {
            //Chegou
            return no_p;
        }
    }

    //Ele só chega ate aqui se a prioridade buscada for a maior da lista. Nesse caso, o tratamento deve ser feito fora da chamada da função.
    return NULL;

}

static no_processo* fila_acha_fim(fila_processos* f)
{
    no_processo* no_p = f->raiz;

    if(no_p == NULL)
    {
        return NULL;
        //printf("\nA fila está vazia e buscaram o fim dela. :C\n");
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

    no_p->prioridade = p->prioridade;
    no_p->proc = p;
    no_p->proximo_no = NULL;

    return no_p;
}

static void libera_no(no_processo* no_p)
{
    free(no_p);
}


void imprimir_lista(escalonador_t* esc)
{
    no_processo* no_p = esc->fila_prontos->raiz;

    int i = 0;
    while(no_p != NULL)
    {
        i++;
        printf("\nElemento %i da lista | PID: %i | Prioridade %f.", i, no_p->proc->pid, no_p->proc->prioridade);

        if(no_p->proximo_no == NULL) break;

        no_p = no_p->proximo_no;
    }
}

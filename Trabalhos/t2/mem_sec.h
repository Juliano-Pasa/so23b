#ifndef MEM_SEC_H
#define MEM_SEC_H

#define MAX_PROGRAMAS 20

#include <stdbool.h>
#include "memoria.h"
#include "programa.h"
#include "err.h"

typedef struct mem_sec_t mem_sec_t;
typedef struct mapa_valor_t mapa_valor_t;
typedef struct mapa_programa_t mapa_programa_t;

struct mem_sec_t
{
    mem_t* mem;
    int bloqueio;
    bool bloqueada;
    
    int inicio;
    mapa_programa_t *mapa_programas[MAX_PROGRAMAS];
};

struct mapa_valor_t
{
    int inicio;
    int fim;
};

mem_sec_t* mem_sec_cria(int tam);
void mem_sec_destroi(mem_sec_t *self);

int mem_sec_salva_programa(mem_sec_t *self, programa_t *prog, char *nome);

bool mem_sec_existe_programa(mem_sec_t *self, char *nome);
mapa_valor_t* mem_sec_pega_end_programa(mem_sec_t *self, char *nome);

err_t mem_sec_le(mem_sec_t *self, int endereco, int *pvalor);


#endif
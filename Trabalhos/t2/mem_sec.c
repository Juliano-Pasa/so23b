#include <stdlib.h>
#include <string.h>
#include "mem_sec.h"
#include <stdio.h>

struct mapa_programa_t
{
    char *key;
    mapa_valor_t *value;
};

void mapea_programa(mem_sec_t* self, char *nome, int inicio, int fim);


mem_sec_t* mem_sec_cria(int tam)
{
    mem_sec_t* memoria = malloc(sizeof(mem_sec_t));

    memoria->mem = mem_cria(tam);
    memoria->bloqueio = 0;
    memoria->bloqueada = false;
    memoria->inicio = 0;

    for (int i = 0; i < MAX_PROGRAMAS; i++)
    {
        memoria->mapa_programas[i] = NULL;
    }

    return memoria;
}

void mem_sec_destroi(mem_sec_t *self)
{
    mem_destroi(self->mem);
    free(self);
}

int mem_sec_salva_programa(mem_sec_t *self, programa_t *prog, char *nome)
{
    int end_inicio = prog_end_carga(prog);
    int end_fim = end_inicio + prog_tamanho(prog) - 1;

    for (int end = end_inicio; end <= end_fim; end++)
    {
        if (mem_escreve(self->mem, end + self->inicio, prog_dado(prog, end)) != ERR_OK) 
        {
            return -1;
        }
    }

    mapea_programa(self, nome, end_inicio + self->inicio, end_fim + self->inicio);
    self->inicio += prog_tamanho(prog);

    return self->inicio;
}

void mapea_programa(mem_sec_t* self, char *nome, int inicio, int fim)
{
    int index = 0;
    while (self->mapa_programas[index] != NULL) index++;

    self->mapa_programas[index] = malloc(sizeof(mapa_programa_t));

    self->mapa_programas[index]->key = nome;
    self->mapa_programas[index]->value = malloc(sizeof(mapa_valor_t));
    self->mapa_programas[index]->value->inicio = inicio;
    self->mapa_programas[index]->value->fim = fim;
}

bool mem_sec_existe_programa(mem_sec_t *self, char *nome)
{
    for (int i = 0; i < MAX_PROGRAMAS; i++)
    {
        if (self->mapa_programas[i] == NULL) continue;
        if (strcmp(nome, self->mapa_programas[i]->key) == 0) return true;
    }
    return false;
}

mapa_valor_t* mem_sec_pega_end_programa(mem_sec_t *self, char *nome)
{
    for (int i = 0; i < MAX_PROGRAMAS; i++)
    {
        if (self->mapa_programas[i] == NULL) continue;
        if (strcmp(nome, self->mapa_programas[i]->key) == 0) return self->mapa_programas[i]->value;
    }
    return NULL;
}

err_t mem_sec_le(mem_sec_t *self, int endereco, int *pvalor)
{
    return mem_le(self->mem, endereco, pvalor);
}

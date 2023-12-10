
#include "memoria.h"
#include "cpu.h"
#include "console.h"
#include "relogio.h"

typedef struct fila_processos fila_processos;
typedef struct no_processo no_processo;
typedef struct escalonador_t escalonador_t;

//Possívelmente adptável a receber multiplas filas diferentes, cada fila com uma prioridade
//Seria uma alternativa a usar prioridade nos nós, faria mais sentido.

//Poderia armazenar só o PID, mas to pensando que talvez seja interessante manter o estado do processo também.
struct no_processo
{
    double prioridade; //Não usado    
    processo* proc; //Processo representado pelo nó.

    no_processo* proximo_no;
};

struct fila_processos
{
    no_processo* raiz;
};

struct escalonador_t{
    fila_processos* fila_prontos;
    //fila_processos fila_nao_prontos;
};

escalonador_t* escalonador_cria(); //Inicializa o escalonador.

//Atua desconsiderando prioridades
void escalonador_enfila_processo(processo* p, escalonador_t* esc);          //Insere um elemento no final da fila.
processo* escalonador_desenfila_processo(escalonador_t* esc);  //Remove o primeiro elemento e retorna

//Considera prioridades, como o pop sempre tira o primeiro, caso os processos só sejam inseridos com prioridade.
//SEMPRE o processo com menor valor de prioridade será escolhido.
void escalonador_enfila_processo_prioridade(processo* p, escalonador_t* esc);

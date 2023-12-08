#include "so.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "processos.h"
#include "escalonador.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 20   // em instruções executadas
#define DEFAULT_QUANTUM_SIZE 5    //Define quanto cada processo recebe de quantums (interrupções de relogio)
#define TAMANHO_TABELA 10
#define TOTAL_TERMINAIS 4

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  console_t *console;
  relogio_t *relogio;

  escalonador_t* escalonador;
  int pid_atual;
  int processo_atual; // Se processo_atual = -1, entao nenhum processo esta sendo executado no momento
  processo* tab_processos[TAMANHO_TABELA];

  int uso_terminais[TOTAL_TERMINAIS];
};


// função de tratamento de interrupção (entrada no SO)
static err_t so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

// funções auxiliares gerais
static void reseta_processos(so_t *self);
static void libera_espera(so_t *self, processo* process);
static void libera_bloqueio(so_t *self, processo* process);
static processo* busca_processo(so_t *self, int pid);
static int busca_indice_processo(so_t *self, int pid);
static int encontra_terminal_livre(so_t *self);


// Se processo_atual = 0, entao nenhum processo esta sendo executado.



so_t *so_cria(cpu_t *cpu, mem_t *mem, console_t *console, relogio_t *relogio)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;

  self->escalonador = escalonador_cria();

  reseta_processos(self);

  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor, 
  //   salva seu estado à partir do endereço 0, e desvia para o endereço 10
  // colocamos no endereço 10 a instrução CHAMAC, que vai chamar 
  //   so_trata_interrupcao (conforme foi definido acima) e no endereço 11
  //   colocamos a instrução RETI, para que a CPU retorne da interrupção
  //   (recuperando seu estado no endereço 0) depois que o SO retornar de
  //   so_trata_interrupcao.
  mem_escreve(self->mem, 10, CHAMAC);
  mem_escreve(self->mem, 11, RETI);

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}


// Tratamento de interrupção

// funções auxiliares para tratar cada tipo de interrupção
static err_t so_trata_irq(so_t *self, int irq);
static err_t so_trata_irq_reset(so_t *self);
static err_t so_trata_irq_err_cpu(so_t *self);
static err_t so_trata_irq_relogio(so_t *self);
static err_t so_trata_irq_desconhecida(so_t *self, int irq);
static err_t so_trata_chamada_sistema(so_t *self);

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static void so_despacha(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC
// essa instrução só deve ser executada quando for tratar uma interrupção
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// na inicialização do SO é colocada no endereço 10 uma rotina que executa
//   CHAMAC; quando recebe uma interrupção, a CPU salva os registradores
//   no endereço 0, e desvia para o endereço 10
static err_t so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  err_t err;
  
  console_printf(self->console, "SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  
  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);
  
  // faz o atendimento da interrupção
  err = so_trata_irq(self, irq);
  
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  
  // escolhe o próximo processo a executar
  so_escalona(self);
  
  // recupera o estado do processo escolhido
  so_despacha(self);
  
  return err;
}

static void so_salva_estado_da_cpu(so_t *self)
{
  if (self->processo_atual == -1)
  {
    return;
  }
  processo* process = self->tab_processos[self->processo_atual];

  mem_le(self->mem, IRQ_END_PC, &(process->estado_cpu->PC));
  mem_le(self->mem, IRQ_END_A, &(process->estado_cpu->A));
  mem_le(self->mem, IRQ_END_X, &(process->estado_cpu->X));
  mem_le(self->mem, IRQ_END_complemento, &(process->estado_cpu->complemento));

  int err_int;
  int modo_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  mem_le(self->mem, IRQ_END_modo, &modo_int);

  err_t erro = err_int;
  cpu_modo_t modo = modo_int;
  process->estado_cpu->erro = erro;
  process->estado_cpu->modo = modo;
}
static void so_trata_pendencias(so_t *self)
{
  // realiza ações que não são diretamente ligadar com a interrupção que
  //   está sendo atendida:
  // - E/S pendente
  // - desbloqueio de processos
  // - contabilidades

  //console_printf(self->console, "A1");
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    //console_printf(self->console, "C0 %i", i);
    if (self->tab_processos[i] == NULL) continue;
    //console_printf(self->console, "C1");
    if (self->tab_processos[i]->estado_processo == WAITING) libera_espera(self, self->tab_processos[i]);
    //console_printf(self->console, "C2");
    if (self->tab_processos[i]->estado_processo == BLOCKED) libera_bloqueio(self, self->tab_processos[i]);
  }
  //console_printf(self->console, "A2");
}
static void so_escalona(so_t *self)
{
  
  if(self->processo_atual >= 0)
  {
    if((self->tab_processos[self->processo_atual])->quantum > 0)
      return;

    escalonador_enfila_processo(self->tab_processos[self->processo_atual], self->escalonador);
  }


  //Pede pro escalonador até achar um processo ready.
  //O escalonador assume que os processos na lista dele estao ready, mas na verdade ele guarda todos os processos.
  
  //console_printf(self->console, "Escalonador em ação");
  processo* processo_candidato = NULL; 
  processo_candidato = escalonador_desenfila_processo(self->escalonador);
  while(NULL != processo_candidato)
  {
      if (processo_candidato->estado_processo != READY)
      {        
        //console_printf(self->console, "Escalonador me retornou nulo, mas que filho da puta.");
        escalonador_enfila_processo(processo_candidato, self->escalonador);
        processo_candidato = escalonador_desenfila_processo(self->escalonador);
        continue;
      }

      self->processo_atual = busca_indice_processo(self, processo_candidato->pid);
      self->tab_processos[self->processo_atual]->quantum = DEFAULT_QUANTUM_SIZE;      

      //Tratar quando não existe ready.
      //Se chegou até aqui, consegui um processo ready.
      break;
  }
  console_printf(self->console, "\n Estamos sem processos. :C");
}
static void so_despacha(so_t *self)
{
  if (self->processo_atual == -1)
  {
    mem_escreve(self->mem, IRQ_END_erro, ERR_CPU_PARADA);
    return;
  }
  
  processo* process = self->tab_processos[self->processo_atual];

  mem_escreve(self->mem, IRQ_END_PC, process->estado_cpu->PC);
  mem_escreve(self->mem, IRQ_END_A, process->estado_cpu->A);
  mem_escreve(self->mem, IRQ_END_X, process->estado_cpu->X);
  mem_escreve(self->mem, IRQ_END_erro, process->estado_cpu->erro);
  mem_escreve(self->mem, IRQ_END_complemento, process->estado_cpu->complemento);
  mem_escreve(self->mem, IRQ_END_PC, process->estado_cpu->PC);
}

static err_t so_trata_irq(so_t *self, int irq)
{
  err_t err;
  console_printf(self->console, "SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  switch (irq) {
    case IRQ_RESET:
      err = so_trata_irq_reset(self);
      break;
    case IRQ_ERR_CPU:
      err = so_trata_irq_err_cpu(self);
      break;
    case IRQ_SISTEMA:
      err = so_trata_chamada_sistema(self);
      break;
    case IRQ_RELOGIO:
      err = so_trata_irq_relogio(self);
      break;
    default:
      err = so_trata_irq_desconhecida(self, irq);
  }
  return err;
}

static err_t so_trata_irq_reset(so_t *self)
{
  int ender = so_carrega_programa(self, "init.maq");
  if (ender != 100) {
    console_printf(self->console, "SO: problema na carga do programa inicial");
    return ERR_CPU_PARADA;
  }

  reseta_processos(self);
  self->processo_atual = 0;

  int terminal = encontra_terminal_livre(self);

  self->tab_processos[self->processo_atual] = cria_processo(ender, 0, 0, ERR_OK, 0, usuario, READY, self->pid_atual, terminal);
  processo* process = self->tab_processos[self->processo_atual];
  (self->uso_terminais[terminal])++;

  mem_escreve(self->mem, IRQ_END_PC, process->estado_cpu->PC);
  mem_escreve(self->mem, IRQ_END_modo, process->estado_cpu->modo);

  
  return ERR_OK;
}

// NAO ESTOU TRATANDO NADA!
// Isso aqui eh na magia thiago, nao sei oq q era pra ser feito.
// No momento eu so to matando o processo que causou a interrupcao,
// nao to nem tratando o erro que foi gerado.
// Tambem to retornando como ERR_OK, pois acredito que se o processo
// foi morto, a principio a CPU pode rodar sem problemas agora.
static err_t so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  // com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)

  processo* process = self->tab_processos[self->processo_atual];
  err_t err = process->estado_cpu->erro;
  console_printf(self->console,
      "SO: Erro na CPU: %s", err_nome(err));

  (self->uso_terminais[process->terminal])--;
  mata_processo(process);
  self->tab_processos[self->processo_atual] = NULL;
  self->processo_atual = -1;

  return ERR_OK;
}

static err_t so_trata_irq_relogio(so_t *self)
{
  // ocorreu uma interrupção do relógio
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  rel_escr(self->relogio, 3, 0); // desliga o sinalizador de interrupção
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);
  // trata a interrupção
  // por exemplo, decrementa o quantum do processo corrente, quando se tem
  if(self->processo_atual > -1)
  {    
    console_printf(self->console, "SO: interrupção do relógio, decrementando o quantum.");
    self->tab_processos[self->processo_atual]->quantum--;
  }
  // um escalonador com quantum
  console_printf(self->console, "SO: interrupção do relógio (não tratada)");
  return ERR_OK;
}

static err_t so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf(self->console,
      "SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  return ERR_CPU_PARADA;
}

// Chamadas de sistema

static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static err_t so_trata_chamada_sistema(so_t *self)
{
  int id_chamada = (self->tab_processos[self->processo_atual])->estado_cpu->A;
  
  console_printf(self->console, "SO: chamada de sistema %d", id_chamada);
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self);
      break;
    default:
      console_printf(self->console,
          "SO: chamada de sistema desconhecida (%d)", id_chamada);
      return ERR_CPU_PARADA;
  }
  return ERR_OK;
}

static void so_chamada_le(so_t *self)
{
  processo* process = self->tab_processos[self->processo_atual];
  int terminal_inicio = process->terminal * 4;

  int estado;
  term_le(self->console, terminal_inicio + 1, &estado);

  if (estado == 0)
  {
    process->estado_processo = BLOCKED;
    process->estado_cpu->A = -1;
    self->processo_atual = -1;
    return;
  }

  term_le(self->console, terminal_inicio, &(process->estado_cpu->A));
}

static void so_chamada_escr(so_t *self)
{
  
  processo* process = self->tab_processos[self->processo_atual];
  
  int terminal_inicio = process->terminal * 4;
  

  int estado;
  
  term_le(self->console, terminal_inicio + 3, &estado);
  
  
  if (estado == 0)
  {
    
    console_printf(self->console, "Processo %d bloqueado para escrita", process->pid);
    
    process->estado_processo = BLOCKED;
    
    process->estado_cpu->A = -1;
    
    self->processo_atual = -1;
    
    return;
  }


  term_escr(self->console, terminal_inicio + 2, process->estado_cpu->X);
  
  process->estado_cpu->A = 0;
  
}

static void so_chamada_cria_proc(so_t *self)
{
  self->pid_atual++;
  processo* process = self->tab_processos[self->processo_atual];

  // Encontra posicao na tabela de processos para colocar novo processo
  int posicao_processo = 0;
  while (posicao_processo < TAMANHO_TABELA && self->tab_processos[posicao_processo] != NULL) posicao_processo++;
  if (posicao_processo == TAMANHO_TABELA) return;

  // em X está o endereço onde está o nome do arquivo
  int ender_proc = process->estado_cpu->X;
  char nome[100];
  if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
    int ender_carga = so_carrega_programa(self, nome);
    if (ender_carga > 0) {
      int terminal = encontra_terminal_livre(self);
      (self->uso_terminais[terminal])++;

      self->tab_processos[posicao_processo] = cria_processo(ender_carga, 0, 0, ERR_OK, 0, usuario, READY, self->pid_atual, terminal);
      console_printf(self->console, "\nprintando!!!\n");
      escalonador_enfila_processo(self->tab_processos[posicao_processo], self->escalonador);
      process->estado_cpu->A = self->pid_atual;

      return;
    }
  }
  
  process->estado_cpu->A = -1;
}

static void so_chamada_mata_proc(so_t *self)
{
  processo* process = self->tab_processos[self->processo_atual];

  if (process->estado_cpu->X == 0)
  {
    (self->uso_terminais[process->terminal])--;

    mata_processo(self->tab_processos[self->processo_atual]);
    self->tab_processos[self->processo_atual] = NULL;
    self->processo_atual = -1;
    return;
  }

  int i = 0;
  while (i < TAMANHO_TABELA && (self->tab_processos[i])->pid != process->estado_cpu->X) i++;
  if (i == TAMANHO_TABELA) return;

  (self->uso_terminais[(self->tab_processos[i])->terminal])--;
  mata_processo(self->tab_processos[i]);
  self->tab_processos[i] = NULL;

  process->estado_cpu->A = 0;
}
static void so_chamada_espera_proc(so_t *self)
{
  processo* process = self->tab_processos[self->processo_atual];
  processo* processo_espera = busca_processo(self, process->estado_cpu->X);
  
  // Coloca o processo em estado de erro caso o processo a ser esperado nao exista
  if (processo_espera == NULL)
  {
    process->estado_cpu->A = -1; // Isso aqui era pra dar erro, mas nao ta acontecendo
    return;
  }
  
  self->processo_atual = -1;
  process->estado_cpu->A = 0;
  process->estado_processo = WAITING;
}


// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf(self->console,
        "Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf(self->console,
          "Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }
  prog_destroi(prog);
  console_printf(self->console,
      "SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não ascii na memória,
//   erro de acesso à memória)
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}


// Funcoes auxiliares gerais

static void reseta_processos(so_t *self)
{
  self->pid_atual = 1;
  self->processo_atual = -1;
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    self->tab_processos[i] = NULL;
  }
  for(int i = 0; i < TOTAL_TERMINAIS; i++)
  {
    self->uso_terminais[i] = 0;
  }
}

static void libera_espera(so_t *self, processo* process)
{
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    if (self->tab_processos[i] == NULL) continue;
    if ((self->tab_processos[i])->pid == process->estado_cpu->X) return;
  }
  process->estado_processo = READY;
}

// PROVAVELMENTE PRECISA SER REFATORADA!
// Somente processos bloqueados entraram nessa funcao.
// A gente tem q encontrar alguma forma de identificar o motivo
// do bloqueio de um processo, pra gente conseguir verificar se a causa
// desse bloqueio ja foi resolvida, e entao desbloquear ele.
//
// Uma ideia que eu tive é o SO armazenar os processos bloqueados
// junto com um identificador. Seria um vetor de pares (processo, bloqueio).
// Dessa forma, da pra identificar o que bloqueou o processo e tratar isso na hora
// de liberar ele. 
//
// Outra opcao eh adicionar mais um campo no processo, identificando porque ele foi bloqueado.
static void libera_bloqueio(so_t *self, processo* process)
{
  int inicio_terminal = process->terminal * 4;

  int estadoLeitura, estadoEscrita;
  term_le(self->console, inicio_terminal + 1, &estadoLeitura);
  term_le(self->console, inicio_terminal + 3, &estadoEscrita);

  // So esta sendo considerado o desbloqueio de processos que foram
  // bloqueados por nao conseguir escrever.
  // Se esse processo poder ser desbloqueado, entao eh necessario fazer a
  // escrita do caracter que tinha sido bloqueado, por isso term_escr eh chamado
  // dentro desse if.
  if (estadoEscrita != 0)
  {
    term_escr(self->console, inicio_terminal + 2, process->estado_cpu->X);
    process->estado_processo = READY;
    process->estado_cpu->A = 0;
  }
}


static processo* busca_processo(so_t *self, int pid)
{
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    if (self->tab_processos[i] == NULL) continue;
    if (self->tab_processos[i]->pid == pid) return self->tab_processos[i];
  }

  return NULL;
}

static int busca_indice_processo(so_t *self, int pid)
{
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    if (self->tab_processos[i] == NULL) continue;
    if (self->tab_processos[i]->pid == pid) return i;
  }

  return -1;
}

static int encontra_terminal_livre(so_t *self)
{
  int menor = self->uso_terminais[0];
  int idMenor = 0;
  for (int i = 1; i < TOTAL_TERMINAIS; i++)
  {
    if (self->uso_terminais[i] < menor)
    {
      idMenor = i;
      menor = self->uso_terminais[i];
    }
  }

  return idMenor;
}

#include "so.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "processos.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define TAMANHO_TABELA 10

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  console_t *console;
  relogio_t *relogio;

  int pid_atual;
  int processo_atual; // Se processo_atual = -1, entao nenhum processo esta sendo executado no momento
  processo* tab_processos[TAMANHO_TABELA];
};


// função de tratamento de interrupção (entrada no SO)
static err_t so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);


// Se processo_atual = 0, entao nenhum processo esta sendo executado.

so_t *so_cria(cpu_t *cpu, mem_t *mem, console_t *console, relogio_t *relogio)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->console = console;
  self->relogio = relogio;

  self->pid_atual = 1;
  self->processo_atual = -1;
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    self->tab_processos[i] = NULL;
  }

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
}
static void so_escalona(so_t *self)
{
  int menorPid = self->pid_atual + 1;
  int indexMenorPid = -1;

  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    if (self->tab_processos[i] == NULL) continue;
    if ((self->tab_processos[i])->estado_processo != READY) continue;
    if (self->tab_processos[i]->pid < menorPid)
    {
      menorPid = self->tab_processos[i]->pid;
      indexMenorPid = i;
    }
  }

  // significa q ele nao achou nenhum processo pronto para ser executado
  if (indexMenorPid == -1)
  {
    return;
  }

  
  self->processo_atual = indexMenorPid;
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

  self->pid_atual = 1;
  self->processo_atual = 0;
  for (int i = 0; i < TAMANHO_TABELA; i++)
  {
    self->tab_processos[i] = NULL;
  }

  self->tab_processos[self->processo_atual] = cria_processo(ender, 0, 0, ERR_OK, 0, usuario, READY, self->pid_atual);
  processo* process = self->tab_processos[self->processo_atual];

  mem_escreve(self->mem, IRQ_END_PC, process->estado_cpu->PC);
  mem_escreve(self->mem, IRQ_END_modo, process->estado_cpu->modo);
  return ERR_OK;
}

static err_t so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  // com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf(self->console,
      "SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  return ERR_CPU_PARADA;
}

static err_t so_trata_irq_relogio(so_t *self)
{
  // ocorreu uma interrupção do relógio
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  rel_escr(self->relogio, 3, 0); // desliga o sinalizador de interrupção
  rel_escr(self->relogio, 2, INTERVALO_INTERRUPCAO);
  // trata a interrupção
  // por exemplo, decrementa o quantum do processo corrente, quando se tem
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
  
  console_printf(self->console,
      "SO: chamada de sistema %d", id_chamada);
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
  // implementação com espera ocupada
  //   deveria bloquear o processo se leitura não disponível.
  //   no caso de bloqueio do processo, a leitura (e desbloqueio) deverá
  //   ser feita mais tarde, em tratamentos pendentes em outra interrupção,
  //   ou diretamente em uma interrupção específica do dispositivo, se for
  //   o caso
  // implementação lendo direto do terminal A
  //   deveria usar dispositivo corrente de entrada do processo
  for (;;) {
    int estado;
    term_le(self->console, 1, &estado);
    if (estado != 0) break;
    // como não está saindo do SO, o laço do processador não tá rodando
    // esta gambiarra faz o console andar
    // com a implementação de bloqueio de processo, esta gambiarra não
    //   deve mais existir.
    console_tictac(self->console);
    console_atualiza(self->console);
  }
  int dado;
  term_le(self->console, 0, &dado);
  // com processo, deveria escrever no reg A do processo
  mem_escreve(self->mem, IRQ_END_A, dado);
}

static void so_chamada_escr(so_t *self)
{
  // implementação com espera ocupada
  //   deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   deveria usar dispositivo corrente de saída do processo
  for (;;) {
    int estado;
    term_le(self->console, 3, &estado);
    if (estado != 0) break;
    // como não está saindo do SO, o laço do processador não tá rodando
    // esta gambiarra faz o console andar
    console_tictac(self->console);
    console_atualiza(self->console);
  }
  int dado;
  mem_le(self->mem, IRQ_END_X, &dado);
  term_escr(self->console, 2, dado);
  mem_escreve(self->mem, IRQ_END_A, 0);
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
      self->tab_processos[posicao_processo] = cria_processo(ender_carga, 0, 0, ERR_OK, 0, usuario, READY, self->pid_atual);
      process->estado_cpu->A = self->pid_atual;
      return;
    }
  }
  
  process->estado_cpu->A = -1;
}

static void so_chamada_mata_proc(so_t *self)
{
  processo* process = self->tab_processos[self->processo_atual];


  // Isso aq eu acho q tem q fazer diferente
  // Se tu olhar em so.h, o processo pode se matar, só q se ele se matar
  // vai dar segfault nas proximas etapas
  if (process->estado_cpu->X == 0)
  {
    mata_processo(self->tab_processos[self->processo_atual]);
    self->tab_processos[self->processo_atual] = NULL;
    self->processo_atual = -1;
    return;
  }

  int i = 0;
  while (i < TAMANHO_TABELA && (self->tab_processos[i])->pid != process->estado_cpu->X) i++;
  if (i == TAMANHO_TABELA) return;

  mata_processo(self->tab_processos[i]);
  self->tab_processos[i] = NULL;

  process->estado_cpu->A = 0;
}
static void so_chamada_espera_proc(so_t *self)
{
  processo* process = self->tab_processos[self->processo_atual];

  int i = 0;
  while (i < TAMANHO_TABELA && (self->tab_processos[i])->pid != process->estado_cpu->X) i++;
  
  // Coloca o processo em estado de erro caso o processo a ser esperado nao exista
  if (i == TAMANHO_TABELA)
  {
    process->estado_cpu->A = -1; // Isso aqui era pra dar erro, mas nao ta acontecendo
    return;
  }
  
  process->estado_cpu->A = 0;
  process->estado_processo = BLOCKED; // O estado desse processo provalvemente vai ter q ser WAITING, em vez de bloqueado
  // Para os processos em espera, verificar o estado_cpu->X dele, e ver se tem algum processo com esse pid em execucao
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

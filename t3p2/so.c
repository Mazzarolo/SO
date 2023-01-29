#include "so.h"
#include "tela.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "fila.h"

#define QUANTUM 20
#define MAX_PROCESSES 10
#define NUM_PROGRAMS 4
#define NONE -1
#define TICTIME 2
#define NUM_QUADROS 3
#define SIZE_QUADRO (MEM_TAM / NUM_QUADROS)

// Struct para quardar informações sobre os programas
typedef struct program
{
  int* instructions;
  int size;
} program;

// Struct para salvar os dados dos processos para printar as metricas
typedef struct process_data
{
  int processStartTime;
  int processFinishTime;
  int processCpuTime;
  int blockedTime;
  int waitingTime;
  int answerTime;
  int answerTimeStart;
  int answerTimeNum;
  int nBlock;
  int nPremp;
} process_data;

// Struct para quardar os dados da execução para o print das metricas
typedef struct exec_data
{
  int execTime;
  int cpuTime;
  int nInt;
} exec_data;

// Struct que quarda informações da memoria principal
typedef struct mem_info
{
  int processIdx;
  int pag;
  int usedTime;
} mem_info;

// struct base para a criação da tabela de processos

struct process {
  int key;
  program code;
  cpu_estado_t *cpu_state;
  process_state pross_state;
  mem_t *mem;
  tab_pag_t* tabPag;
  int killerDisp;
  acesso_t killerAcess;
  bool finished;
  int quantum;
  int creationTime;
  int stopingTime;
  int lastWaiting;
  process_data data;
};

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  process processes_table[MAX_PROCESSES];
  program* programs;
  int total_processes;
  exec_data data;
  Fila* processQuery;
  Fila* fifoPagesQuery;
  int numQuadros;
  int quadroFifo;
  FILE* fp;
  mem_info memInfo[NUM_QUADROS];
};

// funções auxiliares
static void init_mem(so_t *self);
static void panico(so_t *self);

// Função para printar a memoria
void printMem(so_t* self)
{
  int count = 0;
  for (int i = 0; i < mem_tam(contr_mem(self->contr)); i++)
  {
    int aux;
    mem_le(contr_mem(self->contr), i, &aux);
    fprintf(self->fp, "%d ", aux);
    if (count >= SIZE_QUADRO - 1)
    {
      fprintf(self->fp, "| ");
      count = 0;
    } else {
      count++;
    }   
  }
  fprintf(self->fp, "\n");
}

// Função para criar o SO
so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->total_processes = 0;
  self->data.cpuTime = 0;
  self->data.execTime = 0;
  self->data.nInt = 0;
  self->numQuadros = NUM_QUADROS;
  self->processQuery = fila_cria();
  self->fifoPagesQuery = fila_cria();
  self->quadroFifo = 0;

  for (int i = 0; i < NUM_QUADROS; i++)
  {
    self->memInfo[i].pag = NONE;
    self->memInfo[i].processIdx = NONE;
  }

  int prog00[] = {
  #include "init.maq"
  };

  int prog01[] = {
  #include "p1.maq"
  };

  int prog02[] = {
  #include "p2.maq"
  };

  int prog03[] = {
  #include "a1.maq"
  };

  self->programs = (program*) malloc(sizeof(program) * NUM_PROGRAMS);

  self->programs[0].instructions = (int*) malloc(sizeof(prog00));

  self->programs[0].size = sizeof(prog00) / sizeof(int);

  for (int i = 0; i < self->programs[0].size; i++)
  {
    self->programs[0].instructions[i] = prog00[i];
  }

  self->programs[1].instructions = (int*) malloc(sizeof(prog01));

  self->programs[1].size = sizeof(prog01) / sizeof(int);

  for (int i = 0; i < self->programs[1].size; i++)
  {
    self->programs[1].instructions[i] = prog01[i];
  }

  self->programs[2].instructions = (int*) malloc(sizeof(prog02));

  self->programs[2].size = sizeof(prog02) / sizeof(int);
  
  for (int i = 0; i < self->programs[2].size; i++)
  {
    self->programs[2].instructions[i] = prog02[i];
  }

  self->programs[3].instructions = (int*) malloc(sizeof(prog03));

  self->programs[3].size = sizeof(prog03) / sizeof(int);
  
  for (int i = 0; i < self->programs[3].size; i++)
  {
    self->programs[3].instructions[i] = prog03[i];
  }

  char *filename = "mem.txt";

  self->fp = fopen(filename, "w");

  fprintf(self->fp, "Memoria: \n\n"); 

  init_mem(self);

  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

// Função para verificar o processo em execução
static int verifyCurrentProcess (process* processes_table, int num_pross)
{
  for (int i = 0; i < num_pross; i++)
  {
    if (processes_table[i].pross_state == exec && !processes_table[i].finished)
      return i;
  }
  return NONE;
}

// trata chamadas de sistema

// chamada de sistema para leitura de E/S
// recebe em A a identificação do dispositivo
// retorna em X o valor lido
//            A o código de erro
static void so_trata_sisop_le(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  if (err == ERR_OK) {
    cpue_muda_X(self->cpue, val);
  } else {
    self->processes_table[idx].killerDisp = disp;
    self->processes_table[idx].killerAcess = leitura;
    cpue_muda_erro(self->cpue, err, 0);
  }

  cpue_muda_A(self->cpue, err);

  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  if(err != ERR_OK){
    self->processes_table[idx].killerDisp = disp;
    self->processes_table[idx].killerAcess = escrita;
    cpue_muda_erro(self->cpue, err, 0);
  }

  cpue_muda_A(self->cpue, err);

  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// Função do escalonador circular
static int escalonadorCircular(so_t* self)
{
  return fila_retira(self->processQuery);
}

// Função do escalonador Processo mais curto
static int escalonadorMaisCurto(so_t* self)
{
  int shorter = NONE;
  int shorterTime = 0;
  int mediumTime = 0;

  for (int i = 0; i < self->total_processes; i++)
  {
    if (self->processes_table[i].pross_state != blocked && !self->processes_table[i].finished) {
      mediumTime = (self->processes_table[i].lastWaiting + self->processes_table[i].stopingTime - self->processes_table[i].creationTime) / 2;
      if(mediumTime <= shorterTime || shorterTime == 0){
        shorterTime = mediumTime;
        shorter = i;
      }
    }
  }

  if(shorter != NONE)
    self->processes_table[shorter].lastWaiting = shorterTime;
  
  return shorter;
}

static void despacho(so_t *self)
{
  int idx = escalonadorCircular(self);

  if (idx != NONE)
  {
    self->processes_table[idx].pross_state = exec;

    self->processes_table[idx].quantum = QUANTUM;

    self->processes_table[idx].creationTime = rel_agora(contr_rel(self->contr));

    cpue_copia(self->processes_table[idx].cpu_state, self->cpue);

    mmu_usa_tab_pag(contr_mmu(self->contr), self->processes_table[idx].tabPag);
  } else {
    cpue_muda_modo(self->cpue, zumbi);
  }
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  free(self->processes_table[idx].code.instructions);

  free(self->processes_table[idx].cpu_state);

  self->processes_table[idx].finished = 1;    // exclusão lógica do processo da tabela

  self->processes_table[idx].data.processFinishTime = rel_agora(contr_rel(self->contr));

  despacho(self);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// Função para criar um novo processo
static void create_process(so_t *self, int progIdx, process_state state)
{
  int idx = self->total_processes;        // selecionando o indice do vetor de processos

  self->processes_table[idx].data.answerTime = 0;
  self->processes_table[idx].data.answerTimeNum = 0;
  self->processes_table[idx].data.blockedTime = 0;
  self->processes_table[idx].data.nBlock = 0;
  self->processes_table[idx].data.nPremp = 0;
  self->processes_table[idx].data.processCpuTime = 0;
  self->processes_table[idx].data.processStartTime = rel_agora(contr_rel(self->contr));
  self->processes_table[idx].data.waitingTime = 0;

  self->processes_table[idx].lastWaiting = QUANTUM;

  self->processes_table[idx].key = idx;

  self->processes_table[idx].pross_state = state;

  self->processes_table[idx].code.size = self->programs[progIdx].size;

  self->processes_table[idx].code.instructions = (int*) malloc (self->programs[progIdx].size * sizeof(int));

  for (int i = 0; i < self->programs[progIdx].size; i++)
  {
    self->processes_table[idx].code.instructions[i] = self->programs[progIdx].instructions[i];
  }

  self->processes_table[idx].cpu_state = cpue_cria();

  self->processes_table[idx].mem = mem_cria(self->processes_table[idx].code.size + SIZE_QUADRO);

  // colocando na memoria secundaria as instruções do programa:

  for (int i = 0; i < self->programs[progIdx].size; i++) {
    if (mem_escreve(self->processes_table[idx].mem, i, self->programs[progIdx].instructions[i]) != ERR_OK) {
      t_printf("so.init_mem: erro de memória, endereco %d\n", i);
      panico(self);
    }
  }

  int numPags = ceil(self->programs[progIdx].size / SIZE_QUADRO) + 1;

  self->processes_table[idx].tabPag = tab_pag_cria(numPags, SIZE_QUADRO);

  tab_pag_init(self->processes_table[idx].tabPag);

  self->processes_table[idx].killerDisp = NONE;  // o processo acabou de ser criado, ninguem o finalizou ainda

  self->processes_table[idx].finished = 0;        // o processo ainda não foi desativado

  self->total_processes += 1;
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  create_process(self, cpue_A(self->cpue), ready);

  fila_insere(self->processQuery, self->total_processes - 1);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  // o tipo de chamada está no "complemento" do cpue
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  so_chamada_t chamada = cpue_complemento(self->cpue);
  switch (chamada) {
    case SO_LE:
      so_trata_sisop_le(self);
      break;
    case SO_ESCR:
      so_trata_sisop_escr(self);
      break;
    case SO_FIM:
      so_trata_sisop_fim(self);
      break;
    case SO_CRIA:
      so_trata_sisop_cria(self);
      break;
    default:
      t_printf("so: chamada de sistema não reconhecida %d\n", chamada);
      panico(self);
  }
}

static void so_count_data(so_t *self)
{
  // somando 2 pois o trata tic é chamado a cada 2 tics de relogio 
  self->data.execTime += TICTIME;
  if (cpue_modo(self->cpue) != zumbi)
  {
    self->data.cpuTime += TICTIME;
  }
}

// Função para tratar as preempções
static void so_trata_premp(so_t *self)
{
  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  if(idx != NONE) {
  
    if(self->processes_table[idx].creationTime + self->processes_table[idx].quantum - rel_agora(contr_rel(self->contr)) <= 0) {

      self->processes_table[idx].data.nPremp += 1;

      self->processes_table[idx].pross_state = ready;

      exec_copia_estado(contr_exec(self->contr), self->processes_table[idx].cpu_state);

      fila_insere(self->processQuery, idx);     // usado pelo escalonador circular

      self->processes_table[idx].stopingTime = rel_agora(contr_rel(self->contr));

      cpue_muda_erro(self->cpue, ERR_OK, 0);  

      exec_altera_estado(contr_exec(self->contr), self->cpue);
    }
  }
}

// Função para printar as metricas do T2
static void printInfo(so_t* self){
  char *filename = "metricas/CircularQuantum32.txt";

  FILE *fp = fopen(filename, "w");

  fprintf(fp, "Métricas para o Escalonador Circular com Quantum = 32\n\n"); 

  fprintf(fp, "Execucao de %d ciclos de relogio\nTempo de CPU de %d ciclos de relogio\nNumero de Interrupcoes: %d", rel_agora(contr_rel(self->contr)), self->data.cpuTime, self->data.nInt);

  for (int i = 0; i < self->total_processes; i++)
  {
    fprintf(fp, "\n\nProcesso %d:\n\n", i);
    fprintf(fp, "Tempo de Retorno: %d\n", self->processes_table[i].data.processFinishTime - self->processes_table[i].data.processStartTime);
    fprintf(fp, "Tempo Total Bloqueado: %d\n", self->processes_table[i].data.blockedTime);
    fprintf(fp, "Tempo Total de CPU: %d\n", self->processes_table[i].data.processCpuTime);
    fprintf(fp, "Tempo Total de Espera: %d\n", self->processes_table[i].data.waitingTime);
    if(self->processes_table[i].data.answerTimeNum == 0)
      self->processes_table[i].data.answerTimeNum = 1;
    fprintf(fp, "Tempo de Resposta: %d\n", (self->processes_table[i].data.answerTime / self->processes_table[i].data.answerTimeNum));
    fprintf(fp, "Número de bloqueios: %d\n", self->processes_table[i].data.nBlock);
    fprintf(fp, "Número de preempcoes: %d", self->processes_table[i].data.nPremp);
  }

  fclose(fp);
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  for (int i = 0; i < self->total_processes; i++)
  {
    if(!self->processes_table[i].finished) {
      if(self->processes_table[i].pross_state == blocked)
        self->processes_table[i].data.blockedTime += TICTIME;

      if(self->processes_table[i].pross_state == exec)
        self->processes_table[i].data.processCpuTime += TICTIME;
      
      if(self->processes_table[i].pross_state == ready)
        self->processes_table[i].data.waitingTime += TICTIME;
    }

    if(self->processes_table[i].pross_state == blocked && es_pronto(contr_es(self->contr), self->processes_table[i].killerDisp, self->processes_table[i].killerAcess) && !self->processes_table[i].finished) {
      self->processes_table[i].pross_state = ready;
      fila_insere(self->processQuery, i);
      self->processes_table[i].data.answerTime += rel_agora(contr_rel(self->contr)) - self->processes_table[i].data.answerTimeStart;
      self->processes_table[i].data.answerTimeNum += 1;

      if(cpue_modo(self->cpue) == zumbi)  
      {
        cpue_muda_modo(self->cpue, supervisor); //retirando do modo zumbi

        cpue_muda_erro(self->cpue, ERR_OK, 0);

        exec_altera_estado(contr_exec(self->contr), self->cpue);
      }
    }
  }
  
  so_trata_premp(self);

  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  if(idx == NONE) //se não existirem processos executando, tenta chamar o despacho
  {    
    despacho(self);

    cpue_muda_erro(self->cpue, ERR_OK, 0);

    exec_altera_estado(contr_exec(self->contr), self->cpue);
  }
  
  bool desligar = true;

  for (int i = 0; i < self->total_processes; i++)
  {
    if (!self->processes_table[i].finished)
    {
      desligar = false;
    }
  }

  so_count_data(self);
  
  if(desligar) {
    // printInfo(self);
    t_printf("Sem mais processos, fim do Programa!");
    self->paniquei = true;
  }
}

// Função que trata bloqueio, quando encontra um dispositivo ocupado
static void so_trata_ocup(so_t *self)
{
  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  self->processes_table[idx].pross_state = blocked;
  cpue_copia(self->cpue, self->processes_table[idx].cpu_state);
  
  self->processes_table[idx].data.nBlock += 1;

  self->processes_table[idx].stopingTime = rel_agora(contr_rel(self->contr));

  self->processes_table[idx].data.answerTimeStart = rel_agora(contr_rel(self->contr));

  despacho(self);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// Algoritmo para escolha de substituição de paginas de maneira aleatoria
static int randSelectQuadro(so_t *self)
{
  for (int i = 0; i < NUM_QUADROS; i++)
    if(self->memInfo[i].processIdx == NONE)
      return i;
  
  return (rand() % NUM_QUADROS);
}

// Algoritmo para a escolha de substituição de paginas FIFO
static int fifoSelectQuadro(so_t *self)
{
  for (int i = 0; i < NUM_QUADROS; i++)
    if(self->memInfo[i].processIdx == NONE)
      return i;

  return fila_retira(self->fifoPagesQuery);
}

// Algoritmo para salvar um quadro da memoria principal na memoria secundária do processo
static void savePageContext(so_t *self, int quadro, int idx)
{
  mmu_usa_tab_pag(contr_mmu(self->contr), self->processes_table[self->memInfo[quadro].processIdx].tabPag);
  
  if(tab_pag_alterada(self->processes_table[self->memInfo[quadro].processIdx].tabPag, self->memInfo[quadro].pag))
  {
    for (int i = 0; i < SIZE_QUADRO; i++)
    {
      int val;
      mmu_le(contr_mmu(self->contr), self->memInfo[quadro].pag * SIZE_QUADRO + i, &val);
      mem_escreve(self->processes_table[self->memInfo[quadro].processIdx].mem, self->memInfo[quadro].pag * SIZE_QUADRO + i, val);
    }
  }

  tab_pag_muda_quadro(self->processes_table[self->memInfo[quadro].processIdx].tabPag, self->memInfo[quadro].pag, -1);
  tab_pag_muda_valida(self->processes_table[self->memInfo[quadro].processIdx].tabPag, self->memInfo[quadro].pag, false);

  mmu_usa_tab_pag(contr_mmu(self->contr), self->processes_table[idx].tabPag); 
}

// Função para tratar os erro de falpag
static void so_trata_falpag(so_t *self)
{
  int pc = mmu_ultimo_endereco(contr_mmu(self->contr));
  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  if(idx == NONE)
    return;

  int quadro = fifoSelectQuadro(self);

  fila_insere(self->fifoPagesQuery, quadro);

  int pag = pc / SIZE_QUADRO;

  savePageContext(self, quadro, idx);

  mmu_t *mmu = contr_mmu(self->contr);

  tab_pag_muda_quadro(self->processes_table[idx].tabPag, pag, quadro);
  tab_pag_muda_valida(self->processes_table[idx].tabPag, pag, true);

  self->memInfo[quadro].pag = pag;
  self->memInfo[quadro].processIdx = idx;

  for (int i = 0; i < SIZE_QUADRO; i++) {
    int val;
    mem_le(self->processes_table[idx].mem, pag * SIZE_QUADRO + i, &val);
    mmu_escreve(mmu, pag * SIZE_QUADRO + i, val);
  }

  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  printMem(self);
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    case ERR_OCUP:
      so_trata_ocup(self);
      break;
    case ERR_FALPAG:
      so_trata_falpag(self);
      break;
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      self->paniquei = true;
  }
  self->data.nInt += 1;
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

// carrega um programa na memória
static void init_mem(so_t *self)
{
  create_process(self, 0, exec);

  self->processes_table[self->total_processes - 1].creationTime = rel_agora(contr_rel(self->contr));

  self->processes_table[self->total_processes - 1].quantum = QUANTUM;

  mmu_usa_tab_pag(contr_mmu(self->contr), self->processes_table[0].tabPag);
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}

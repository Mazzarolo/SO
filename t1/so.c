#include "so.h"
#include "tela.h"
#include <stdlib.h>

#define MAX_PROCESSES 10
#define NUM_PROGRAMS 3
#define NONE -1

typedef struct program
{
  int* instructions;
  int size;
} program;


// struct base para a criação da tabela de processos

struct process {
  int key;
  program code;
  cpu_estado_t *cpu_state;
  process_state pross_state;
  mem_t *mem;
  int killerDisp;
  acesso_t killerAcess;
  bool finished;
};

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  process processes_table[MAX_PROCESSES];
  program* programs;
  int total_processes;
};

// funções auxiliares
static void init_mem(so_t *self);
static void panico(so_t *self);

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->total_processes = 0;

  int prog00[] = {
  #include "init.maq"
  };

  int prog01[] = {
  #include "p1.maq"
  };

  int prog02[] = {
  #include "p2.maq"
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

  init_mem(self);

  // coloca a CPU em modo usuário
  /*
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_modo(self->cpue, usuario);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  */
  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

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

static int escalonador(process* processes_table, int num_pross)
{
  for (int i = 0; i < num_pross; i++)
  {
    if (processes_table[i].pross_state == ready && !processes_table[i].finished)
      return i;
  }
  return NONE;
}

static void despacho(so_t *self)
{
  int idx = escalonador(self->processes_table, self->total_processes);

  if (idx != NONE)
  {
    self->processes_table[idx].pross_state = exec;

    cpue_copia(self->processes_table[idx].cpu_state, self->cpue);

    // inicializa a memória com o programa 
    mem_t *mem = contr_mem(self->contr);
    for (int i = 0; i < mem_tam(self->processes_table[idx].mem); i++) {
      int val;
      mem_le(self->processes_table[idx].mem, i, &val);
      mem_escreve(mem, i, val);
    }
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

  free(self->processes_table[idx].mem);

  self->processes_table[idx].finished = 1;    // exclusão lógica do processo da tabela

  despacho(self);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  int idx = self->total_processes;        // selecionando o indice do vetor de processos

  int progIdx = cpue_A(self->cpue);       // selecionando o indice que será usado no vetor de programas

  self->processes_table[idx].key = idx;

  self->processes_table[idx].pross_state = ready;

  self->processes_table[idx].code.size = self->programs[progIdx].size;

  self->processes_table[idx].code.instructions = (int*) malloc (self->programs[progIdx].size * sizeof(int));

  for (int i = 0; i < self->programs[progIdx].size; i++)
  {
    self->processes_table[idx].code.instructions[i] = self->programs[progIdx].instructions[i];
  }

  self->processes_table[idx].cpu_state = cpue_cria();

  self->processes_table[idx].mem = mem_cria(mem_tam(contr_mem(self->contr)));

  // colocando na memoria as instruções do programa:

  for (int i = 0; i < self->programs[progIdx].size; i++) {
    if (mem_escreve(self->processes_table[idx].mem, i, self->programs[progIdx].instructions[i]) != ERR_OK) {
      t_printf("so.init_mem: erro de memória, endereco %d\n", i);
      panico(self);
    }
  }

  self->processes_table[idx].killerDisp = NONE;  // o processo acabou de ser criado, ninguem o finalizou ainda

  self->processes_table[idx].finished = 0;        // o processo ainda não foi desativado

  self->total_processes += 1;

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

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  for (int i = 0; i < self->total_processes; i++)
  {
    if(self->processes_table[i].pross_state == blocked && es_pronto(contr_es(self->contr), self->processes_table[i].killerDisp, self->processes_table[i].killerAcess) && !self->processes_table[i].finished) {
      self->processes_table[i].pross_state = ready;

      if(cpue_modo(self->cpue) == zumbi)  
      {
        cpue_muda_modo(self->cpue, supervisor); //retirando do modo zumbi

        cpue_muda_erro(self->cpue, ERR_OK, 0);

        exec_altera_estado(contr_exec(self->contr), self->cpue);
      }
    }
  }

  if(verifyCurrentProcess(self->processes_table, self->total_processes) == NONE) //se não existirem processos executando, tenta chamar o despacho
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
  
  if(desligar) {
    t_printf("Sem mais processos, fim do Programa!");
    self->paniquei = true;
  }
}

static void so_trata_ocup(so_t *self)
{
  int idx = verifyCurrentProcess(self->processes_table, self->total_processes);

  self->processes_table[idx].pross_state = blocked;
  cpue_copia(self->cpue, self->processes_table[idx].cpu_state);
  for (int i = 0; i < mem_tam(self->processes_table[idx].mem); i++)
  {
    int val;
    mem_le(contr_mem(self->contr), i, &val);
    mem_escreve(self->processes_table[idx].mem, i, val);
  }
  
  despacho(self);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  exec_altera_estado(contr_exec(self->contr), self->cpue);
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
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      self->paniquei = true;
  }
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

static void first_process(so_t *self)
{
  int idx = self->total_processes;        // selecionando o indice do vetor de processos

  int progIdx = 0;                        // selecionando o indice que será usado no vetor de programas que também será o id unico do processo criado

  self->processes_table[idx].key = progIdx;

  self->processes_table[idx].pross_state = exec;

  self->processes_table[idx].code.size = self->programs[progIdx].size;

  self->processes_table[idx].code.instructions = (int*) malloc (self->programs[progIdx].size * sizeof(int));

  for (int i = 0; i < self->programs[progIdx].size; i++)
  {
    self->processes_table[idx].code.instructions[i] = self->programs[progIdx].instructions[i];
  }

  self->processes_table[idx].cpu_state = cpue_cria();

  self->processes_table[idx].mem = mem_cria(mem_tam(contr_mem(self->contr)));

  for (int i = 0; i < mem_tam(self->processes_table[idx].mem); i++)
  {
    int val;
    mem_le(contr_mem(self->contr), i, &val);
    mem_escreve(self->processes_table[idx].mem, i, val);
  }

  self->processes_table[idx].killerDisp = NONE;  // o processo acabou de ser criado, ninguem o finalizou ainda

  self->processes_table[idx].finished = 0;        // o processo ainda não foi desativado

  self->total_processes += 1;
}

// carrega um programa na memória
static void init_mem(so_t *self)
{
  first_process(self);

  // inicializa a memória com o programa 
  mem_t *mem = contr_mem(self->contr);
  for (int i = 0; i < self->programs[0].size; i++) {
    if (mem_escreve(mem, i, self->programs[0].instructions[i]) != ERR_OK) {
      t_printf("so.init_mem: erro de memória, endereco %d\n", i);
      panico(self);
    }
  }
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}

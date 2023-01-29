#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fila.h"

struct no {
   int info;
   struct no* prox;
};
typedef struct no No;

struct fila {
   No* ini;
   No* fim;
};


Fila* fila_cria (void)
{
   Fila* f = (Fila*) malloc(sizeof(Fila));
   f->ini = NULL;
   f->fim = NULL;
   return f;
}

int fila_vazia (Fila* f)
{
    return (f->ini==NULL);
}


void fila_insere (Fila* f, int v)
{
   No* n = (No*) malloc(sizeof (No));
   n->info = v;              
   n->prox = NULL;      
   if (f->fim != NULL)    
      f->fim->prox = n;
   else                       
      f->ini = n;
   f->fim = n;               
}


int fila_retira (Fila* f){
   if (fila_vazia(f)) {
      return -1;                  
   }

   No* t = f->ini;
   int v = t->info;
   f->ini = t->prox;
   if (f->ini == NULL)
      f->fim = NULL;
   free(t);
   return v;
}


void fila_libera (Fila* f)
{
   No* q = f->ini;
   while (q!=NULL) {
      No* t = q->prox;
      free(q);
      q = t;
   }
   free(f);
}

int fila_tamanho(Fila* f)
{
   No * no = f->ini;
   int cont = 0;
   while (no!=NULL) {
      no = no->prox;
      cont++;
   }
   return cont;
}

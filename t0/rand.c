#include "rand.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

struct rand_t {
    int max;
};

rand_t *rand_cria(int max)
{
    srand(time(NULL));
    rand_t *self = (rand_t*) malloc (sizeof(rand_t));
    self->max = max + 1;  // + 1 para permitir que o valor maximo passado pelo usuario seja inclusivo
    return self;
}

void rand_destroi(rand_t *self)
{
    free(self);
    return;
}

err_t rand_le(void *disp, int id, int *pvalor)
{
    int max = ((rand_t*)disp)->max;
    (*pvalor) = rand() % max;
    return ERR_OK;
}
#ifndef RAND_H
#define RAND_H

#include "es.h"

// Dispositivo de numeros aleatorios

typedef struct rand_t rand_t;

// cria e inicializa um dispositivo de numeros aleatorios
// retorna NULL em caso de erro
rand_t *rand_cria(int max);

// destrói um dispositivo de numeros aleatorios
// nenhum outro numero aleatorio pode ser lido deste dispositivo depois desta chamada
void rand_destroi(rand_t *self);

// Funcao para implementar o protocolo de acesso a um dispositivo pelo
//   controlador de E/S
err_t rand_le(void *disp, int id, int *pvalor);

#endif // RAND_H

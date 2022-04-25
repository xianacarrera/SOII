#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>

#define MAX_BUFFER 5 /* tamaño del buffer */
#define DATOS_A_PRODUCIR 100 /* número de datos a producir/consumir */

size_t tam_msg;

/* cola de entrada de mensajes para el productor */
mqd_t buz_ordenes;
/* cola de entrada de mensajes para el consumidor */
mqd_t buz_items;

char cola_final[DATOS_A_PRODUCIR];


void consumir_item(char item, int iter){
    printf("[ITER %02d] Leido item recibido: %c\n", iter, item);
    cola_final[iter] = item;
}

void imprimir_cola_final(){
    int i, j;

    for (i = 0; i < DATOS_A_PRODUCIR; i += 10){
        printf("ITER -> ");
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf("%02d ", j);
        printf("\nITEM -> ");
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf(" %c ", cola_final[j]);
        printf("\n\n");
    }
}

void consumidor(void) {
    char item = ' ';
    int i;

    for (i = 0; i < MAX_BUFFER; i++) mq_send(buz_ordenes, &item, tam_msg, 0);

    printf("Órdenes enviadas\n");

    for (i = 0; i < DATOS_A_PRODUCIR; i++){
        // Retira el mensaje más antiguo de buz_ y lo almacena en m
        // Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal
        mq_receive(buz_items, &item, tam_msg, 0);
        printf("[ITER %02d] Recibido item\n", i);
        mq_send(buz_ordenes, &item, tam_msg, 0);
        printf("[ITER %02d] Enviada petición de un nuevo item\n", i);
        consumir_item(item, i);
    }

    printf("\n\n\n");
    printf("Finalizados envíos y recepciones. Cola de items consumidos:\n");
    imprimir_cola_final();
}




    //printf("   ITER    |  ITEM |\n");
    //for (i = 0; i < DATOS_A_PRODUCIR; i++) printf("    %02d  -> |   %c   |\n", i, cola_final[i]);



int main() {
    tam_msg = sizeof(char);

    /* Apertura de los buffers */
    buz_ordenes = mq_open("/BUZON_ORDENES", O_WRONLY);
    buz_items = mq_open("/BUZON_ITEMS", O_RDONLY);

    if ((buz_ordenes == -1) || (buz_items == -1)) {
        perror ("mq_open");
        exit(EXIT_FAILURE);
    }

    consumidor();
    mq_close(buz_ordenes);
    mq_close(buz_items);

    exit(EXIT_SUCCESS);
}

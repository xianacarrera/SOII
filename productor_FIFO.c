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

/*
 * DUDA: PRIORIDAD???
 */

char producir_elemento(int iter){
    char item;

    // pos es la iteración en la que se encuentra el productor
    printf("[ITER %02d] Recibida orden\n", iter);
    item = 'a' + (iter % MAX_BUFFER);
    cola_final[iter] = item;
    return item;
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

void productor(void) {
    char item;
    int i;

    for (i = 0; i < DATOS_A_PRODUCIR; i++){
        // item = producir_elemento(0);
        // Retira el mensaje más antiguo de buz_ordenes y lo almacena en m
        // Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal

        mq_receive(buz_ordenes, &item, tam_msg, 0);
        item = producir_elemento(i);
        mq_send(buz_items, &item, tam_msg, 0);
        printf("[ITER %02d] Enviado item %c\n", i, item);
    }

    printf("\n\n\n");
    printf("Finalizados envíos y recepciones. Cola de items producidos:\n");
    imprimir_cola_final();
}


int main() {
    struct mq_attr attr; /* Atributos de la cola */

    tam_msg = sizeof(char);

    attr.mq_maxmsg = MAX_BUFFER;
    attr.mq_msgsize = tam_msg;

    /* Borrado de los buffers de entrada
    por si existían de una ejecución previa*/
    mq_unlink("/BUZON_ORDENES");
    mq_unlink("/BUZON_ITEMS");

    /* Apertura de los buffers */
    buz_ordenes = mq_open("/BUZON_ORDENES", O_CREAT|O_RDONLY, 0777, &attr);
    buz_items = mq_open("/BUZON_ITEMS", O_CREAT|O_WRONLY, 0777, &attr);

    if ((buz_ordenes == -1) || (buz_items == -1)) {
        perror ("mq_open");
        exit(EXIT_FAILURE);
    }

    productor();
    mq_close(buz_ordenes);
    mq_close(buz_items);

    exit(EXIT_SUCCESS);
}

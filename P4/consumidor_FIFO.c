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


/*
 * DUDA: PRIORIDAD???
 */

void consumir_item(char item){
    printf("Consumido item %c\n", item);
}

void consumidor(void) {
    char item;
    int i;

    for (i = 0; i < MAX_BUFFER; i++) mq_send(buz_ordenes, &item, tam_msg, 0);

    printf("Órdenes enviadas\n");

    while (1){
        // Retira el mensaje más antiguo de buz_ y lo almacena en m
        // Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal
        mq_receive(buz_items, &item, tam_msg, 0);
        printf("Ostias qué pizza cojonuda me estoy tomando en el restaurante Luna Rosa\n");
        mq_send(buz_ordenes, &item, tam_msg, 0);
        printf("Deme otra de sus pizzas cojonudas buen señor\n");
        consumir_item(item);
    }
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
    buz_ordenes = mq_open("/BUZON_ORDENES", O_CREAT|O_WRONLY, 0777, &attr);
    buz_items = mq_open("/BUZON_ITEMS", O_CREAT|O_RDONLY, 0777, &attr);

    if ((buz_ordenes == -1) || (buz_items == -1)) {
        perror ("mq_open");
        exit(EXIT_FAILURE);
    }

    consumidor();
    mq_close(buz_ordenes);
    mq_close(buz_items);

    exit(EXIT_SUCCESS);
}

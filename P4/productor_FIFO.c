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

char producir_elemento(int pos){
    return 'a' + (pos % 26);
}

void productor(void) {
    char item;

    while (1){
        //item = producir_elemento(0);
        // Retira el mensaje más antiguo de buz_ordenes y lo almacena en m
        // Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal

        printf("hola\n");
        mq_receive(buz_ordenes, &item, tam_msg, 0);
        printf("Oído cocina\n");
        mq_send(buz_items, &item, tam_msg, 0);
        printf("Tome su pizza con piña\n");
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

#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>

#define AZUL "\033[0;34m"
#define ROJO "\033[0;31m"
#define RESET "\033[0m"

#define MAX_BUFFER 5 /* tamaño del buffer */
#define DATOS_A_PRODUCIR 100 /* número de datos a producir/consumir */

size_t tam_msg;

/* cola de entrada de mensajes para el productor */
mqd_t buz_ordenes;
/* cola de entrada de mensajes para el consumidor */
mqd_t buz_items;

char consumiciones[DATOS_A_PRODUCIR];
int prioridades[DATOS_A_PRODUCIR];


void consumir_item(char item, int iter, int prio){
    printf("[ITER %02d] Leido item recibido: %c\n", iter, item);
    consumiciones[iter] = item;
    prioridades[iter] = prio;
}

void imprimir_consumiciones(){
    int i, j;
    char * color;

    for (i = 0; i < DATOS_A_PRODUCIR; i += 10){
        printf("ITER -> ");
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf("%02d ", j);
        printf("\nITEM -> ");
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf(" %c ", consumiciones[j]);
        printf("\nPRIO -> ");
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++){
            
            // No hay dos items con la misma prioridad
            if (j == 0) color = RESET;
            else color = prioridades[j] > prioridades[j-1]? ROJO : AZUL;

            printf("%s%02d%s ", color, prioridades[j], RESET);
        }
        printf("\n\n");
    }
}

void consumidor(void) {
    char item = ' ';
    int i;
    unsigned int prio;

    for (i = 0; i < MAX_BUFFER; i++) mq_send(buz_ordenes, &item, tam_msg, 0);

    printf("Órdenes enviadas\n");

    for (i = 0; i < DATOS_A_PRODUCIR; i++){
        // Retira el mensaje más antiguo de buz_ y lo almacena en m
        // Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal
        mq_receive(buz_items, &item, tam_msg, &prio);
        printf("[ITER %02d] Recibido item de prioridad %u\n", i, prio);
        mq_send(buz_ordenes, &item, tam_msg, 0);
        printf("[ITER %02d] Enviada petición de un nuevo item\n", i);
        consumir_item(item, i, prio);
    }

    printf("\n\n\n");
    printf("Finalizados envíos y recepciones. Items consumidos:\n");
    imprimir_consumiciones();
}




    //printf("   ITER    |  ITEM |\n");
    //for (i = 0; i < DATOS_A_PRODUCIR; i++) printf("    %02d  -> |   %c   |\n", i, consumiciones[i]);



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

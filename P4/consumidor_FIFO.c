#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>

/* Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 4 - Consumidor FIFO
 *
 * Este programa implementa una solución al problema del productor-consumidor empleando el mecanismo de pase de
 * mensajes.
 * En particular, este codigo corresponde al consumidor en una version en la que el consumidor retira los items del 
 * buffer en orden Firt-In-First-Out. 
 * 
 * Para compilar se debe usar la opción -lrt.
 * El productor debe comezar a ejecutarse antes del consumidor.
 */


#define MAX_BUFFER 5                         // Tamaño del buffer
#define DATOS_A_CONSUMIR 100                 // Número de datos a producir/consumir


mqd_t buz_ordenes;                   // Cola de entrada de mensajes para el productor
mqd_t buz_items;                     // Cola de entrada de mensajes para el consumidor

size_t tam_msg;                      // Tamaño de cada mensaje 

char cola_final[DATOS_A_CONSUMIR];   // Historial de mensajes recibidos

void consumir_item(char item, int iter);        // Funcion de consumicion de mensajes
void consumidor();                   // Función que implementa el consumidor

void imprimir_cola_final();         // Función para la impresión del historial


int main() {
    tam_msg = sizeof(char);         // Cada mensaje contendrá un carácter

    // Se abren los buffers de recepción del productor y del consumidor, respectivamente.
    buz_ordenes = mq_open("/BUZON_ORDENES", O_WRONLY);      // En el buffer de ordenes, el consumidor solo escribe.
    buz_items = mq_open("/BUZON_ITEMS", O_RDONLY);          // En el buffer de ordenes, el consumidor solo lee.

    if ((buz_ordenes == -1) || (buz_items == -1)) {     // Error en la apertura de algún buffer
        perror("No se ha podido abrir los buffers del programa");
        exit(EXIT_FAILURE);    
    }

    consumidor();                 // Bucle principal del consumidor

    if (mq_close(buz_ordenes) || mq_close(buz_items)){
        perror("Error al cerrar los buffers del programa");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

/* Función con la que se consumen (se imprimen) los mensajes recibidos. 
 * Se imprime su contenido y la iteración actual, y se guarda una referencia en el historial para uso posterior.
 * @param item: mensaje recibido.
 * @param iter: iteración actual.
 */
void consumir_item(char item, int iter){
    printf("[ITER %02d] Leido item recibido: %c\n", iter, item);            // Imprime el mensaje recibido
    cola_final[iter] = item;    // Guarda una referencia en el historial de mensajes
}

/* Función que muestra todos los mensajes recibidos por el consumidor a lo largo del programa, para facilitar la
 * comprobación de la validez del resultado. 
 */
void imprimir_cola_final(){
    int i, j;

    // Se imprime el historial en líneas de 10 mensajes
    for (i = 0; i < DATOS_A_CONSUMIR; i += 10){
        printf("ITER -> ");         // Título de la línea (iteración)
        // En la condicion de finalizacion se comprueba que j no alcance el tamaño del historial
        for (j = i; j < i + 10 && j < DATOS_A_CONSUMIR; j++) printf("%02d ", j);
        
        printf("\nITEM -> ");       // Contenido de la línea (mensaje)
        for (j = i; j < i + 10 && j < DATOS_A_CONSUMIR; j++) printf(" %c ", cola_final[j]);
        
        printf("\n\n");
    }
}

/*
 * Función principal del consumidor. 
 * En primer lugar, llena el buffer del productor enviando MAX_BUFFER mensajes.
 * Luego, entra en un bucle de procesado de mensajes de DATOS_A_CONSUMIR iteraciones.
 */
void consumidor() {
    // Item vacío. Con él, el consumidor indica al productor que está preparado para recibir un nuevo mensaje
    char item = ' ';    
    int i;          // Variable de iteración

    /* Se envían MAX_BUFFER mensajes al buffer buz_ordenes (buffer de lectura del productor).
     * Los argumentos de la función mq_send son:
     * - Cola a donde se enviará el mensaje
     * - Puntero al mensaje
     * - Tamaño del mensaje
     * - Prioridad del mensaje
     * El consumidor siempre usará prioridad 0 en sus mensajes (ya que el contenido es irrelevante: son mensajes vacíos 
     * que únicamente sirven de indicación al productor de que hay espacio en buz_items).
     */
    for (i = 0; i < MAX_BUFFER; i++) mq_send(buz_ordenes, &item, tam_msg, 0);
    printf("Ordenes enviadas. Se ha llenado el buffer del productor\n");

    // En cada iteración del bucle principal, se recibe un mensaje enviado por el productor, se le devuelve el item
    // vacío (como señal de que hay hueco en el buffer del consumidor para más items) y se procesa el mensaje recibido.
    for (i = 0; i < DATOS_A_CONSUMIR; i++){
        // Con mq_receive se retira el mensaje más antiguo de buz_items (pues el productor tampoco usa prioridades),
        // y se almacena en item. El tamaño es el mismo que el de los mensajes enviado por el consumidor.
        // La prioridad del mensaje recibido se guardaría en el cuarto argumento. La ignoramos (NULL).
        // Si no hay mensajes, el consumidor se bloquea hasta que llege uno o lo despierte una señal.
        mq_receive(buz_items, &item, tam_msg, NULL);
        printf("[ITER %02d] Recibido item\n", i);       // Se notifica la recepción
        mq_send(buz_ordenes, &item, tam_msg, 0);        // Se devuelve el item vacío al productor
        printf("[ITER %02d] Enviada petición de un nuevo item\n", i);
        consumir_item(item, i);         // Se imprime el mensaje y se guarda en un historial
    }

    printf("\n\n\n");
    // Al acabar, el consumidor imprime todo el historial de mensajes en orden.
    printf("Finalizados envíos y recepciones. Cola de items consumidos:\n");
    imprimir_cola_final();
}

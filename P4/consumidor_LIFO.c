#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>

// Colores para mostrar la evolución de las prioridades de los mensajes
#define AZUL "\033[0;34m"
#define ROJO "\033[0;31m"
#define RESET "\033[0m"

/* Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 4 - Consumidor LIFO
 *
 * Este programa implementa una solución al problema del productor-consumidor empleando el mecanismo de pase de
 * mensajes.
 * En particular, este codigo corresponde al consumidor en una version en la que el consumidor retira los items del 
 * buffer en orden Last-In-First-Out. 
 * 
 * Para compilar se debe usar la opción -lrt.
 * El productor debe comezar a ejecutarse antes del consumidor.
 */


#define MAX_BUFFER 5                         // Tamaño del buffer
#define DATOS_A_CONSUMIR 100                 // Número de datos a producir/consumir


mqd_t buz_ordenes;                   // Pila de entrada de mensajes para el productor
mqd_t buz_items;                     // Pila de entrada de mensajes para el consumidor

size_t tam_msg;                      // Tamaño de cada mensaje 

char consumiciones[DATOS_A_CONSUMIR];           // Historial de mensajes consumidos
int prioridades[DATOS_A_CONSUMIR];              // Historial de la prioridad asociada a cada mensaje consumido


long buzon_tiene_elementos(char buffer){
    struct mq_attr attr;

    switch(buffer){
        case 'P':
            mq_getattr(buz_ordenes, &attr);
            return attr.mq_curmsgs; 
        case 'C':
            mq_getattr(buz_items, &attr);
            return attr.mq_curmsgs; 
    }
    return -1;
}


/* Función con la que se consumen (se imprimen) los mensajes recibidos. 
 * Se imprime su contenido y la iteración actual, y se guarda una referencia en el historial para uso posterior.
 * @param item: mensaje recibido.
 * @param iter: iteración actual.
 * @param prioridad: prioridad asociada al mensaje.
 */
void consumir_item(char item, int iter, int prio){
    printf("[ITER %02d] Consumido item %c con prioridad %d\n", iter, item, prio);
    consumiciones[iter] = item;         // Se guarda el contenido del mensaje
    prioridades[iter] = prio;           // Se guarda la prioridad asociada al mensaje
}

/* Función que muestra todos los mensajes recibidos por el consumidor a lo largo del programa, para facilitar la
 * comprobación de la validez del resultado. Se indican también las prioridades de los mensajes utilizando el siguiente
 * código de colores:
 * - Color predeterminado de la consola: primer mensaje 
 * - Azul: prioridad menor que el item anterior
 * - Rojo: prioridad mayor que el item anterior
 */
void imprimir_historial_buzon(){
    int i, j;
    char * color;

    // Se imprime el historial en líneas de 10 mensajes
    for (i = 0; i < DATOS_A_CONSUMIR; i += 10){
        printf("ITER -> ");         // Título de la línea (iteración)
        // En la condicion de finalizacion se comprueba que j no alcance el tamaño del historial
        for (j = i; j < i + 10 && j < DATOS_A_CONSUMIR; j++) printf("%02d ", j);

        printf("\nITEM -> ");       // Contenido del mensaje
        for (j = i; j < i + 10 && j < DATOS_A_CONSUMIR; j++) printf(" %c ", consumiciones[j]);

        printf("\nPRIO -> ");       // Prioridad del mensaje
        for (j = i; j < i + 10 && j < DATOS_A_CONSUMIR; j++){
            
            // No hay dos items con la misma prioridad por construcción del código
            if (j == 0) color = RESET;      // Color predeterminado
            // Si la prioridad ha aumentado con respecto al anterior mensaje, se usa rojo. Si no, azul.
            else color = prioridades[j] > prioridades[j-1]? ROJO : AZUL;

            printf("%s%02d%s ", color, prioridades[j], RESET);
        }
        printf("\n\n");     // Separación entre bloques
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
    int i;                      // Variable de iteración
    unsigned int prio;          // Prioridad de los mensajes recibidos

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

    for (i = 0; i < DATOS_A_CONSUMIR; i++){
        /* Con mq_receive se retira el mensaje de mayor prioridad que haya llegado a buz_items. En caso de empate, se 
         * tomaría el más antiguo, pero por la implementación usada en el productor, todos los items tendrán una
         * prioridad distinta (e igual a la correspondiente iteración en la que se encuentre el productor).
         * Nótese que el orden en el que los mensajes llegan al consumidor no tiene por qué corresponderse con el
         * orden en el que el productor los envía.
         * El contenido del item, que tiene tamaño tam_msg, se almacena en la variable item.
         * La prioridad se guarda en prio.
         *
         * Si no había mensajes en buz_items, el consumidor se bloquea hasta que llege uno o lo despierte una señal.
         */
        mq_receive(buz_items, &item, tam_msg, &prio);
        printf("[ITER %02d] Recibido item\n", i);
        mq_send(buz_ordenes, &item, tam_msg, 0);   // Se devuelve el item al productor
        // El contenido del item no se modifica porque es irrelevante. El productor no lo leerá
        printf("[ITER %02d] Enviada petición de un nuevo item\n", i);
        consumir_item(item, i, prio);               // Se imprime el mensaje y se guarda en un historial      
    }

    printf("\n\n\n");

    // Al acabar, el consumidor imprime todo el historial de mensajes en orden.
    printf("Finalizados envios y recepciones. Cola de items consumidos:\n");
    imprimir_historial_buzon();

    if (buzon_tiene_elementos('C')) printf("\n\nLa cola de entrada del consumidor no esta vacia\n\n");
    while (buzon_tiene_elementos('C')){
        mq_receive(buz_items, &item, tam_msg, NULL);
        printf("Recogido item de la cola de entrada del consumidor\n");
    }
}


int main() {
    tam_msg = sizeof(char);         // Cada mensaje contendrá un carácter

    // Se abren los buffers de recepción del productor y del consumidor, respectivamente.ç
    // Ambos fueron previamente creados por el productor.
    buz_ordenes = mq_open("/BUZON_ORDENES", O_WRONLY);      // En el buffer de ordenes, el consumidor solo escribe.
    buz_items = mq_open("/BUZON_ITEMS", O_RDONLY);          // En el buffer de ordenes, el consumidor solo lee.

    if ((buz_ordenes == -1) || (buz_items == -1)) {     // Error en la apertura de algún buffer
        perror("No se ha podido abrir los buffers del programa");
        exit(EXIT_FAILURE);    
    }

    consumidor();                 // Bucle principal del consumidor

    // El consumidor cierra ambos buzones (el productor los cierra y elimina)
    if (mq_close(buz_ordenes) || mq_close(buz_items)){
        perror("Error al cerrar los buffers del programa");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

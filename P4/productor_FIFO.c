#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>

/* Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 4 - Productor FIFO
 *
 * Este programa implementa una solución al problema del productor-consumidor empleando el mecanismo de pase de
 * mensajes.
 * En particular, este codigo corresponde al productor en una version en la que el consumidor retira los items del 
 * buffer en orden Firt-In-First-Out.  
 * 
 * Para compilar se debe usar la opción -lrt.
 * El productor debe comezar a ejecutarse antes del consumidor.
 */

#define MAX_BUFFER 5                         // Tamaño del buffer
#define DATOS_A_PRODUCIR 100                 // Número de datos a producir/consumir


mqd_t buz_ordenes;                   // Cola de entrada de mensajes para el productor
mqd_t buz_items;                     // Cola de entrada de mensajes para el consumidor

size_t tam_msg;                      // Tamaño de cada mensaje 

char cola_final[DATOS_A_PRODUCIR];   // Historial de mensajes enviados

char producir_elemento(int iter);   // Función que genera un nuevo elemento
void productor();                   // Función que implementa el productor

void imprimir_cola_final();         // Función para la impresión del historial

int main() {
    struct mq_attr attr;            // Atributos de la cola

    // El productor se encarga de crear las colas de ambos programas. El consumidor únicamente tendrá que abrirlas 
    // (deberá comenzar a ejecutarse después del productor).

    tam_msg = sizeof(char);           // Los mensajes serán de un solo carácter	

    attr.mq_maxmsg = MAX_BUFFER;      // Número máximo de mensajes en los buffers
    attr.mq_msgsize = tam_msg;        // Tamaño de cada mensaje

    // SE borran los buffers de entrada por si ya existían debido a una ejecución previa
    if (mq_unlink("/BUZON_ORDENES") || mq_unlink("/BUZON_ITEMS")){
        perror("Error al borrar los buffers de entrada");
        exit(EXIT_FAILURE);
    }

    // Se crean y abren los buffers de entrada del productor y del consumidor, respectivamente. Se conceden todos
    // los permisos (777) y se utiliza la configuración establecida a través de attr.
    // El productor escribirá en en el segundo y el consumidor en el primero. Por tanto, el productor los abre con
    // permisos de solo lectura y solo escritura, respectivamente.
    buz_ordenes = mq_open("/BUZON_ORDENES", O_CREAT|O_RDONLY, 0777, &attr);
    buz_items = mq_open("/BUZON_ITEMS", O_CREAT|O_WRONLY, 0777, &attr);

    if ((buz_ordenes == -1) || (buz_items == -1)) {
        perror ("Error - no es ha podido crear los buffers de entrada");
        exit(EXIT_FAILURE);
    }

    productor();        // Funcion principal del productor

    if (mq_close(buz_ordenes) || mq_close(buz_items)){
        perror("Error al cerrar los buffers del programa");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}


/* Función a través de la cual el productor genera un mensaje a enviar al consumidor.
 * El contenido del mensaje se calcula en función de la iteración actual del proceso.
 * @param iter: iteración actual.
 * @return: mensaje a enviar.
 */
char producir_elemento(int iter){
    char item;              // Item a enviar

    // Tras recibir una orden (una indicación de que el consumidor tiene slots vacíos en su buffer de entrada),
    // el productor genera un nuevo mensaje.
    printf("[ITER %02d] Recibida orden\n", iter);
    item = 'a' + (iter % MAX_BUFFER);           // Con MAX_BUFFER = 5, el mensaje será 'a', 'b', 'c', 'd' ó 'e'
    cola_final[iter] = item;                    // Guarda el contenido en el historial de mensajes producidos
    return item;
}

/* Función que muestra todos los mensajes generados por el productor a lo largo del programa, para facilitar la
 * comprobación de la validez del resultado. 
 */
void imprimir_cola_final(){
    int i, j;

    // Se imprime el historial en líneas de 10 mensajes
    for (i = 0; i < DATOS_A_PRODUCIR; i += 10){
        printf("ITER -> ");         // Título de la línea (iteración)
        // En la condicion de finalizacion se comprueba que j no alcance el tamaño del historial
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf("%02d ", j);

        printf("\nITEM -> ");       // Contenido de la línea (mensaje)
        for (j = i; j < i + 10 && j < DATOS_A_PRODUCIR; j++) printf(" %c ", cola_final[j]);

        printf("\n\n");
    }
}

/* Función principal del productor.
 * En cada iteración, espera a recibir una orden (un mensaje vacío) del consumidor. A continuación, genera un nuevo
 * item y se lo envía.
 * Se llevan a cabo un total de DATOS_A_PRODUCIR iteraciones.
 */

void productor() {
    char item;          // Item donde se almacena el mensaje recibido del consumidor
                        // También guardará el mensaje a enviar como respuesta
    int i;              // Contador de iteraciones

    for (i = 0; i < DATOS_A_PRODUCIR; i++){
        /* El productor lee un mensaje de su buffer de recepción, buz_ordenes, usando mq_receive. Se toma el mensaje
         * de mayor prioridad y, en caso de empate, aquel que ha llegado antes al buffer.
         * Los argumentos de la función son:
         * - buz_ordenes: buffer de donde se leerá el mensaje.
         * - item: puntero a la variable donde se almacenará el mensaje leído. 
         * - tam_msg: tamaño del mensaje a leer (será un carácter).
         * - NULL: como este argumento se pasaría un puntero a un unsigned int donde guardar la prioridad del mensaje.
         * No obstante, el consumidor siempre usará prioridad 0 en sus mensajes (ya que el contenido es irrelevante:
         * son mensajes vacíos que únicamente sirven de aviso). Por tanto, este argumento se ignora (NULL).
         * 
         * Si no hay mensajes, el productor se bloquea hasta que llege uno o lo despierte una señal.
         */
        mq_receive(buz_ordenes, &item, tam_msg, NULL);
        item = producir_elemento(i);            // El elemento producido se genera en base a la iteración actual
        // Se envía el elemento producido al buffer de entrada del consumidor (buz_items)
        // No es necesario usar distintas prioridades, pues en caso de igualdad la implementación es FIFO por defecto.
        // De esta forma, el consumidor leerá siempre el mensaje más antiguo que ha llegado a su buffer.
        mq_send(buz_items, &item, tam_msg, 0);
        printf("[ITER %02d] Enviado item %c\n", i, item);
    }

    printf("\n\n\n");
    // Al acabar, el productor imprime todo el historial de mensajes en orden.
    printf("Finalizados envíos y recepciones. Cola de items producidos:\n");
    imprimir_cola_final();
}


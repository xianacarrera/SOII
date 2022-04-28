#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/*
 * Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 3 - Ejercicio 2 - Versión 2
 *
 * Este programa implementa el problema del productor-consumidor resuelto utilizando mutexes, pero sin emplear
 * variables de condicion. Se basa en la emulación del comportamiento de una variable de control y en el uso de
 * pthread_yield.
 * Los cambios se encuentran, principalmente, en las funciones producir() y consumir().
 * Debe compilarse con la opción -pthread.
 */

#define P 5          // Número de productores
#define C 4          // Número de consumidores

#define PROD 1       // Código de los productores
#define CONS 2       // Código de los consumidores

#define N 10                       // Tamaño del buffer
#define ITEMS_BY_P 20              // Items producidos por cada productor
#define SLEEP_MAX_TIME 4           // Máximo tiempo de bloqueo por un sleep

#define VERDE "\033[32m"           // Color en el que imprimirán los productores
#define AZUL "\033[34m"            // Color en el que imprimirán los consumidores
#define ROJO "\033[0;31m"          // Finalización de procesos
#define RESET "\033[39m"           // Reseteado de color
#define CURSIVA "\033[3m"          // Letra en cursiva (buffer)
#define RESET_CURS "\033[23m"      // Reseteado de cursiva


// Función de impresión del buffer con un código de colores para productores (verde) y consumidores (azul)
void log_buffer(int hilo);
// Función de impresión de un mensaje junto (opcionalmente) al contenido del buffer en una línea
// Su propósito es permitir imprimir de forma casi atómica y sin interferencias entre hilos
void imprimir(char * cadena, int ver_buffer);

// Función auxiliar que inicializa los mutexes y variables de condicion
void inicializar();
// Función auxiliar que destruye los mutexes y variables de condicion
void destruir();

// Función que comprueba si el buffer tiene N elementos
int esta_buffer_lleno();
// Función que comprueba si el buffer tiene 0 elementos
int esta_buffer_vacio();

// Función de generación de un item (productores)
char produce_item(int id);
// Función de inserción de un item en el buffer (productores)
void insert_item(char letra, int id);
// Función de eliminación de un item del buffer (consumidores)
char remove_item(int id);
// Función de impresión de un item eliminado (consumidores)
void consume_item(char item, int id);

// Función de ejecución de los hilos productores
void * producir(void * ptr_id);
// Función de ejecución de los hilos consumidores
void * consumir(void * ptr_id);


// Función que encapsula la creación de un hilo, que ejecutará una funcion con un argumento entero
void crear_hilo(pthread_t * hilo, void * funcion, int arg);
// Función que ejecuta pthread_join sobre un hilo y comprueba que finalice correctamente
void esperar_hilo(pthread_t hilo);


pthread_mutex_t mutex;             // Mutex de acceso a la región crítica
pthread_mutex_t mutex_impr;        // Mutex de impresión por consola

char * buffer = NULL;       // Buffer de caracteres compartido por productor y consumidor
int cuenta = 0;             // Número de elementos guardados en el buffer


int main(int argc, char * argv[]){
    pthread_t consumidores[C];              // Identificadores de los hilos consumidores
    pthread_t productores[P];               // Identificadores de los hilos productores
    int i;                                  // Variables de iteración

    srand(time(NULL));      // Fijamos una semilla de generación de valores aleatorios

    // En primer lugar, se reserva memoria para el buffer compartido entre procesos. Tendrá un máximo de N chars.
    if ((buffer = (char *) malloc(N * sizeof(char))) == NULL){
        fprintf(stderr, "Error: no se pudo reservar memoria para el buffer\n");
        exit(EXIT_FAILURE);
    }

    // Inicializamos todo el buffer con el carácter '_', que representa una posición vacía
    memset(buffer, '_', (size_t) N * sizeof(char));

    printf("**************************** PROBLEMA DEL PRODUCTOR-CONSUMIDOR ***************************************\n");
    printf("Preparado buffer de caracteres. Contenido inicial: buffer = [");
    for (i = 0; i < N - 1; i++)
        printf("%c ", *(buffer + i));           // Leemos uno a uno los caracteres contenidos en el buffer
    printf("%c]\n", *(buffer + i));
    printf("Número de items inicial: cuenta = %d\n\n\n", cuenta);

    printf("Se empleará el siguiente código de colores:\n");
    printf("\t%sPRODUCTORES%s\n", VERDE, RESET);
    printf("\t%sCONSUMIDORES%s\n", AZUL, RESET);
    printf("\t%sFINALIZACIÓN DE PROCESOS%s\n\n\n", ROJO, RESET);

    // Se inicializan los mutexes y las variables de condicion
    inicializar();

    // Creamos C consumidores y P productores. Cada uno de ellos será denotado por el valor de la variable i en el
    // momento de su creación. Guardamos su identificador en los arrays consumidores[] y productores[]
    for (i = 0; i < C; i++) crear_hilo(&consumidores[i], consumir, i);
    for (i = 0; i < P; i++) crear_hilo(&productores[i], producir, i);

    // El hilo principal espera a que finalicen todos los hilos que ha creado antes de continuar
    for (i = 0; i < C; i++) esperar_hilo(consumidores[i]);
    for (i = 0; i < P; i++) esperar_hilo(productores[i]);

    // Se destruyen los mutexes y las variables de condicion
    destruir();

    free(buffer);       // Liberamos también la memoria reservada para el buffer

    printf("\n\n\nFinalizando problema del productor-consumidor...\n\n");
    exit(EXIT_SUCCESS);
}



/*
* Función ejecutada por los productores, que introducen nuevos elementos en el buffer por la parte superior de la
* pila. Para asegurar que no se produzcan carreras críticas, se sincronizan con los productores empleando el mutex
* 'mutex', pero sin emplear variables de condicion.
* Los mensajes del productor aparecen en color verde y en el lado izquierdo de la terminal.
* @param ptr_id: Identificador del hilo pasado como puntero a void.
*/
void * producir(void * ptr_id){
    int id = (intptr_t) ptr_id;    // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)
    char item;                     // Item producido que se guardará en el buffer
    char cadena[100];              // Cadena donde se guardará la información que vaya a imprimir el hilo, para poder
                                // manejarla de la forma más atómica posible
    int tam_cad = sizeof(cadena);       // Tamaño en bytes que ocupa la cadena
    int i, j;                      // Contadores de iteraciones

    // Cada productor realiza un número fijo de iteraciones: 20, una por cada item que produzca

    for (i = 0; i < ITEMS_BY_P; i++){
        // Para imprimir, construimos el mensaje y lo almacenamos en cadena. Después, se la pasamos a la función
        // imprimir
        // Como segundo argumento de snprintf pasamos el número máximo de bytes a almacenar, esto es, el tamaño de la
        // cadena, evitando escrituras en posiciones que no correspondan al array
        snprintf(cadena, tam_cad,
                "%s[%d] **INICIO ITERACION %d**%s\n", VERDE, id, i, RESET);
        imprimir(cadena, 0);     // No imprimimos el buffer al estar fuera de la región crítica (puede desactualizarse)

        // Se crea un nuevo elemento y se almacena en la variable item
        // Cada hilo producirá siempre el mismo elemento, cuyo valor dependerá de su identificador. Será una letra
        // del abecedario que podrá ir en mayúsculas (identificadores impares) o en minúsculas (identificadores pares)
        item = produce_item(id);

        // Esperamos un núemro de segundos aleatorio de entre 0 y 4 para dar más variedad a las situaciones que
        // se pueden producir (buffer lleno, buffer vacío y situaciones intermedias).
        sleep(((int) rand()) % SLEEP_MAX_TIME);

        /*
         * A continuación, el productor se prepara para ejecutar la región crítica. Para ello, solicita acceso
         * realizando un lock sobre el mutex principal.
         * Si no hay nadie en la región crítica, el hilo adquiere el mutex, excluyendo a cualquier otro de acceder.
         * Entonces, pthread_mutex_lock finaliza, de forma que se continúa con la ejecución de la función.
         * Si ya hay alguien en la región crítica (es decir, el mutex ya está en posesión por otro), se bloqueará
         * al productor. Cuando el otro hilo salga de la región crítica, tendrá la responsabilidad de despertar a
         * uno y solo uno de los hilos bloqueados por el mutex (a través de pthread_mutex_unlock).
         */

        pthread_mutex_lock(&mutex);

        /*
         * La clave de esta implementación sin variables de condición se encuentra en este bucle. En él, se emula
         * la implementación de una variable de condición.
         * 1) En primer lugar, se comprueba si el buffer está lleno. Si es así, se libera el mutex para posibilitar
         *     que un consumidor entre en la región crítica y cree un hueco en la pila.
         * 2) A continuación, el hilo vuelve a comprobar si la condición de que el buffer esté lleno se sigue
         *     cumpliendo. Mientras eso ocurra, ejecutará pthread_yield para ceder su turno de la CPU, pues no
         *     podrá avanzar. Empleamos dos bucles while anidados para minimizar las interferencias de este hilo
         *     con el mutex mientras no el buffer siga lleno.
         * 3) Cuando el buffer ya no esté lleno, el hilo dejará de ceder su turno y tratará de adquirir el mutex.
         *     Entre este punto y el momento en el que se le conceda puede pasar un largo período, por lo que
         *     volverá a comprobar la condición del while externo y, de haberse llenado otra vez el buffer,
         *     repetirá el proceso.
         * Con esta implementación no es necesario que los productores y consumidores se notifiquen entre sí con
         * respecto al vaciado y llenado. Cada hilo se responsabiliza de comprobar la condición para sí mismo
         * leyendo la variable cuenta.
         */
        while(esta_buffer_lleno()){
            snprintf(cadena, tam_cad,
                    "%s[%d] cede el mutex por estar el buffer lleno**%s\n", VERDE, id, i, RESET);
            imprimir(cadena, 0);
            pthread_mutex_unlock(&mutex);
            while (esta_buffer_lleno()) pthread_yield();
            pthread_mutex_lock(&mutex);
        }
        /**************************************** REGIÓN CRÍTICA *******************************************/
        insert_item(item, id);      // Se introduce el item en la región crítica y se actualiza cuenta
        /************************************** FIN DE LA REGIÓN CRÍTICA **********************************/
        pthread_mutex_unlock(&mutex);           // El productor abandona la región crítica. Libera el mutex para
        // permitir que otro hilo pueda acceder a ella. Si había uno o varios bloqueados por pthread_mutex_lock, el
        // sistema operativo escogerá a uno de ellos y le concederá el mutex para que pueda continuar. Si no había
        // ninguno, el mutex queda libre para que lo use el primero que ejecute pthread_mutex_lock.


        // Se imprime una cadena con el identificador del hilo, el número de iteraciones pendientes. No imprimimos
        // el buffer al estar fuera de la región crítica
        snprintf(cadena, tam_cad,
                "%s[%d] Me quedan %d iteraciones%s\n", VERDE, id, ITEMS_BY_P - i - 1, RESET);
        imprimir(cadena, 0);
    }

    // Se imprime un mensaje de finalización en rojo
    snprintf(cadena, tam_cad,
             "\n%s[%d] Finalizando productor...%s\n", ROJO, id, RESET);
    imprimir(cadena, 0);            // En este caso, pasamos un 0 para no mostrar el buffer

    // En el ejercicio 2 empleábamos la función exit() para cerrar la ejecución. Ahora, dado que empleamos hilos,
    // cerramos empleando pthread_exit, para que el proceso continúe ejecutándose.
    pthread_exit((void *) "Hilo finalizado correctamente");
}



/*
* Función ejecutada por los consumidores, que eliminan (sobreescriben con un espacio ' ') elementos preexistentes en
* el buffer por la parte superior de la pila.
* Para asegurar que no se produzcan carreras críticas, los consumidores sincronizan el acceso a su región crítica con
* los productores. Para ello, emplean un mutex sobre toda la región crítica (aunque sin variables de condicion).
* Los mensajes del consumidor aparecen en color azul y en el lado derecho de la terminal.
* @param ptr_id: Identificador del hilo pasado como puntero a void.
*/
void * consumir(void * ptr_id){
    int id = (intptr_t) ptr_id;    // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)
    char item;                     // Item consumido. Será mostrado por pantalla.
    char cadena[100];              // Cadena donde se guardará la información que vaya a imprimir el hilo, para poder
                                   // manejarla de la forma más atómica posible
    int tam_cad = sizeof(cadena);       // Tamaño en bytes que ocupa la cadena
    int num_iters;                 // Número de iteraciones que tendrá que ejecutar cada consumidor
    int i, j;                      // Contadores de iteraciones

    /*
     * El número de iteraciones totales (ITEMS_BY_P * P = 20 * P) se divide de forma equitativa entre los consumidores.
     * No obstante, habría problemas en caso de que C no fuese divisor de ITEMS_BY_P * P. En ese caso, el resto de la
     * división se asigna como iteraciones extra para el primer consumidor (el de identificador 0). Es decir, a este
     * le corresponde el cociente y el resto. Los demás llevarán a cabo ITEMS_BY_P * P iteraciones (el cociente).
     */
    num_iters = !id? (ITEMS_BY_P * P / C) + (ITEMS_BY_P * P % C) : ITEMS_BY_P * P / C;

    for (i = 0; i < num_iters; i++){
        // Para imprimir, construimos el mensaje y lo almacenamos en cadena. Después, se la pasamos a la función
        // imprimir.
        // Como segundo argumento de snprintf pasamos el número máximo de bytes a almacenar, esto es, el tamaño de la
        // cadena, evitando escrituras en posiciones que no correspondan al array
        snprintf(cadena, tam_cad,
                "\t\t\t\t\t\t%s[%d] **INICIO ITERACION %d**%s\n", AZUL, id, i, RESET);
        imprimir(cadena, 0);     // No imprimimos el buffer al estar fuera de la región crítica (puede desactualizarse)

        /*
         * El consumidor comienza solicitando acceso a la región crítica. Para ello, realiza un lock sobre el mutex
         * principal, que previene de que haya más de 2 hilos simultáneamente trabajando en ella.
         * Si no hay nadie en la región crítica, el hilo adquiere el mutex, excluyendo a cualquier otro de acceder.
         * Entonces, pthread_mutex_lock finaliza, de forma que se continúa con la ejecución de la función.
         * Si ya hay alguien en la región crítica (es decir, el mutex ya está reservado por otro), se bloqueará
         * al productor. Cuando el otro hilo salga de la región crítica, tendrá la responsabilidad de despertar a
         * uno y solo uno de los hilos bloqueados por el mutex (a través de pthread_mutex_unlock).
         */
        pthread_mutex_lock(&mutex);

        /*
         * La clave de esta implementación sin variables de condición se encuentra en este bucle. En él, se emula
         * la implementación de una variable de condición.
         * 1) En primer lugar, se comprueba si el buffer está vacío. Si es así, se libera el mutex para posibilitar
         *     que un productor entre en la región crítica y coloque un elemento en la pila.
         * 2) A continuación, el hilo vuelve a comprobar si la condición de que el buffer esté vacío se sigue
         *     cumpliendo. Mientras eso ocurra, ejecutará pthread_yield para ceder su turno de la CPU, pues no
         *     podrá avanzar. Empleamos dos bucles while anidados para minimizar las interferencias de este hilo
         *     con el mutex mientras no el buffer siga lleno.
         * 3) Cuando el buffer ya no esté lleno, el hilo dejará de ceder su turno y tratará de adquirir el mutex.
         *     Entre este punto y el momento en el que se le conceda puede pasar un largo período, por lo que
         *     volverá a comprobar la condición del while externo y, de haberse llenado otra vez el buffer,
         *     repetirá el proceso.
         * Con esta implementación no es necesario que los productores y consumidores se notifiquen entre sí con
         * respecto al vaciado y llenado. Cada hilo se responsabiliza de comprobar la condición para sí mismo
         * leyendo la variable cuenta.
         */
        while(esta_buffer_vacio()){
            snprintf(cadena, tam_cad,
                    "\t\t\t\t\t\t%s[%d] cede el mutex por estar el buffer vacio%s\n", AZUL, id, RESET);
            imprimir(cadena, 0);
            pthread_mutex_unlock(&mutex);
            while (esta_buffer_vacio()) pthread_yield();
            pthread_mutex_lock(&mutex);
        }
        /**************************************** REGIÓN CRÍTICA *******************************************/
        item = remove_item(id);      // Se elimina un item del buffer y se actualiza cuenta
        /************************************** FIN DE LA REGIÓN CRÍTICA **********************************/
        pthread_mutex_unlock(&mutex);       // El consumidor abandona la región crítica

        // Esperamos un núemro de segundos aleatorio de entre 0 y 4 para dar más variedad a las situaciones que
        // se pueden producir (buffer lleno, buffer vacío y situaciones intermedias).
        sleep(((int) rand()) % SLEEP_MAX_TIME);

        // Mostramos por pantalla el item consumido, junto al identificador del consumidor que lo ha eliminado
        consume_item(item, id);

        // Imprimimos un mensaje indicando el número de iteraciones que le quedan por ejecutar a este hilo, así como
        // su identificador. No imprimimos el buffer al estar fuera de la región crítica
        snprintf(cadena, tam_cad,
                "\t\t\t\t\t\t%s[%d] Me quedan %d iteraciones%s\n", AZUL, id, num_iters - i - 1, RESET);
        imprimir(cadena, 0);
    }

    // Se imprime un mensaje de finalización en rojo
    snprintf(cadena, tam_cad,
            "\n\t\t\t\t\t\t%s[%d] Finalizando consumidor...%s\n", ROJO, id, RESET);
    imprimir(cadena, 0);            // En este caso, pasamos un 0 para no mostrar el buffer

    // En el ejercicio 2 empleábamos la función exit() para cerrar la ejecución. Ahora, dado que empleamos hilos,
    // cerramos empleando pthread_exit, para que el proceso continúe ejecutándose.
    pthread_exit((void *) "Hilo finalizado correctamente");
}


// Función que verifica si el buffer ha llegado a su máximo de capacidad
// El propósito de definir esta función es, principalmente, ayudar a la legibilidad del código
// Devuelve 1 si es así y 0 en caso contrario.
int esta_buffer_lleno(){
    return cuenta == N;
}

// Función que verfica si el buffer tiene algún elemento
// El propósito de definir esta función es, principalmente, ayudar a la legibilidad del código
// Devuelve 1 si está vacío, y 0 si hay elementos en él
int esta_buffer_vacio(){
    return !cuenta;
}


/*
 * Función que genera un  carácter dependiente del identificador del hilo. En concreto, los hilos pares imprimen en
 * minúsculas, y los pares, en mayúsculas. El carácter en concreto se calcula sumándole a 'a' o 'A' un número igual
 * al triple del identificador (para dejar una cierta separación entre letras).
 * Además, se realiza el módulo por 26 para asegurar que los caracteres sean siempre letras del alfabeto. Como 3 y 26
 * son coprimos, se utilizará todo el abecedario.
 * Cada hilo imprime siempre el mismo carácter.
 * Esta función es empleada por los productores.
 * @param id: Identificador del hilo.
 */
char produce_item(int id){
    // Al sumar (id % 2) * (-32), estamos restando -32 para los hilos de identificador impar (pasamos del rango 'a-z'
    // al 'A-Z'), pero no hacemos nada para los pares.
    return ('a' + (id * 3 % 26)) + ((id % 2) * (-32));
}

/*
 * Función que coloca una letra en el buffer. Se insertará en la posición superior, puesto que se trata de una pila
 * LIFO.
 * Dicha posición vendrá dada por la variable compartida cuenta.
 * Esta función es empleada por los productores y forma parte de la región crítica.
 * No se comprueba que el buffer no esté lleno; este es un prerrequisito a corroborar de forma externa.
 * @param letra: Carácter a colocar en el buffer.
 * @param id: Identificador del hilo (solo usado a efectos de impresión).
 */
void insert_item(char letra, int id){
    char cadena[100];                // Línea a imprimir en el log.

    buffer[cuenta] = letra;         // Se almacena el nuevo elemento en la primera posición libre del buffer.

    // Incrementamos el valor de cuenta para reflejar el nuevo elemento
    cuenta++;

    // Preparamos la línea con snprintf y luego la imprimimos de forma atómica llamando a imprimir
    snprintf(cadena, sizeof(cadena),
             "%s[%d] Guardado item %c en %d -> cuenta = %d%s  \t\t\t\t\t\t\t\t\t",
             VERDE, id, letra, cuenta - 1, cuenta, RESET);
    imprimir(cadena, PROD);         // Con el 2º argumento mostramos el buffer en verde

}

/*
 * Función que retira una letra del buffer. Como se implementa como una cola LIFO, se borra de la posición superior.
 * Dicha letra es devuelta por la función y reemplazada por un guion bajo '_'.
 * Esta función es empleada por los consumidores y forma parte de la región crítica.
 * @param id: identificador del hilo (solo usado a efectos de impresión)
 */
char remove_item(int id){
    char item;                   // Letra que contenía el buffer en la posición inicio.
    char cadena[100];            // Línea que se imprimirá

    // Como cuenta indica el número de posiciones ocupadas, el último elemento estará en cuenta - 1
    item = buffer[cuenta - 1];          // Guardamos el item
    buffer[cuenta - 1] = '_';           // Borramos la posición
    cuenta--;                           // Decrementamos el número de items presentes en el buffer

    // Indicamos que se ha retirado un elemento (lo imprimimos desde la región crítica para que quede constancia
    // inmediata)
    snprintf(cadena, sizeof(cadena),
             "\t\t\t\t\t\t%s[%d] Retirado item %c, cuenta = %d    %s\t\t\t\t", AZUL, id, item, cuenta, RESET);
    imprimir(cadena, CONS);             // Con el 2º argumento mostramos el buffer en azul

    return item;                        // Devolvemos el elemento leído
}


/*
 * Función que muestra el carácter que fue retirado del buffer por la función remove_item.
 * Esta función es empleada por los consumidores, que imprimen en color azul.
 * @param item: Elemento a imprimir y que fue previamente retirado del buffer.
 * @param id: Identificador del hilo.
 */
void consume_item(char item, int id){
    char cadena[100];           // Línea que se imprimirá

    // Preparamos la línea con snprintf y luego la imprimimos de forma atómica llamando a imprimir
    snprintf(cadena, sizeof(cadena),
             "\t\t\t\t\t\t%s[%d] Consumido item %c%s\n", AZUL, id, item, RESET);
    imprimir(cadena, 0);
    // Con el 2º argumento evitamos que se imprima el buffer. Dado que estamos fuera de la región crítica, este
    // podría ya haber cambiado su contenido con respecto a la anterior eliminación en remove_item y podría dar
    // lugar a confusión
}


/*
 * Función que imprime los contenidos del buffer en una línea. Es empleada por los productores, que imprimen en verde,
 * y por los consumidores, que imprimen en azul. Toda la cadena aparece en cursiva.
 * @param hilo: Valdrá PROD (1) si se trata de un hilo productor, y CONS (2) si es un consumidor.
 */
void log_buffer(int hilo){
    int i;          // Variable de iteración

    // Dependiendo del argumento hilo, se imprime en verde o en azul. Siempre se activa la cursiva.
    printf("%s%sbuffer = [", hilo == PROD? VERDE : AZUL, CURSIVA);
    for (i = 0; i < N - 1; i++)
        printf("%c ", *(buffer + i));           // Leemos uno a uno los caracteres contenidos en el buffer
    printf("%c]%s%s\n", *(buffer + i), RESET, RESET_CURS);
    // No utilizamos \n hasta el final para tratar de que el buffer se imprima de forma atómica
}

/*
 * Función de impresión que busca asegurar que esta tenga lugar de forma atómica entre hilos. Para ello,
 * no se permite que varios hilos empleen esta función a la vez (para lo cual se utiliza un mutex, mutex_impr) y
 * se añade una pequeña espera antes de terminarla, para tratar de que el hilo complete su uso de la consola.
 * La impresión corresponde a una sola línea y consta de un mensaje seguido opcionalmente del contenido del buffer.
 * @param cadena: Mensaje inicial a imprimir antes del buffer.
 * @param ver_buffer: 0 para no incluir el buffer en la línea, !0 para añadirlo (1 para imprimir como productor y 2
 *                    para imprimir como consumidor).
 */
void imprimir(char * cadena, int ver_buffer){
    // Adquirimos el mutex de impresión. Así, definimos el empleo de la consola como una región crítica (al fin y al
    // cabo, se está empleando un recurso compartido). Únicamente un hilo podrá utilizarla de forma simultánea.
    pthread_mutex_lock(&mutex_impr);
    printf("%s", cadena);
    // Si se ha solicitado, se imprime el buffer en la misma línea
    // Pasamos ver_buffer a log_buffer para que se tenga en cuenta si se trata del productor (1) o del consumidor (2)
    if (ver_buffer) log_buffer(ver_buffer);

    usleep(150);     // Dormimos al hilo algunos microsegundos para tratar de que dé tiempo a imprimir y las
                     // llamadas a printf procedentes de distintos hilos no se mezclen por consola

    // Se libera el mutex de impresión, permitiendo que un (y solamente un) hilo que estuviera esperando para imprimir
    // pueda salir de pthread_mutex_lock y ejecutar los printf.
    pthread_mutex_unlock(&mutex_impr);
}

// Función auxiliar que inicializa los mutexes
void inicializar(){
    // Antes de poder emplear los mutexes en las funciones pthread_mutex_lock, pthread_mutex_unlock, etc., deben ser
    // inicializados. Para ello, utilizamos la función pthread_mutex_init
    // Dejamos el segundo argumento a NULL para emplear la configuración de atributos por defecto.
    if (pthread_mutex_init(&mutex, NULL)){
        fprintf(stderr, "Error en la inicializacion del mutex de la region critica\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&mutex_impr, NULL)){
        fprintf(stderr, "Error en la inicializacion del mutex de impresion\n");
        exit(EXIT_FAILURE);
    }
    // Inicializamos también los dos mutexes propios a esta implementación
    if (pthread_mutex_init(&mutex_vacio, NULL)){
        fprintf(stderr, "Error en la inicializacion del mutex del buffer vacio\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&mutex_lleno, NULL)){
        fprintf(stderr, "Error en la inicializacion del mutex del buffer lleno\n");
        exit(EXIT_FAILURE);
    }
}

// Función auxiliar que destruye los mutexes
void destruir(){
    // Una vez ha finalizado el uso de los mutexes, se pueden destruir con seguridad.
    // Tenemos la seguridad de que todos los mutexes están desbloqueados, pues todos los hilos han finalizado
    // correctamente (en caso contrario, pthread_mutex_destroy podría dar error)
    if (pthread_mutex_destroy(&mutex)){
        fprintf(stderr, "Error en la destruccion del mutex de la region critica\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&mutex_impr)){
        fprintf(stderr, "Error en la destruccion del mutex de impresion\n");
        exit(EXIT_FAILURE);
    }
    // Destruimos también los mutexes propios de esta implementación
    if (pthread_mutex_destroy(&mutex_vacio)){
        fprintf(stderr, "Error en la destruccion del mutex del buffer vacio\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&mutex_lleno)){
        fprintf(stderr, "Error en la destruccion del mutex del buffer lleno\n");
        exit(EXIT_FAILURE);
    }
}


/*
 * Función que encapsula la creación de un hilo.
 * @param hilo: puntero al pthread_t donde se guardará el identificador del hilo creado
 * @param funcion: funcion que ejecutará el hilo nada más crearse
 * @param arg: argumento que se le pasará a la función del hilo craedo
 */
void crear_hilo(pthread_t * hilo, void * funcion, int arg){
    int error;          // Comprobación de errores

    // El argumento debe enviarse como un puntero a void, y no como un entero. Para realizar una conversión segura,
    // pasamos primero el dato a intptr_t y después a un puntero a void.
    // Pasamos NULL como segundo argumento para no modificar los atributos por defecto del hilo
    if ((error = pthread_create(hilo, NULL, funcion, (void *) (intptr_t) arg)) != 0){
        fprintf(stderr, "Error en la creación de un hilo\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Función que encapsula una llamada a pthread_join, que provocará que el hilo que la ejecute espere a la Finalización
 * de algún otro antes de continuar con su ejecución.
 * @param hilo: Identificador del hilo al que se debe esperar.
 */
void esperar_hilo(pthread_t hilo){
    int error;              // Comprobación de errores
    char * exit_hilo;       // Mensaje de finalización del hilo a esperar

    // El hilo en ejecución se enlaza al pasado como argumento. Cuando este finalice, se guardará el mensaje que haya
    // pasado a través de pthread_exit en exit_hilo
    if ((error = pthread_join(hilo, (void **) &exit_hilo)) != 0){
        fprintf(stderr, "Error %s al esperar por un hilo", strerror(error));
        exit(EXIT_FAILURE);
    }

    // Si el mensaje de finalización no es "Hilo finalizado correctamente", tuvo lugar algún problema
    if (strcmp(exit_hilo, "Hilo finalizado correctamente")){
        fprintf(stderr, "Error: finalización incorrecta o inesperada de un hilo");
        exit(EXIT_FAILURE);
    }
}

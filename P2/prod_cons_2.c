#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 2 - Ejercicio 2
 *
 * Este programa implementa el problema del productor-consumidor solucionándolo mediante el uso de semáforos.
 * El buffer empleado funciona como una cola FIFO.
 * En este ejercicio el consumidor se crea antes que el productor.
 *
 * Debe compilarse con la opción -pthread.
 */


#define N 15                        // Tamaño del buffer compartido entre productor y consumidor
#define N_ITER 100                  // Número de iteraciones de cada proceso

#define VERDE "\033[32m"            // Color en el que imprimirá el productor
#define AZUL "\033[34m"             // Color en el que imprimirá el consumidor
#define RESET "\033[39m"            // Reseteado de color
#define CURSIVA "\033[3m"           // Letra en cursiva
#define RESET_CURS "\033[23m"       // Reseteado de cursiva


// Función del productor
void producir();
// Función del consumidor
void consumir();

// Función de generación de un item (productor)
char produce_item(int pos);
// Función de inserción de un item en el buffer (productor)
void insert_item(int * final, char letra);
// Función de eliminación de un item del buffer (consumidor)
char remove_item(int * inicio);
// Función de impresión de un item eliminado (consumidor)
void consume_item(char item);

// Función de impresión del contenido del buffer
void log_buffer(int proceso);

// Función de cierre del área de memoria compartida
void cerrar_mem_compartida();

// Función de cierre de semáforos con sem_close
void cerrar_semaforos(sem_t * vacias, sem_t * mutex, sem_t * llenas);
// Función de destrucción de semáforos con sem_unlink
void destruir_semaforos();

// Función auxiliar de finalización con error
void cerrar_con_error(char * mensaje, int ver_errno);


char * buffer = NULL;                      // Área de memoria compartida: un buffer de caracteres (cola FIFO)


int main(int argc, char * argv[]){
    sem_t * vacias = NULL;       // Semáforo que representa el número de posiciones vacías en el buffer
    sem_t * mutex = NULL;        // Semáforo que salvaguarda el acceso al buffer (solo toma los valores 0 y 1)
    sem_t * llenas = NULL;       // Semáforo que representa el número de posiciones llenas en el buffer
    pid_t exit_wait;      // Error de la función
    int status;           // Condición de finalización de un proceso hijo
    pid_t error_fork;     // Código de retorno del fork
    int exit_unlink;      // Error de la función sem_unlink


    /*
     * Reservamos un área de memoria compartida y anónima (sin archivo de respaldo) a través de mmap. Cuando creemos
     * los procesos hijos (productor y consumidor), hererdarán dicha región.
     *
     * Indicamos como argumentos:
     * NULL -> El kernel elige la dirección inicial (alineada con las páginas)-
     * N * sizeof(char) -> Tamaño en bytes que tendrá la zona de memoria (N chars)
     * PROT_READ | PROT_WRITE -> Se obtendrán permisos de escritura y lectura.
     * MAP_SHARED | MAP_ANONYMOUS -> Memoria compartida y sin archivo de respaldo. El contenido se inicializa a 0.
     * -1 -> No hay descriptor, al estar usando MAP_ANONYMOUS. En este caso, algunas implementaciones requieren que
     *       este argumento sea -1.
     * 0 -> El offset también debe ser 0, ya que estamos usando MAP_ANONYMOUS.
     * MAP_FAILED es (void *) -1, por lo que lo casteamos a (char *).
     */
    if ((buffer = (char *) mmap(NULL, (size_t) N * sizeof(char), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS, -1, (off_t) 0)) == (char *) MAP_FAILED)
        // Si hay algún error, finalizamos la ejecución e imprimimos errno con un mensaje personalizado
        cerrar_con_error("Error: no se ha podido realizar la proyección de memoria con archivo", 1);


    // En el buffer, el carácter ' ' indicará que la posición está vacía. Inicializamos así toda la región, esto es,
    // los N * sizeof(char) bytes.
    memset(buffer, ' ', (size_t) N * sizeof(char));

    // Destruimos los semáforos si ya existían previamente, como medida de precaución
    // Si no hay ningún error, a continuación los creamos y les damos valores iniciales (N, 0 y 1)

    // Utilizamos sem_unlink para destruir los semáforos si ya existían, indicando su nombre
    // Si el semáforo no existía, sem_unlink guardará ENOENT en errno y devolverá un error (que ignoraremos)
    if ((exit_unlink = sem_unlink("PC_VACIAS")) && errno != ENOENT)
        // El único otro error posible es de permisos inadecuados
        cerrar_con_error("Error: permisos inadecuados para el semáforo PC_VACIAS", 1);

    if ((exit_unlink = sem_unlink("PC_MUTEX")) && errno != ENOENT)
        cerrar_con_error("Error: permisos inadecuados para el semáforo PC_MUTEX", 1);

    if ((exit_unlink = sem_unlink("PC_LLENAS")) && errno != ENOENT)
        cerrar_con_error("Error: permisos inadecuados para el semáforo PC_LLENAS", 1);

    /*
     * Utilizamos la función sem_open para crear los semáforos (opción O_CREAT). Les damos todos los permisos para
     * el usuario, y ninguno para el grupo y para otros.
     * vacias empieza con el valor inicial N: todo el buffer está inicialmente vacío.
     */
    if ((vacias = sem_open("PC_VACIAS", O_CREAT, 0700, N)) == SEM_FAILED)
        cerrar_con_error("Error: no se ha podido crear el semaforo PC_VACIAS", 1);

    // mutex se inicia con 1, pues al comenzar, la región crítica no está ocupada
    if ((mutex = sem_open("PC_MUTEX", O_CREAT, 0700, 1)) == SEM_FAILED)
        cerrar_con_error("Error: no se ha podido crear el semaforo PC_MUTEX", 1);

    // llenas se inicia a 0, pues al comenzar, no hay ninguna posición del buffer ocupada
    if ((llenas = sem_open("PC_LLENAS", O_CREAT, 0700, 0)) == SEM_FAILED)
        cerrar_con_error("Error: no se ha podido crear el semaforo PC_LLENAS", 1);

    printf("Se procede a iniciar los procesos productor y consumidor. Se utilizará el código de colores:\n");
    printf("%s\tPRODUCTOR%s\n", VERDE, RESET);
    printf("%s\tCONSUMIDOR%s\n\n\n", AZUL, RESET);


    /*
     * Al crear el consumidor antes que el productor, aseguramos que ambos procesos empiecen aproximadamente a la vez.
     * Si comenzara el productor, este podría insertar un número elevado de elementos en el buffer antes de que el
     * consumidor llegara a estar disponible para eliminarlos. Si empieza el consumidor, quedará bloqueado hasta que
     * el productor empiece a actuar (pues no puede consumir nada mientras el buffer esté vacío)
     */

    if ((error_fork = fork()) == 0)         // Se crea el proceso consumidor
        consumir();
    else if (error_fork == -1){         // Error en el fork
        // Cerramos y destruimos los semáforos con un mensaje de error, así como el área de memoria compartida
        cerrar_mem_compartida();
        cerrar_semaforos(vacias, mutex, llenas);
        destruir_semaforos();
        cerrar_con_error("Error al crear el proceso hijo consumidor", 0);
    }


    if ((error_fork = fork()) == 0)         // Se crea el proceso productor
        producir();
    else if (error_fork == -1){      // Error en el fork
        // Cerramos y destruimos los semáforos con un mensaje de error, así como el área de memoria compartida
        cerrar_mem_compartida();
        cerrar_semaforos(vacias, mutex, llenas);
        destruir_semaforos();
        cerrar_con_error("Error al crear el proceso hijo productor", 0);
    }

    // Una vez creados los procesos hijos, el proceso principal no usará más la memoria compartida (la cierra)
    cerrar_mem_compartida();
    // Tampoco usará los semáforos
    cerrar_semaforos(vacias, mutex, llenas);

    // El proceso queda atrapado en un bucle hasta que todos sus hijos hayan finalizado (waitpid devolverá -1)
    while((exit_wait = waitpid(-1, &status, 0)) != -1){
        /*
        * Si WIFEXITED(status) es 0, el hijo no terminó con un exit() o volviendo del main. Sería el caso de
        * finalización por una señal.
        * Si WIFEXITED(status) es 1, podemos comprobar WEXITSTATUS para determinar cuál fue el valor del exit del hijo.
        *
        * Si hay error, cortamos la ejecución tras cerrar y destruir los semáforos.
        */
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS){
            destruir_semaforos();
            cerrar_con_error("Finalización incorrecta de un proceso hijo", 0);
        }
    }

    // Se destruyen los semáforos empleados (pues se sabe que todos los hijos han finalizado)
    destruir_semaforos();

    // Se cierra el programa
    printf("\n\n\n\nFinalizando ejecucion del problema del productor-consumidor...\n");
    exit(EXIT_SUCCESS);
}



/*
 * Función ejecutada por el productor, que introduce nuevos elementos en el buffer por el final.
 * Para asegurar que no se produzcan carreras críticas, emplea los semáforos vacias, mutex y llenas.
 * Los mensajes del productor aparecen en color verde y en el lado izquierdo de la terminal.
 */
void producir(){
    int final = 0;        // Almacena la posición donde se debe insertar el próximo item (el buffer es una cola FIFO)
    sem_t * vacias;       // Semáforo que representa el número de posiciones vacías en el buffer
    sem_t * mutex;        // Semáforo que salvaguarda el acceso al buffer (solo toma los valores 0 y 1)
    sem_t * llenas;       // Semáforo que representa el número de posiciones llenas en el buffer
    char item;            // Variable que almacena un elemento producido
    int i=0;              // Contador de iteraciones

    // El productor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    vacias = sem_open("PC_VACIAS", 0);
    mutex = sem_open("PC_MUTEX", 0);
    llenas = sem_open("PC_LLENAS", 0);

    srand(time(NULL));          // Establecemos una semilla para la generación de números aleatorios

    while (i < N_ITER){         // Máximo de 100 iteraciones
        // Imprimimos en el log con color verde. Se muestran también los contenidos del buffer y la posicioń final
        // de la cola, donde el productor insertará el próximo item
        printf("%s**INICIO ITERACION %d** Final de cola = %d\t\t\t\t\t\t\t\t\t\t", VERDE, i, final);
        log_buffer(1);

        // Esperamos un número de segundos aleatorio entre 0 y 4
        sleep(rand() % 5);

        // Se crea un nuevo elemento, que será almacenado en la posición final del buffer
        item = produce_item(final);
        // sem_wait decrementa en 1 el valor de un semáforo, si este era >0
        // En caso contrario, bloquea al proceso hasta que el semáforo pase a tener un valor positivo. En ese punto,
        // lo decrementa y desbloquea al proceso.
        sem_wait(vacias);           // Se disminuye el valor de vacías, pues se guardará un item
        sem_wait(mutex);            // Se solicita acceso a la región crítica
        insert_item(&final, item);          // Región crítica: se almacena el item en la posición final del buffer
        // sem_post incrementa en 1 el valor de un semáforo. Si el consumidor estaba bloqueado por la función
        // sem_wait, esperando a que el semáforo cambiara, será despertado
        sem_post(mutex);            // Se deja la región crítica
        sem_post(llenas);           // Se registra que ha quedado una posición libre menos

        i++;       // Cambiamos de iteración
    }

    // Una vez el productor finaliza su trabajo, cierra la región de memoria asociada al buffer y los semáforos
    cerrar_mem_compartida((void *) buffer);
    cerrar_semaforos(vacias, mutex, llenas);

    printf("\n%sFinalizando productor...%s\n", VERDE, RESET);
    exit(EXIT_SUCCESS);         // El proceso finaliza su ejecución
}

/*
 * Función ejecutada por el consumidor, que elimina (sobreescribe con un espacio ' ') elementos preexistentes en el
 * buffer por el inicio.
 * Para asegurar que no se produzcan carreras críticas, emplea los semáforos vacias, mutex y llenas.
 * Los mensajes del consumidor aparecen en color azul y en el lado derecho de la terminal.
 */
void consumir(){
    int inicio = 0;       // Almacena la posición del próximo item a ser eliminado (el buffer es una cola FIFO)
    sem_t * vacias;       // Semáforo que representa el número de posiciones vacías en el buffer
    sem_t * mutex;        // Semáforo que salvaguarda el acceso al buffer (solo toma los valores 0 y 1)
    sem_t * llenas;       // Semáforo que representa el número de posiciones llenas en el buffer
    char item;            // Variable que almacena un elemento consumido, para imprimir su valor
    int i=0;              // Contador de iteraciones

    // El consumidor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    vacias = sem_open("PC_VACIAS", 0);
    mutex = sem_open("PC_MUTEX", 0);
    llenas = sem_open("PC_LLENAS", 0);

    srand(time(NULL));          // Establecemos una semilla para la generación de números aleatorios

    while (i < N_ITER){     // Máximo de 100 iteraciones
        // El consumidor imprime un mensaje avisando de que va a iniciar una nueva ejecución
        // Muestra el inicio de la cola (de donde eliminará un elemento) y los contenidos del buffer
        printf("\t\t\t\t\t\t%s**INICIO ITERACION %d** Inicio de cola = %d\t\t\t\t", AZUL, i, inicio);
        log_buffer(0);

        // Esperamos un número de segundos aleatorio entre 0 y 4
        sleep(rand() % 5);

        // sem_wait decrementa en 1 el valor de un semáforo, si este era >0
        // En caso contrario, bloquea al proceso hasta que el semáforo pase a tener un valor positivo. En ese punto,
        // lo decrementa y desbloquea al proceso.
        sem_wait(llenas);           // Si no hay ningún elemento en el buffer, el consumidor se bloquea
                                    // Si hay algún elemento, reduce la cuenta
        sem_wait(mutex);            // Solicita acceso a la región crítica
        item = remove_item(&inicio);
                // Región crítica: se elimina un item de la posición inicio y se almacena en la variable item
                // También se incrementa inicio
        sem_post(mutex);            // Se abandona la región crítica, permitiendo el acceso al productor si este
                                    // estaba bloqueado esperando
        sem_post(vacias);           // Se incrementa el contador de posiciones vacías, pues una ha quedado libre
        consume_item(item);         // Se imprime el valor del elemento eliminado (sobreescrito por ' ')

        sleep(rand() % 5);      // Dormimos de nuevo al proceso para provocar más variaciones

        i++;                        // Se pasa a la siguiente iteración
    }

    // Una vez el consumidor finaliza su trabajo, cierra la región de memoria asociada al buffer y los semáforos
    cerrar_mem_compartida();
    cerrar_semaforos(vacias, mutex, llenas);

    printf("\n\t\t\t\t\t\t%sFinalizando consumidor...%s\n", AZUL, RESET);
    exit(EXIT_SUCCESS);         // El consumidor finaliza su ejecución
}

/*
 * Función que genera un elemento de la forma 'a' + pos, siendo pos igual a la posición que ocupará la letra en el
 * buffer. Como este tiene 15 posiciones, se generarán las letras minúsculas de la 'a' a la 'o'.
 * Esta función es empleada por el productor.
 * @param pos: Posición que ocupará el elemento dentro del buffer.
 */
char produce_item(int pos){
    // Por cuestiones de seguridad (por ejemplo, si se incrementara el tamaño del buffer a un N > 26), empleamos
    // el módulo por el número de letras del abecedario, 26, de forma que se reiniciaría la secuencia.
    return 'a' + pos % 26;
}

/*
 * Función que coloca una letra en el buffer. Se insertará en la última posición, puesto que se trata de una cola FIFO.
 * Esta función es empleada por el productor.
 * @param final: Posición en la cual se introducirá el carácter, y que a continuación será incrementada.
 * @param letra: Carácter a colocar en el buffer.
 */
void insert_item(int * final, char letra){
    buffer[*final] = letra;         // Se almacena el nuevo elemento.

    // Se imprime un mensaje de aviso junto a los contenidos del buffer
    // Se emplea el color verde, puesto que esta función solo es empleada por el productor.
    printf("%sPosición %d -> item %c%s\t\t\t\t\t\t\t\t\t\t\t\t\t", VERDE, *final, letra, RESET);
    log_buffer(1);

    // Incrementamos el valor de final, realizando el módulo por el número de posiciones del buffer para no
    // sobrepasar el límite de tamaño
    *final = (*final + 1) % N;
}

/*
 * Función que retira una letra del buffer. Dado que se implementa como una cola FIFO, se elimina del comienzo.
 * Dicha letra es devuelta por la función y reemplazada por un espacio en blanco.
 * Esta función es empleada por el consumidor.
 * @param inicio: Posición de la cual se eliminará la letra.
 */
char remove_item(int * inicio){
    char item;         // Letra que contenía el buffer en la posición inicio.

    item = buffer[*inicio];          // Leemos el elemento que había en el buffer
    buffer[*inicio] = ' ';           // Reemplazamos el valor de esa posición por un espacio en blanco
    *inicio = (*inicio + 1) % N;     // Incrementamos inicio utilizando un módulo para no sobrepasar el límite (15)
    return item;                     // Devolvemos el elemento leído
}

/*
 * Función que muestra el carácter que fue retirado del buffer por la función remove_item.
 * Esta función es empleada por el consumidor, que imprime en color azul.
 * @param item: Elemento a imprimir y que fue previamente retirado del buffer.
 */
void consume_item(char item){
    printf("\t\t\t\t\t\t%sConsumido item %c%s\t\t\t\t\t\t\t", AZUL, item, RESET);
    log_buffer(0);       // Imprimimos también los contenidos del buffer (0 indica que este proceso es el consumidor)
}

/*
 * Función que imprime los contenidos del buffer en una línea. Es empleada por el productor, que imprime en verde,
 * y por el consumidor, que imprime en azul. Toda la cadena aparece en cursiva.
 * @param proceso: Valdrá !0 si se trata del proceso productor, y 0 si es el consumidor.
 */
// Productor = !0, consumidor = 0
void log_buffer(int proceso){
    int i;          // Variable de iteración

    // Dependiendo del argumento proceso, se imprime en verde o en azul. Siempre se activa la cursiva.
    printf("%s%sbuffer = [", proceso? VERDE : AZUL, CURSIVA);
    for (i = 0; i < N - 1; i++)
        printf("%c ", *(buffer + i));           // Leemos uno a uno los caracteres contenidos en el buffer
    printf("%c]%s%s\n", *(buffer + i), RESET, RESET_CURS);
    // No utilizamos \n hasta el final para tratar de que el buffer se imprima de forma atómica
}

/*
 * Función que cierra una región de memoria para el proceso que la llama.
 */
void cerrar_mem_compartida(){
    // Utilizamos munmap, indicando el puntero a la región y el tamaño de esta (N caracteres)
    if (munmap((void *) buffer, (size_t) N * sizeof(char)) == -1)
        cerrar_con_error("Error: no se ha podido cerrar la proyección del área compartida entre los procesos", 1);
}

/*
 * Función auxiliar que cierra los semáforos empleados por un proceso.
 * Esta función es utilizada por el proceso padre, así como por el productor y el consumidor.
 * @param vacias: Semáforo que representa el número de posiciones vacías en el buffer y tiene nombre "PC_VACIAS".
 * @param mutex: Semáforo que representa la posibilidad de acceder a la región crítica y tiene nombre "PC_MUTEX".
 * @param llenas: Semáforo que representa el número de posiciones usadas en el buffer y tiene nombre "PC_LLENAS".
 */
void cerrar_semaforos(sem_t * vacias, sem_t * mutex, sem_t * llenas){
    // Si sem_close falla, imprimimos errno y cortamos la ejecución.
    if (sem_close(vacias)) cerrar_con_error("No se pudo cerrar el semaforo PC_VACIAS", 1);
    if (sem_close(mutex)) cerrar_con_error("No se pudo cerrar el semaforo PC_MUTEX", 1);
    if (sem_close(llenas)) cerrar_con_error("No se pudo cerrar el semaforo PC_LLENAS", 1);
}

/*
 * Función auxiliar que destruye los semáforos empleados en el programa.
 */
void destruir_semaforos(){
    if (sem_unlink("PC_VACIAS")) cerrar_con_error("No se pudo destruir el semáforo PC_VACIAS", 1);
    if (sem_unlink("PC_MUTEX")) cerrar_con_error("No se pudo destruir el semáforo PC_MUTEX", 1);
    if (sem_unlink("PC_LLENAS")) cerrar_con_error("No se pudo destruir el semáforo PC_LLENAS", 1);
}

/*
 * Función auxiliar que imprime un mensaje de error y finaliza la ejecución.
 * @param mensaje: descripción personalizada del error que será imprimido.
 * @param ver_errno: valdrá !0 para indicar el argumento "mensaje" junto a la descripción de errno, y 0 para mostrar
 *                   solo el mensaje.
 */
void cerrar_con_error(char * mensaje, int ver_errno){
    /*
     * Si ver_errno es !0, se indica qué ha ido mal en el sistema a través de la macro errno. Se imprime mensaje
     * seguido de ": " y el significado del código que tiene errno.
     */
    if (ver_errno) perror(mensaje);
    else fprintf(stderr, "%s", mensaje);    // Solo se imprime mensaje
    exit(EXIT_FAILURE);                     // Cortamos la ejecución del programa
}

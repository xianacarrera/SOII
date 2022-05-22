#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/* Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica Optativa - Problema de los filósofos con mutexes y variables
 * de condición.
 *
 * Este programa implementa una solución al problema de los filósofos 
 * emplando semáforos como mecanismo de sincronización.
 * 
 * Se debe compilar con la opción -pthread.
 */



#define MAX_ITER 10                 // Número de iteraciones máximas del programa
#define MAX_SLEEP 3                 // Número máximo de segundos que puede durar un sleep

// Macros que simbolizan al filósofo a la izquierda y a la derecha en la mesa, empleando su id
#define IZQUIERDO (id+N-1)%N
#define DERECHO (id+1)%N

// Estados de los filósofos 
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2      

// Control de la consola
#define COLOR "\033[0;%dm"          // String que permite cambiar el color de la consola
#define RESET "\033[0m"             // Reset del color de la consola
#define MOVER_A_COL "\r\033[64C"    // Mueve el cursor de la consola a la columna 64


int N;                             // Número de filósofos (es introducido por el usuario)

int * estado;                      // Estado de cada filósofo (pensando, hambriento o comiendo)

pthread_mutex_t mutex;             // Mutex de acceso a la región crítica
pthread_cond_t * conds;            // Cada filósofo tiene una variable de condición



// Funciones principales
void * filosofo(void * ptr_id);
void probar(int id);
void tomar_tenedores(int id);
void poner_tenedores(int id);
void pensar();
void comer(int id);

// Funciones de impresión
void log_consola(int id, char * msg);
char * ver_estados();

// Funciones auxiliares
void crear_hilo(pthread_t * hilo, int i);
void unirse_a_hilo(pthread_t hilo);
void inicializar_mutex_varcon();
void destruir_mutex_varcon();
void salir_con_error(char * mensaje, int ver_errno);


int main(){
    pthread_t * hilos;      // Filósofos del programa
    int i;

    // Solicite al usuario el número de filósofos (N)
    printf("Introduce el numero de filosofos ó -1 para salir ");    
    scanf("%d", &N);
    if (N == -1){
        printf("Cerrando programa...\n");
        exit(EXIT_SUCCESS);
    }
    while (N < 0){
        printf("El numero de filosofos debe ser mayor o igual que 1\n");
        printf("Introduce el numero de filosofos ó -1 para salir ");
        scanf("%d", &N);
        if (N == -1){
            printf("Cerrando programa...\n");
            exit(EXIT_SUCCESS);
        }
    }

    srand(time(NULL));              // Semilla para la generación de números aleatorios

    // Reservamos memoria para el arary de filósofos
    if ((hilos = (pthread_t *) malloc(N * sizeof(pthread_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los hilos\n", 0);
    // Reservamos memoria para el array de variables de condición (hay una por filósofo)
    if ((conds = (pthread_cond_t *) malloc(N * sizeof(pthread_cond_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para las variables de condicion\n", 0);
    // Reservamos memoria para el array de estados
    if ((estado = (int *) malloc(N * sizeof(int))) == NULL)
        salir_con_error("No se ha podido reservar memoria para el estado de los filosofos\n", 0);
        
    // Inicialmente todos los filósofos están pensando
    for (i = 0; i < N; i++) estado[i] = PENSANDO;    

    printf("\n");
    printf("Estados posibles para los filósofos:\n");
    printf("  P: Pensando\n");
    printf("  H: Hambriento\n");
    printf("  C: Comiendo\n\n");


    // Se inicializan el mutex y las variables de condicion
    inicializar_mutex_varcon();

    // Se crean los filósofos
    for (i = 0; i < N; i++) crear_hilo(&hilos[i], i);
    // El hilo principal espera a que todos los filósofos terminen antes de continuar
    for (i = 0; i < N; i++) unirse_a_hilo(hilos[i]);

    // Se destruye el mutex y las variables de condicion
    destruir_mutex_varcon();

    // Liberamos la memoria reservada
    free(hilos);
    free(conds);
    free(estado);

    printf("\n\nEjecución finalizada. Cerrando programa...\n\n");
    
    exit(EXIT_SUCCESS);
}


/*
 * Función que representa el ciclo de vida de un filósofo. Recibe como argumento el identificador que utilizará
 * a lo largo de su ejecución (su número de filósofo).
 */
void * filosofo(void * ptr_id){
    int id = (intptr_t) ptr_id;   // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)
                                  // id va de 0 a N-1
    int i;

    // Al contrario de lo que ocurría con los semáforos, no es necesario que los 
    // filósofos vuelvan a abrir los mutexes o las variables de condición que ya inicializó el hilo principal.

    // Realizamos un número finito de iteraciones para controlar el tiempo de ejecución
    for (i = 0; i < MAX_ITER; i++){
        pensar();              // El filósofo no actúa
        tomar_tenedores(id);   // El filósofo toma ambos tenedores o queda bloqueado esperando
        comer(id);             // El filósofo espera mientras sostiene ambos tenedores
        poner_tenedores(id);   // El filósofo devuelve los 2 tenedores a la mesa
    }

    // Tampoco es necesario que los filósofos cierren los mutexes y las variables
    // de condición; se encargará el hilo principal.

    pthread_exit((void *) "Hilo finalizado correctamente");
}


/*
 * Se comprueba si el filósofo de número id puede comer. Si puede, lo hace. Si no, se bloquea hasta que pueda.
 */
void probar(int id){
    /*
     * Se comprueba que el filósofo esté hambriento (que implica que no está comiendo, en cuyo caso ya tendría los tenedores, y que quiere comer) y que ni el filósofo de su izquierda está comiendo ni el de su derecha (por lo que sus tenedores están libres).
     *
     * Es importante verificar que el estado del hilo sea HAMBRIENTO porque puede que sea alguno de sus vecinos quien esté llamando a esta función. Si no se verificara, los vecinos tendrían la capacidad de obligarle a comer cuando en realidad él aún no quiere.
     */
    if (estado[id] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && estado[DERECHO] != COMIENDO){
        estado[id] = COMIENDO;      // El filósofo ya no deja que ninguno de sus vecinos tome sus tenedores.
        pthread_cond_signal(&conds[id]);    // El filósofo que ha llamado a esta función señala a su vecino para que
        // salga del bloqueo de la variable de condición, si estaba en él (al no haber estado los tenedores libres cuando llamó a probar), y comience a comer.
    }
}


/*
 * El filósofo trata de tomar los tenedores de su izquierda y de su derecha. Si no puede, se bloquea hasta que pueda.
 */
void tomar_tenedores(int id){
    pthread_mutex_lock(&mutex);       // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores (es decir, si el mutex ya está tomado).
    log_consola(id, "Quiere tomar tenedores");
    estado[id] = HAMBRIENTO;  // Registra que quiere tomar los tenedores
    probar(id);             // Comprueba si el filósofo puede comer (él está hambriento y sus vecinos no están comiendo, es decir, tienen libres los tenedores)

    /* A diferencia del caso de los semáforos, esta comprobación debe realizarse desde dentro de la región crítica,
     * porque está asociada al mutex de la misma, y debe ser liberado en caso de que el filósofo pueda continuar.
     * No es necesario emplear un while. Basta un if, porque los filósofos no pueden decrementar el estado de otros,
     * solo incrementarlo (de HAMBRIENTO a COMIENDO).   
     */
    if (estado[id] != COMIENDO){        // Al probar, el filósofo ha visto que alguno de sus tenedores están bloqueados
        // need to already have the lock before calling this
        // release the lock, so that other threads can use shared data
        pthread_cond_wait(&conds[id], &mutex);
    }

    pthread_mutex_unlock(&mutex);    // Sale de la región crítica liberando el mutex.
}

/*
 * Tras comer, el filósofo deja sus tenedores en la mesa, dejándoselos disponibles a sus vecinos.
 */
void poner_tenedores(int id){
    pthread_mutex_lock(&mutex);           // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores.
    log_consola(id, "Va a dejar sus tenedores");
    estado[id] = PENSANDO;    // El filósofo está ocioso. No está comiendo ni quiere tomar tenedores.
    probar(IZQUIERDO);      // Si el filósofo de la izquierda quiere comer y su tenedor izquierdo está libre, se le cede el tenedor derecho y se le permite comer (deja de esperar su turno).
    if (estado[IZQUIERDO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino izquierdo y este come");
    probar(DERECHO);        // Análog_consolao con el filósofo de la derecha.
    if (estado[DERECHO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino derecho y este come");
    pthread_mutex_unlock(&mutex);        // Sale de la región crítica. Se dedicará a pensar.
}


// El filósofo queda bloqueado durante un tiempo aleatorio (como máximo, MAX_SLEEP segundos)
void pensar(){
   sleep(rand() % MAX_SLEEP);
}

/* 
 * El filósofo anuncia que está comiendo y queda bloqueado un tiempo aleatorio (como máximo, MAX_SLEEP segundos)
 */
void comer(int id){
    log_consola(id, "Está comiendo");
    sleep(rand() % MAX_SLEEP);
}


/*
 * Función que construye una cadena con el estado actual de cada filósofo, para poder imprimirla.
 */
char * ver_estados(){
    int i;
    char * estados = (char *) malloc(N*sizeof(char));

    for (i = 0; i < N; i++){
        switch (estado[i]){
            case PENSANDO:
                estados[i] = 'P';
                break;
            case HAMBRIENTO:
                estados[i] = 'H';
                break;
            case COMIENDO:
                estados[i] = 'C';
                break;
            default:
                estados[i] = '?';   // Error
        }
    }
    return estados;
}

/*
 * Función que imprime el un mensaje para el filósofo que se encuentra en ejecución (de identificador id), junto
 * al estado de cada filósofo en el momento actual de ejecución.
 */
void log_consola(int id, char * msg) {
    // Si el id es menor que 16, se usan los colores originales de la consola. Si no, se dan saltos de 24 en función del id (para dar más variedad a los colores de ids próximos). El total debe estar en módulo 256.
    int color = (id > 15)? ((id % 15) + (id / 24)) % 256  : id;

    // Con la macro MOVER_A_COL nos colocamos en una columna a la derecha para imprimir los estados de los filósofos
    printf(COLOR "[%d]: %s" MOVER_A_COL "%s" RESET "\n", color, id, msg, ver_estados());
}



/*
 * Función auxiliar que crea un nuevo hilo, esto es, un nuevo filósofo. Este recibe como argumento de su 
 * función un identificador "i". El identificador real del pthread, "hilo", se devuelve por referencia.
 */
void crear_hilo(pthread_t * hilo, int i){
    int error;

    // El argumento debe enviarse como un puntero a void, y no como un entero. Para realizar una conversión segura,
    // pasamos primero el dato a intptr_t y después a un puntero a void.
    // Pasamos NULL como segundo argumento para no modificar los atributos por defecto del hilo
    if ((error = pthread_create(hilo, NULL, filosofo, (void *) (intptr_t) i)) != 0){
        fprintf(stderr, "Error %d al crear un hilo: %s\n", error, strerror(error));
        exit(EXIT_FAILURE);
    }
}



/*
 * Función auxiliar que provoca que el hilo principal quede esperando hasta que finalice el hilo cuyo 
 * identificador se pasa como argumento.
 */
void unirse_a_hilo(pthread_t hilo){
    int error;
    char * exit_hilo;

    if ((error = pthread_join(hilo, (void **) &exit_hilo)) != 0){
            fprintf(stderr, "Error %d al unirse a un hilo: %s\n", error, strerror(error));
            exit(EXIT_FAILURE);
    }
}

/*
 * Función auxiliar que inicializa los mutexes y las variables de condición.
 */
void inicializar_mutex_varcon(){
    int i;

    // Antes de poder emplear los mutexes en las funciones pthread_mutex_lock, pthread_mutex_unlock, etc., deben ser
    // inicializados. Para ello, utilizamos la función pthread_mutex_init
    // Dejamos el segundo argumento a NULL para emplear la configuración de atributos por defecto.
    if (pthread_mutex_init(&mutex, NULL))
        salir_con_error("Error en la inicializacion del mutex de la region critica\n", 0);

    for (i = 0; i < N; i++)
        // De forma análoga, inicializamos las variables de condición empleando pthread_cond_init, indicando los atributos por defecto.
        if (pthread_cond_init(&conds[i], NULL))
            salir_con_error("Error en la inicializacion de la variable de condición de un filosofo\n", 0);
}

/*
 * Función auxiliar que destruye los mutexes empleados en el programa, así como
 * las variables de condición.
 */
void destruir_mutex_varcon(){    
    int i;              // Contador de iteración

    // Una vez ha finalizado el uso de los mutexes y de las variables de condición, se pueden destruir con seguridad.
    // Llamar a destruir las desinicializará. Tendremos la seguridad de que todos los mutexes estarán desbloqueados y las
    // variables de condición, liberadas, una vez todos los hilos hayan finalizado correctamente (en caso contrario,
    // pthread_cond_destroy podría dar error)
    for (i = 0; i < N; i++)
        if (pthread_cond_destroy(&conds[i]))
            salir_con_error("Error en la destruccion de la variable de condición de un filósofo\n", 0);

    // Hacemos lo mismo con el mutex de la región crítica
    // Tenemos la seguridad de que estará desbloqueado una vez todos
    // los hilos hayan finalizado correctamente (en caso contrario, pthread_mutex_destroy podría dar error)
    if (pthread_mutex_destroy(&mutex))
        salir_con_error("Error en la destruccion del mutex de la region critica\n", 0);
}

/*
 * Función auxiliar que cierra el programa en caso de error
 */
void salir_con_error(char * mensaje, int ver_errno){
    /*
     * Si ver_errno es !0, se indica qué ha ido mal en el sistema a través de la macro errno. Se imprime mensaje
     * seguido de ": " y el significado del código que tiene errno.
     */
    if (ver_errno) perror(mensaje);
    else fprintf(stderr, "%s", mensaje);    // Solo se imprime mensaje
    exit(EXIT_FAILURE);                     // Cortamos la ejecución del programa
}

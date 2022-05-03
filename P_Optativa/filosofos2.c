#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

// TODO: comentar este código


#define IZQUIERDO (id+N-1)%N
#define DERECHO (id+1)%N
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2
#define MAX_ITER 10
#define COLOR "\033[0;%dm"
#define RESET "\033[0m"
#define MOVER_A_COL "\r\033[64C"
//#define POSICION "\033[y;xH" moves curser to row y, col x:"

int N;

int * estado;

pthread_mutex_t mutex;             // Mutex de acceso a la región crítica
pthread_mutex_t * m;               // Cada filósofo tiene un mutex
pthread_cond_t * conds;            // Cada filósofo tiene una variable de condición


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

void log_consola(int id, char * msg) {
    // Si el id es menor que 16, se usan los colores originales de la consola. Si no, se dan saltos de 24 en función del id (para dar más variedad a los colores de ids próximos). El total debe estar en módulo 256.
    int color = (id > 15)? ((id % 15) + (id / 24)) % 256  : id;
    printf(COLOR "[%d]: %s" MOVER_A_COL "%s" RESET "\n", color, id, msg, ver_estados());
}


void salir_con_error(char * mensaje, int ver_errno){
    /*
     * Si ver_errno es !0, se indica qué ha ido mal en el sistema a través de la macro errno. Se imprime mensaje
     * seguido de ": " y el significado del código que tiene errno.
     */
    if (ver_errno) perror(mensaje);
    else fprintf(stderr, "%s", mensaje);    // Solo se imprime mensaje
    exit(EXIT_FAILURE);                     // Cortamos la ejecución del programa
}

void pensar(){
   sleep(1);
}

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

void comer(int id){
    log_consola(id, "Está comiendo");
    sleep(1);
}

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

// Función auxiliar que inicializa los mutexes
void inicializar_mutexes(){
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

// Función auxiliar que destruye los mutexes
void destruir_mutexes(){
    int i;

    // Una vez ha finalizado el uso de los mutexes y de las variables de condición, se pueden destruir con seguridad.
    // Llamar a destruir las desinicializará. Tenemos la seguridad de que todos los mutexes están desbloqueados y las
    // variables de condición, liberadas, pues todos los hilos han finalizado correctamente (en caso contrario,
    // estas funciones podrían dar error)
    for (i = 0; i < N; i++)
        if (pthread_cond_destroy(&conds[i]))
            salir_con_error("Error en la destruccion de la variable de condición de un filósofo\n", 0);

    // Una vez ha finalizado el uso de los mutexes, se pueden destruir con seguridad.
    // Tenemos la seguridad de que todos los mutexes están desbloqueados, pues todos los hilos han finalizado
    // correctamente (en caso contrario, pthread_mutex_destroy podría dar error)
    if (pthread_mutex_destroy(&mutex))
        salir_con_error("Error en la destruccion del mutex de la region critica\n", 0);
}


void * filosofo(void * ptr_id){
    int id = (intptr_t) ptr_id;   // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)
                                  // id va de 0 a N-1
    int i;

    // Realizamos un número finito de iteraciones
    for (i = 0; i < MAX_ITER; i++){
        pensar();              // El filósofo no actúa
        tomar_tenedores(id);   // El filósofo toma ambos tenedores o queda bloqueado esperando
        comer(id);             // El filósofo espera mientras sostiene ambos tenedores
        poner_tenedores(id);   // El filósofo devuelve los 2 tenedores a la mesa
    }

    pthread_exit((void *) "Hilo finalizado correctamente");
}


void crear_hilo(pthread_t * hilo, int i){
    int error;

    // El argumento debe enviarse como un puntero a void, y no como un entero. Para realizar una conversión segura,
    // pasamos primero el dato a intptr_t y después a un puntero a void.
    // Pasamos NULL como segundo argumento para no modificar los atributos por defecto del hilo
    if ((error = pthread_create(hilo, NULL, filosofo, (void *) (intptr_t) i)) != 0){
        fprintf(stderr, "Error %d al crear un hilo: %s\n", error, strerror(error));
        //cerrar_semaforos(vacias, mutex, llenas);
        //destruir_semaforos();
        exit(EXIT_FAILURE);
    }
}


void unirse_a_hilo(pthread_t hilo){
    int error;
    char * exit_hilo;

    if ((error = pthread_join(hilo, (void **) &exit_hilo)) != 0){
            fprintf(stderr, "Error %d al unirse a un hilo: %s\n", error, strerror(error));
            //destruir_semaforos();
            exit(EXIT_FAILURE);
    }
}


int main(){
    int i;
    int error;
    pthread_t * hilos;

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

    if ((hilos = (pthread_t *) malloc(N * sizeof(pthread_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los hilos\n", 0);
    if ((m = (pthread_mutex_t *) malloc(N * sizeof(pthread_mutex_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los semaforos\n", 0);
    if ((conds = (pthread_cond_t *) malloc(N * sizeof(pthread_cond_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para las variables de condicion\n", 0);
    if ((estado = (int *) malloc(N * sizeof(int))) == NULL)
        salir_con_error("No se ha podido reservar memoria para el estado de los filosofos\n", 0);
    

    // Se inicializan los mutexes y las variables de condicion
    inicializar_mutexes();

    for (i = 0; i < N; i++) crear_hilo(&hilos[i], i);
    for (i = 0; i < N; i++) unirse_a_hilo(hilos[i]);

    // Se destruyen los mutexes y las variables de condicion
    destruir_mutexes();
    
    exit(EXIT_SUCCESS);
}
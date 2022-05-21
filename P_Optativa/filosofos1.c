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
 * Práctica Optativa - Problema de los filósofos con semáforos
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


#define COLOR "\033[0;%dm"          // String que permite cambiar el color de la consola
#define RESET "\033[0m"             // Reset del color de la consola
#define MOVER_A_COL "\r\033[64C"    // Mueve el cursor de la consola a la columna 64


int N;                   // Número de filósofos (es introducido por el usuario)

int * estado;            // Estado de cada filósofo (pensando, hambriento o comiendo)
sem_t * mutex = NULL;    // Semáforo que da acceso exclusivo a la región crítica (donde se toman o liberan los tenedores)
sem_t ** s;              // Cada filósofo tiene un semáforo


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
void crear_semaforos();
void destruir_semaforos();
void abrir_semaforos();
void cerrar_semaforos();
void salir_con_error(char * mensaje, int ver_errno);



int main(){
    pthread_t * hilos;              // Filósofos del programa
    int i;                          // Contador de iteraciones


    // Pedimos que se introduzca el valor de N 
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


    // Reservamos memoria para el array de filósofos
    if ((hilos = (pthread_t *) malloc(N * sizeof(pthread_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los hilos\n", 0);
    // Reservamos memoria para el array de semáforos
    if ((s = (sem_t **) malloc(N * sizeof(sem_t *))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los semaforos\n", 0);
    // Reservamos memoria para el array de estados 
    if ((estado = (int *) malloc(N * sizeof(int))) == NULL)
        salir_con_error("No se ha podido reservar memoria para el estado de los filosofos\n", 0);
    
    printf("\n");
    printf("Estados posibles para los filósofos:\n");
    printf("  P: Pensando\n");
    printf("  H: Hambriento\n");
    printf("  C: Comiendo\n\n");

    // Como medida cautelar, destruimos los semáforos que pudiera haber en el sistema antes de volverlos
    // a crear. Así, el hilo principal los deja disponibles para que sus hijos los empleen.
    destruir_semaforos();
    crear_semaforos();

    // Creamos los N filósofos
    for (i = 0; i < N; i++) crear_hilo(&hilos[i], i);
    // El hilo principal espera a que todos los filósofos acaben antes de finalizar él
    for (i = 0; i < N; i++) unirse_a_hilo(hilos[i]);

    cerrar_semaforos();         // El hilo principal cierra los semáforos (que nunca llega a usar)
    destruir_semaforos();       // Volvemos a destruir los semáforos para que no se queden en el sistema

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
    int i;                        // Contador de iteraciones

    abrir_semaforos();          // El filósofo abre los semáforos para tener acceso a ellos

    // Realizamos un número finito de iteraciones para controlar el tiempo de ejecución
    for (i = 0; i < MAX_ITER; i++){
        pensar();              // El filósofo no actúa
        tomar_tenedores(id);   // El filósofo toma ambos tenedores o queda bloqueado esperando
        comer(id);             // El filósofo espera mientras sostiene ambos tenedores
        poner_tenedores(id);   // El filósofo devuelve los 2 tenedores a la mesa
    }

    cerrar_semaforos();         // El filósofo cierra los semáforos que ha empleado

    pthread_exit((void *) "Hilo finalizado correctamente");
}


/*
 * El filósofo comprueba si puede comer. Si puede, lo hace. Si no, se bloquea hasta que pueda.
 */
void probar(int id){
    /*
     * Se comprueba que el filósofo esté hambriento (que implica que no está comiendo, en cuyo caso ya tendría los tenedores, y que quiere comer) y que ni el filósofo de su izquierda está comiendo ni el de su derecha (por lo que sus tenedores están libres).
     *
     * Es importante verificar que el estado del hilo sea HAMBRIENTO porque puede que sea alguno de sus vecinos quien esté llamando a esta función. Si no se verificara, los vecinos tendrían la capacidad de obligarle a comer cuando en realidad él aún no quiere.
     */
    if (estado[id] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && estado[DERECHO] != COMIENDO){
        estado[id] = COMIENDO;      // El filósofo ya no deja que ninguno de sus vecinos tome sus tenedores.
        sem_post(s[id]);   // Este semáforo actúa a modo de 'return' para esta función. Le indicará al filósofo que puede comer.
    }
}

/*
 * El filósofo trata de tomar los tenedores de su izquierda y de su derecha. Si no puede, se bloquea hasta que pueda.
 */
void tomar_tenedores(int id){
    sem_wait(mutex);       // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores.
    log_consola(id, "Quiere tomar tenedores");
    estado[id] = HAMBRIENTO;  // Registra que quiere tomar los tenedores
    probar(id);             // Comprueba si el filósofo puede comer (él está hambriento y sus vecinos no están comiendo, es decir, tienen libres los tenedores)
    sem_post(mutex);        // Sale de la región crítica
    sem_wait(s[id]);        // Si el filósofo puede comer (al probar, ha comprobado que los tenedores están disponibles y se ha declarado como "COMIENDO"), su semáforo se habrá incrementado. Si no, seguirá a 0 y quedará bloqueado.
}

/*
 * Tras comer, el filósofo deja sus tenedores en la mesa, dejándoselos disponibles a sus vecinos.
 */
void poner_tenedores(int id){
    sem_wait(mutex);           // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores.
    log_consola(id, "Va a dejar sus tenedores");
    estado[id] = PENSANDO;    // El filósofo está ocioso. No está comiendo ni quiere tomar tenedores.
    probar(IZQUIERDO);      // Si el filósofo de la izquierda quiere comer y su tenedor izquierdo está libre, se le cede el tenedor derecho y se le permite comer (deja de esperar su turno).
    if (estado[IZQUIERDO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino izquierdo y este come");
    probar(DERECHO);        // Análogo con con el filósofo de la derecha.
    if (estado[DERECHO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino derecho y este come");
    sem_post(mutex);        // Sale de la región crítica. Se dedicará a pensar.
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
 * Función que crea los semáforos que se emplearán a lo largo del programa.
 */
void crear_semaforos(){
    int i;                      // Contador de iteraciones
    char * nombre_sem;          // Cadena con el nombre del semáforo

    // Semáforo de acceso a la región crítica
    // Lo creamos con todos los permisos para el usuario (0700)
    if ((mutex = sem_open("/F_MUTEX", O_CREAT, 0700, 1)) == SEM_FAILED)
        salir_con_error("Error: no se ha podido crear el semaforo mutex", 1);

    // Los semáforos de cada filósofo tendrán como nombre /F_1, /F_2, ..., /F_N,
    // donde N es el número de filósofos (asumimos N < 1000).
    // Construimos una cadena para cada nombre de semáforo
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        // Añadimos a nombre_sem el identificador del filósofo
        snprintf(&nombre_sem[3], 4, "%d", i);       

        // Creamos el semáforo
        if((s[i] = sem_open(nombre_sem, O_CREAT, 0700, 0)) == SEM_FAILED)
            salir_con_error("Error no se ha podido crear el semaforo de un filosofo", 1);   
    }
}

/*
 * Función que destruye los semáforos empleados a lo largo del programa.
 */
void destruir_semaforos(){
    int exit_unlink;            // Variable de retorno (error)
    int i;                      // Contador
    char * nombre_sem;          // Cadena de caracteres que contiene el nombre del semáforo

    // Destruimos mutex. Si el error se debe a que el semáforo no existía previamente, 
    // ignoramos el aviso.
    if ((exit_unlink = sem_unlink("/F_MUTEX")) && errno != ENOENT)
        salir_con_error("Error: permisos inadecuados para el semaforo mutex", 1);

    // Construimos la cadena con el nombre que el semáforo de cada filósofo tiene en el 
    // sistema: /F_1, /F_2, ..., /F_N
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        snprintf(&nombre_sem[3], 4, "%d", i);
        if((exit_unlink = sem_unlink(nombre_sem)) && errno != ENOENT)
            salir_con_error("Error: permisos inadecuados para el semaforo", 1);   
    }
}

/*
 * Función que abre los semáforos del programa para el hilo actual. Los semáforos deben haberse creado
 * previamente.
 */ 
void abrir_semaforos(){
    int i;                          // Variable de iteración
    char * nombre_sem;              // Cadena para almacenar el nombre de los semáforos    

    // El productor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    mutex = sem_open("/F_MUTEX", 0);

    // Abrimos el resto de los semáforos, para lo cual debemos construir la cadena con su nombre
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        snprintf(&nombre_sem[3], 4, "%d", i);
        sem_open(nombre_sem, 0); 
    }
}

/*
 * Función auxiliar que ejecuta sem_close() sobre los semáforos que emplea el programa
 */
void cerrar_semaforos(){
    int i;                          // Variable de iteración
    char * nombre_sem;              // Cadena para almacenar el nombre de los semáforos    

    // El productor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    if (sem_close(mutex)) salir_con_error("Error: no se ha podido cerrar el semaforo mutex", 1);

    // Para cada semáforo construimos la cadena de su nombre
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        if (sem_close(s[i])) salir_con_error("Error: no se ha podido cerrar el semaforo", 1);
    }
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
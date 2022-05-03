#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>


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
sem_t * mutex = NULL;    // Semáforo que da acceso exclusivo a la región crítica (donde se toman o liberan los tenedores)
sem_t ** s;              // Cada filósofo tiene un semáforo


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
        sem_post(s[id]);   // Este semáforo actúa a modo de 'return' para esta función. Le indicará al filósofo que puede comer.
    }
}

void comer(int id){
    log_consola(id, "Está comiendo");
    sleep(1);
}

void tomar_tenedores(int id){
    sem_wait(mutex);       // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores.
    log_consola(id, "Quiere tomar tenedores");
    estado[id] = HAMBRIENTO;  // Registra que quiere tomar los tenedores
    probar(id);             // Comprueba si el filósofo puede comer (él está hambriento y sus vecinos no están comiendo, es decir, tienen libres los tenedores)
    sem_post(mutex);        // Sale de la región crítica
    sem_wait(s[id]);        // Si el filósofo puede comer (al probar, ha comprobado que los tenedores están disponibles y se ha declarado como "COMIENDO"), su semáforo se habrá incrementado. Si no, seguirá a 0 y quedará bloqueado.
}

void poner_tenedores(int id){
    sem_wait(mutex);           // El filósofo trata de acceder a la región crítica. Queda bloqueado si ya hay alguien cogiendo/dejando tenedores.
    log_consola(id, "Va a dejar sus tenedores");
    estado[id] = PENSANDO;    // El filósofo está ocioso. No está comiendo ni quiere tomar tenedores.
    probar(IZQUIERDO);      // Si el filósofo de la izquierda quiere comer y su tenedor izquierdo está libre, se le cede el tenedor derecho y se le permite comer (deja de esperar su turno).
    if (estado[IZQUIERDO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino izquierdo y este come");
    probar(DERECHO);        // Análog_consolao con el filósofo de la derecha.
    if (estado[DERECHO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino derecho y este come");
    sem_post(mutex);        // Sale de la región crítica. Se dedicará a pensar.
}

void abrir_semaforos(){
    int i;
    char * nombre_sem;

    // El productor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    mutex = sem_open("/F_MUTEX", 0);

        // Construir cadena
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        snprintf(&nombre_sem[3], 4, "%d", i);
        sem_open(nombre_sem, 0); 
    }
}

void cerrar_semaforos(){
        int i;
    char * nombre_sem;

        // El productor abre los semáforos para tener acceso a ellos, pero no los inicializa
    // Para cada semáforo se indica su nombre y 0 como segundo argumento, indicando que no se está creando
    if (sem_close(mutex)) salir_con_error("Error: no se ha podido cerrar el semaforo mutex", 1);



        // Construir cadena
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        if (sem_close(s[i])) salir_con_error("Error: no se ha podido cerrar el semaforo", 1);
    }
}


void filosofo(int id){
    int i;

    abrir_semaforos();

    // Realizamos un número finito de iteraciones
    for (i = 0; i < MAX_ITER; i++){
        pensar();              // El filósofo no actúa
        tomar_tenedores(id);   // El filósofo toma ambos tenedores o queda bloqueado esperando
        comer(id);             // El filósofo espera mientras sostiene ambos tenedores
        poner_tenedores(id);   // El filósofo devuelve los 2 tenedores a la mesa
    }

    cerrar_semaforos();

    // Al volver de esta función, el proceso finaliza inmediatamente
}



void destruir_semaforos(){
    int exit_unlink;
    int i;
    char * nombre_sem;

    if ((exit_unlink = sem_unlink("/F_MUTEX")) && errno != ENOENT)
        salir_con_error("Error: permisos inadecuados para el semaforo mutex", 1);

    // Construir cadena
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        snprintf(&nombre_sem[3], 4, "%d", i);
        if((exit_unlink = sem_unlink(nombre_sem)) && errno != ENOENT)
            salir_con_error("Error: permisos inadecuados para el semaforo", 1);   
    }
}

void crear_semaforos(){
    int i;
    char * nombre_sem;

    if ((mutex = sem_open("/F_MUTEX", O_CREAT, 0700, 1)) == SEM_FAILED)
        salir_con_error("Error: no se ha podido crear el semaforo mutex", 1);

    // Construir cadena
    nombre_sem = (char *) malloc(6 * sizeof(char));
    snprintf(nombre_sem, 4, "/F_");
    for (i = 0; i < N; i++){
        snprintf(&nombre_sem[3], 4, "%d", i);
        if((s[i] = sem_open(nombre_sem, O_CREAT, 0700, 0)) == SEM_FAILED)
            salir_con_error("Error no se ha podido crear el semaforo de un filosfiojdsofjdajodfjsiodfjsao", 1);   
    }
}



void crear_hijo(int i){
    int ret_fork;

    if ((ret_fork = fork()) == -1){
        salir_con_error("Error - no se ha podido crear un proceso hijo", 1);
    }
    else if (ret_fork){
        filosofo(i);
        exit(EXIT_SUCCESS);
    }
}


void esperar_hijos(){
    pid_t exit_wait;
    int status;

    // El proceso queda atrapado en un bucle hasta que todos sus hijos hayan finalizado (waitpid devolverá -1)
    while((exit_wait = waitpid(-1, &status, 0)) != -1){
        /*
        * Si WIFEXITED(status) es 0, el hijo no terminó con un exit() Sería el caso de finalización por una señal.
        * Si WIFEXITED(status) es 1, podemos comprobar WEXITSTATUS para determinar cuál fue el valor del exit del hijo.
        *
        * Si hay error, cortamos la ejecución tras imprimir un mensaje descriptivo.
        */
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
            salir_con_error("Finalización incorrecta de un proceso hijo", 0);
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
    if ((s = (sem_t **) malloc(N * sizeof(sem_t *))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los semaforos\n", 0);
    if ((estado = (int *) malloc(N * sizeof(int))) == NULL)
        salir_con_error("No se ha podido reservar memoria para el estado de los filosofos\n", 0);
    

    destruir_semaforos();
    crear_semaforos();

    for (i = 0; i < N; i++) crear_hijo(i);
    esperar_hijos();

    cerrar_semaforos();
    destruir_semaforos();
    
    exit(EXIT_SUCCESS);
}
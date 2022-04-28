#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


#define IZQUIERDO (id+N-1)%N
#define DERECHO (id+1)%N
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2

int N;

int * estado;
sem_t * mutex = NULL;
sem_t ** s;


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
    if(estado[id] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && estado[DERECHO] != COMIENDO){
        estado[id] = COMIENDO;
        sem_post(s[id]);
    }
}

void comer(){
    sleep(2);
}

void tomar_tenedores(int id){
    sem_wait(mutex);
    estado[id] = HAMBRIENTO;
    probar(id);
    sem_post(mutex);
    sem_wait(s[id]);
}

void poner_tenedores(int id){
    sem_wait(mutex);
    estado[id] = PENSANDO;
    probar(IZQUIERDO);
    probar(DERECHO);
    sem_post(mutex);
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




void * filosofo(void * ptr_id){
    int id = (intptr_t) ptr_id;   // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)

    abrir_semaforos();

    while(1){
        pensar();
        tomar_tenedores(id);
        comer();
        poner_tenedores(id);
    }

    cerrar_semaforos();
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

    // TODO: Comprobar reserva de memmoria
    hilos = (pthread_t *) malloc(N * sizeof(pthread_t));
    s = (sem_t **) malloc(N * sizeof(sem_t *));
    estado = (int *) malloc(N * sizeof(int));

    destruir_semaforos();
    crear_semaforos();

    for (i = 0; i < N; i++){
        crear_hilo(&hilos[i], i);
    }

    for (i = 0; i < N; i++){
        unirse_a_hilo(hilos[i]);
    }
    
    exit(EXIT_SUCCESS);
}
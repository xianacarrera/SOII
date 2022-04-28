#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define IZQUIERDO (i+N-1)%N
#define DERECHO (i+1)%N
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2

int N;

int * estado;
sem_t * mutex = NULL;
sem_t ** s;

void pensar(){
   sleep(1);
}

void comer(){
    sleep(2);
}

void tomar_tenedores(int i){
    sem_wait(&mutex);
    estado[i] = HAMBRIENTO;
    probar(i);
    sem_post(&mutex);
    sem_wait(&s[i]);
}

void poner_tenedores(int i){
    sem_wait(&mutex);
    estado[i] = PENSANDO;
    probar(IZQUIERDO);
    probar(DERECHO);
    sem_post(&mutex);
}

void probar(int i){
    if(estado[i] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && estado[DERECHO] != COMIENDO){
        estado[i] = COMIENDO;
        sem_post(&s[i]);
    }
}


void filosofo(int i){
    while(1){
        pensar();
        tomar_tenedores(i);
        comer();
        poner_tenedores(i);
    }
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

void destruir_semaforos(){
    int exit_unlink;
    int i;
    char * nombre_sem;

    if ((exit_unlink = sem_unlink("/F_MUTEX")) && errno != ENOENT)
        salir_con_error("Error: permisos inadecuados para el semaforo F_MUTEX", 1);

    // Construir cadena
    nombre_sem = (char *) malloc(4 * sizeof(char));
    for (i = 0; i < N; i++){
        
        if((exit_unlink = sem_unlink("/F"))
    }
}


void crear_hilo(pthread_t * hilo){
    int error;

    if ((error = pthread_create(hilo, NULL, filosofo, NULL)) != 0){
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
    if (N == -1) exit(EXIT_SUCCESS);
    while (N < 0){
        printf("El numero de filosofos debe ser mayor o igual que 1\n");
        printf("Introduce el numero de filosofos ó -1 para salir ");
        scanf("%d", &N);
        if (N == -1) exit(EXIT_SUCCESS);
    }

    hilos = (pthread_t *) malloc(N * sizeof(pthread_t));
    s = (sem_t **) malloc(N * sizeof(sem_t *));
    estado = (int *) malloc(N * sizeof(int));

    for (i = 0; i < N; i++){
        crear_hilo(&hilos[i]);
    }

    for (i = 0; i < N; i++){
        unirse_a_hilo(hilos[i]);
    }

    
    exit(EXIT_SUCCESS);
}
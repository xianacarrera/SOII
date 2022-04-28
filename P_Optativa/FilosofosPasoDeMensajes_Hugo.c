#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mqueue.h>

/* Se definirán a continuación una serie de constantes y variables globales que
se utilizarán a lo largo del código: */

//En primero lugar, los 3 estados en los que se pueden encontrar los filósofos:
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2

//A continuación el número de filósofios implicados:
int N;

//Se identificarán los filósofos vecinos de la siguiente forma:
#define IZQUIERDO (i + N - 1) % N
#define DERECHO (i + 1) % N

//Se emplearán colores:
#define CYAN "\033[0;36m"
#define BLUE "\033[0;34m"
#define YELLOW "\033[0;33m"
#define ns "\033[35m"
#define RESET "\033[0m"

// Se empleará pase de mensajes por lo que se definen los buffers:
mqd_t entradaRegion; //-> Control del acceso a la región crítica
mqd_t *filosofo;     //-> Colas que usarán los filósofos para los mensajes

int *estado; //-> Se almacenará el estado de cada filósofo

/* Siendo fiel al código definido por Tanenbaum se implementarán las siguientes
funciones: */
void pensar();
void tomarTenedores(int i);
void comer();
void ponerTenedores(int i);
void probar(int i);
void *funcion(void *i);

int main(){
    char nombreCola[20];

    //Se pide al usuario que introduzca el número de filósofos que desea crear
    printf(BLUE "Introduce el número de filósofos a crear\n" RESET);
    printf(CYAN "|\n└---> ");
    scanf(" %d"RESET, &N);
   
    pthread_t filosofos[N];

    //Se reserva memoria para las colas de los filósofos:
    filosofo = (mqd_t*) malloc (N * sizeof(mqd_t));

    //Se declaró de forma globar un vector de estados para el que se reserva 
    //memoria. Además, los filósofos se consideran en estado pensante(0):
    estado = (int*) malloc(N * sizeof(int));
    for(int i = 0; i < N; i++) estado[i] =0; 

    struct mq_attr attrs;
    attrs.mq_maxmsg = 1; //Maximo número de mensajes = 1
    attrs.mq_msgsize = 30*sizeof(char); //Tamaño máximo del mensaje por lo alto

    //Se borran las colas (por si estaban ya creadas) y se crean de nnuevo:
    for(int i = 0; i < N; i++) {
        sprintf(nombreCola,"/FILOSOFO%i",i); //Para identificar las colas
        mq_unlink(nombreCola);
        if((filosofo[i] = mq_open(nombreCola, O_CREAT | O_RDWR, 0777, &attrs)) < 0){
            printf("Error en mq_open");   
            exit(-1);        
        }
    }

    //Lo mismo con la otra cola:
    mq_unlink("/REGION");
    if((entradaRegion = mq_open("/REGION", O_CREAT | O_RDWR, 0777, &attrs))<0){
        printf("Error en mq_open(2)");   
        exit(-1);        
    } 

    /*Siguiendo la práctica de pase de mensajes, llenamos el buffer 
    (esta vez 1 elemento es suficiente) */
    char solicitud[] = "solicitud...";
    if(mq_send(entradaRegion, solicitud, sizeof(solicitud), 0) < 0 ) {
        perror("Error en mq_send (solicitud)");
        exit(-1);
    }

    /*Se pide emplear threads por lo que se procede a crear los N hilos 
    implicados: */
    for(int i = 0; i < N; i++){
        if((pthread_create(&filosofos[i], NULL, funcion, (void*)i)) != 0){
            perror("Error en pthread_create");
            exit(-1);
        }

    }

    //Se espera a que todos terminen:
      for(int i = 0; i < N; i++){

        if((pthread_join(filosofos[i], NULL)) != 0){
            perror("Error en pthread_join");
            exit(-1);
        }

    }
    //Se cierran y borran las colas:
    for(int i = 0; i < N; i++){
        sprintf(nombreCola,"/FILOSOFO%i",i);

        if(mq_close(filosofo[i]) == -1){
            perror("Error en mq_close");
        }

        if(mq_unlink(nombreCola) == -1){
            perror("Error en mq_close");
        }

    }

    if(mq_close(entradaRegion) == -1){
        perror("Error en mq_close");
    }

    if(mq_unlink("/REGION") == -1){
        perror("Error en mq_close");
    }
    
    //Se libera la memoria reservada
    free(filosofo);
    free(estado);

    return 0;
}

void *funcion(void *i){
    //Se utiliza para representar el hilo que se encuentra trabajando
    int variable = (int) i; 
        
    while(1){
        printf(YELLOW"El filósofo %d se encuentra PENSANDO\n"RESET, variable);
        sleep(rand()%4);//-> Sería la función pensar();
        tomarTenedores(variable);
        printf(CYAN"El filósofo %d va a COMER\n"RESET, variable);
        sleep(rand()%4); //--> Sería la función comer();
        ponerTenedores(variable);
    }
}

void tomarTenedores(int i){
    printf(BLUE"El filósofo %d TOMA TENEDORES\n"RESET, i);
    char mensajeRecibido [30];

    /*Se emplea una llamada a receive para asegurar que nadie más entre en la
    región crítica y entra este filosofo*/
    if((mq_receive(entradaRegion, mensajeRecibido, sizeof(mensajeRecibido), NULL)) == -1){
        perror("problema en mq_receive");
        exit(-1);
    }

    estado[i] = HAMBRIENTO; //Se pone dicho filósofo en estado hambriento

    //Se prueba a comer:
    probar(i);

    //Se envía mensaje para indicar la salida de la región crítica:
    if((mq_send(entradaRegion,"B",sizeof(char),0)) == -1){
        perror("mq_send");
    }
    //Por último, si no se pudieron tomar los tenedores se bloquea:
    if((mq_receive(filosofo[i], mensajeRecibido, sizeof(mensajeRecibido),NULL)) == -1){
        perror("Error en mq_receive");
        exit(-1);
    }

}

void probar(int i){
    //Siguiendo el pseudocódigo de Tanenbaum:
    /* Si el filósofo en cuestión está hambriento y sus vecinos no se encuentran
    comiendo */
    if(estado[i] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && 
     estado[DERECHO] != COMIENDO){
        //Entonces se puede poner a comer:
        estado[i] = COMIENDO;
        //Para que no se bloquee en tomarTenedores() envío un mensaje:
        if((mq_send(filosofo[i], "A", sizeof(char), 0)) == -1){
            perror("Error en mq_send");
            exit(-1);
        }
    }
}

void ponerTenedores(int i){
    printf(ns"El filósofo %d PONE TENEDORES\n"RESET, i);
    char mensajeRecibido [30];

    //Se solicita acceso a la región crítica
    if ((mq_receive(entradaRegion, mensajeRecibido, sizeof(mensajeRecibido), NULL)) == -1) {
        perror("Error en mq_receive");
    }

    estado[i] = PENSANDO; // Cambio del estado del filósofo a pensando

    //Se corrobora si los vecinos pueden comer
    probar(IZQUIERDO);
    probar(DERECHO);

    //Se sale de la región crítica
    if(mq_send(entradaRegion, "A", sizeof(char), 0) == -1){
     perror("Error en mq_send");
     exit(-1);
    }
}


/*==Conclusiones:===============================================================
 Se emplea pase de mensajes como técnica para evitar carreras críticas. 
 Como se observa, los resultados obtenidos son satisfactorios. Se establecen N+1 
 colas para poder implementar el problema de los filósofos (una por hilo y una
 extra para poder gestionar la entrada a la región crítica). 
 
 Esta última actúa a modo de "mutex" ya que se trató de ser lo más fiel posible
 a la solución de Tanenbaum. Es una de las muchas formas de implementar esta 
 solución; no obstante, el pase de mensajes no es la solución más óptima y 
 existen otras estrategias más sofisticadas. 
 
 Por último, cabe destacar que el funcionamiento
 no se ve afectado por el número de hilos ya que funciona correctamente para los 
 diferentes valores empleados a la hora de realizar pruebas.
================================================================================*/
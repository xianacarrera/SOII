#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/* Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica Optativa - Problema de los filósofos con paso de mensajes
 *
 * Se consideran N filósofos (donde N se solicita al usuario), representados
 * cada uno por un hilo, que alternan entre períodos en los que comen y piensan.
 * Para comer, deben adquirir un tenedor izquierdo y un tenedor derecho, cada
 * uno de los cuales comparten con el vecino de ese mismo lado. Un tenedor no
 * puede estar en posesión de más de un filósofo a la vez, y cada filósofo
 * deja sus tenedores cuando termina de comer.
 * 
 * La región crítica de este programa son los puntos de intercambio tenedores,
 * esto es, donde se dejan o toman de la mesa y donde se ceden a algún vecino.
 * 
 * 
 * 
 * En esta versión se implementa una solución emplando paso de mensajes 
 * aprovechando el carácter bloqueante asociado a la lectura de las colas de
 * recepción cuando no tienen mensajes.
 * 
 * Se empleará una cola global, a la que todos los filósofos tendrán acceso,
 * para representar un mutex de acceso a la región crítica. Asimismo,
 * cada filósofo tendrá una cola individual que lo dejará bloqueado
 * cuando no pueda tomar sus tenedores y a través de la cual otros hilos
 * podrán despertarlo (enviándole un mensaje y liberándolo del bloqueo
 * asociado a la lectura de la cola de recepción) para indicarle que 
 * puede continuar su ejecución.
 * 
 * Se debe compilar con la opción -pthread y -lrt.
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


#define COLOR "\033[%dm"          // String que permite cambiar el color de la consola
#define RESET "\033[0m"             // Reset del color de la consola
#define MOVER_A_COL "\r\033[64C"    // Mueve el cursor de la consola a la columna 64


int N;                   // Número de filósofos (es introducido por el usuario)

int * estado;            // Estado de cada filósofo (pensando, hambriento o comiendo)
mqd_t * colas_fil;       // Cada filósofo tiene una cola de recepción que lo dejará
        // bloqueado si no puede tomar los tenedores y lo desbloqueará cuando estos
        // quedan libres
mqd_t cola_rc;           // Esta cola representa un mutex sobre la región crítica.
        // En ella habrá, a lo sumo, un mensaje. Cuando un filósofo intente leer la
        // cola, si no hay mensajes, quedará bloqueado (mutex tomado). Si hay un 
        // mensaje, significará que la región crítica está libre y podrá avanzar.

// Todas las colas almacenarán, a lo sumo, un mensaje

size_t tam_msg;          // Tamaño de cada mensaje (emplearemos caracteres)


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
void crear_colas();
void cerrar_colas();
void destruir_colas();
void salir_con_error(char * mensaje, int ver_errno);




int main(){
    pthread_t * filosofos;         // Identificadores de los hilos filósofos
    char msg = '_';                // Mensaje que se enviará a cola_rc (contenido irrelevante)
    int i;                         // Variable de iteración

    // Solicitamos al usuario que introduzca el número de filósofos, N
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

    // Reservamos memoria para el array de hilos
    if ((filosofos = (pthread_t *) malloc(N * sizeof(pthread_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los hilos\n", 0);
    // Reservamos memoria para el array de colas de los filósofos
    if ((colas_fil = (mqd_t *) malloc(N * sizeof(mqd_t))) == NULL)
        salir_con_error("No se ha podido reservar memoria para los semaforos\n", 0);
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

    destruir_colas();       // Destruimos las colas por si ya existían de una ejecución previa
    crear_colas();          // Creamos la cola de la región crítica y las de los filósofos

    // Antes de empezar, enviamos un mensaje a la cola de la región crítica, que simboliza un mutex.
    // Así, indicamos que la región crítica está libre
    // El tamaño del mensaje es el de 1 char (1 byte). La prioridad es 0 porque este sistema no es
    // necesario (todas las colas tienen solo un slot)
    mq_send(cola_rc, &msg, tam_msg, 0);


    // Se crean los N filósofos
    for (i = 0; i < N; i++) crear_hilo(&filosofos[i], i);
    // El hilo principal espera a que todos los filósofos terminen antes de continuar
    for (i = 0; i < N; i++) unirse_a_hilo(filosofos[i]);


    cerrar_colas();         // Cerramos todas las colas
    destruir_colas();       // Y también las borramos del sistema
    
    // Liberamos la memoria reservada
    free(filosofos);
    free(colas_fil);
    free(estado);

    printf("\n\nEjecución finalizada. Cerrando programa...\n\n");

    exit(EXIT_SUCCESS);}



/*
 * Función que representa el ciclo de vida de un filósofo. Recibe como argumento el identificador que utilizará
 * a lo largo de su ejecución (su número de filósofo). La estructura general de esta función no cambia en paso de mensajes,
 * pero sí las funciones tomar_tenedores y poner_tenedores.
 */
void * filosofo(void * ptr_id){
    int id = (intptr_t) ptr_id;   // Identificador del hilo (lo pasamos a entero de forma segura con el tipo intptr_t)
                                  // id va de 0 a N-1
    int i;                        // Contador de iteraciones

    // No es necesario que los filósofos abran las colas, porque como usamos hilos, la heredan directamente del hilo padre

    // Realizamos un número finito de iteraciones para controlar el tiempo de ejecución
    for (i = 0; i < MAX_ITER; i++){
        pensar();              // El filósofo no actúa
        tomar_tenedores(id);   // El filósofo toma ambos tenedores o queda bloqueado esperando
        comer(id);             // El filósofo espera mientras sostiene ambos tenedores
        poner_tenedores(id);   // El filósofo devuelve los 2 tenedores a la mesa
    }

    // El hilo principal se encarga de cerrar y destruir las colas

    pthread_exit((void *) "Hilo finalizado correctamente");
}


/*
 * Se comprueba si el filósofo de número id quiere y puede comer. Si es así, lo hace. Si no, la función 
 * tomar_tenedores() lo obligará a esperar.
 */
void probar(int id){
    char msg = '_';             // Mensaje que se le enviará al filósofo id si puede comer, para desbloquearlo
                                // Su contenido no se utiliza nunca

    /*
     * Se comprueba que el filósofo esté hambriento (que implica que no está comiendo, en cuyo caso ya tendría los tenedores, 
     * y que quiere comer) y que ni el filósofo de su izquierda está comiendo ni el de su derecha (por lo que sus tenedores 
     * están libres).
     *
     * Es importante verificar que el estado del hilo sea HAMBRIENTO porque puede que sea alguno de sus vecinos quien esté 
     * llamando a esta función. Si no se verificara, los vecinos tendrían la capacidad de obligarle a comer cuando en realidad 
     * él aún no quiere.
     */
    if (estado[id] == HAMBRIENTO && estado[IZQUIERDO] != COMIENDO && estado[DERECHO] != COMIENDO){
        estado[id] = COMIENDO;      // El filósofo ya no deja que ninguno de sus vecinos tome sus tenedores.

        // Se le envía un mensaje al filósofo id para que no quede bloqueado en la función tomar_tenedores
        // La prioridad es 0 porque en la implementación del problema estamos usando siempre colas sin prioridades 
        mq_send(colas_fil[id], &msg, tam_msg, 0);
    }
}

/*
 * El filósofo trata de tomar los tenedores de su izquierda y de su derecha. Si no puede, se bloquea hasta que pueda.
 */
void tomar_tenedores(int id){
    char msg;           // Mensaje enviado/recibido a través de las colas

    /*
     * El filósofo trata de acceder a la región crítica llamando a mq_receive. La clave de esta implementación es que la función es
     * bloqueante, de forma que si no hay ningún mensaje en la cola de la región crítica (análogo a un mutex tomado), el hilo
     * quedará esperando hasta que alguien salga de la región crítica y envíe un mensaje a la cola.
     * Aunque haya varios hilos esperando, solo uno leerá el mensaje, asegurando que solo un hilo entra en la región crítica.
     * 
     * Aunque guardamos el mensaje en msg, no lo leeremos (lo importante es solamente que haya un mensaje). También ignoramos la
     * prioridad, pues no usamos este sistema (cuarto argumento).
     */
    mq_receive(cola_rc, &msg, tam_msg, NULL);


    log_consola(id, "Quiere tomar tenedores");
    estado[id] = HAMBRIENTO;  // Registra que quiere tomar los tenedores
    probar(id);             // Comprueba si el filósofo puede comer (él está hambriento y sus vecinos no están comiendo, es decir, 
                            // tienen libres los tenedores)

    // El filósofo sale de la región crítica. Para dejar entrar a otro, envía un mensaje a la cola correspondiente
    // La prioridad es 0 porque no la estamos usando. No modificamos el contenido de msg porque es irrelevante.
    mq_send(cola_rc, &msg, tam_msg, 0);

    // Si el filósofo puede comer, desde la función probar se le habrá enviado un mensaje a su cola individual. En caso contrario,
    // su cola estará vacía. Empleamos el carácter bloqueante de mq_receive para forzar a que el filósofo no pueda avanzar hasta
    // que le llegue un mensaje.
    // Cuando reciba tal mensaje, el estado del filósofo será "COMIENDO" y tendrá disponibles ambos tenedores.
    mq_receive(colas_fil[id], &msg, tam_msg, NULL);
}

/*
 * Tras comer, el filósofo deja sus tenedores en la mesa, dejándoselos disponibles a sus vecinos.
 */
void poner_tenedores(int id){
    char msg;           // Mensaje de las colas (su contenido no se utiliza)

    /* El filósofo trata de acceder a la región crítica. Si está libre, habrá un mensaje sin leer en la cola de la región
     * crítica. En caso contrario, alguien está dentro y el filósofo quedará esperando a que salga y envíe un mensaje a la cola.
     * Aunque haya varios hilos observando la cola, solo uno leerá el mensaje que llegue. Por tanto, solo este se desbloqueará
     * y en la región crítica no habrá varios hilos simultáneamente.
     */
    mq_receive(cola_rc, &msg, tam_msg, NULL);

    log_consola(id, "Va a dejar sus tenedores");
    estado[id] = PENSANDO;    // El filósofo está ocioso. No está comiendo ni quiere tomar tenedores.

    // Si el filósofo de la izquierda quiere comer y su tenedor izquierdo está libre, 
    // se le cede el tenedor derecho y se le permite comer (deja de esperar su turno). Esto se hará
    // enviándole un mensaje a su cola individual.
    probar(IZQUIERDO);     
    if (estado[IZQUIERDO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino izquierdo y este come");
    probar(DERECHO);        // Análogo con con el filósofo de la derecha.
    if (estado[DERECHO] == COMIENDO) log_consola(id, "Cede un tenedor al vecino derecho y este come");

    // El filósofo abandona la región crítica. "Levanta el mutex" enviando un mensaje a la cola
    // Reciclamos el contenido de msg ya que no lo leemos en ningún momento
    mq_send(cola_rc, &msg, tam_msg, 0);
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
    // Empleamos los colores rojo, verde, amarillo, azul, magenta o fucsia según el id del hilo (31-36)
    int color = 31 + id % 6;
    char * estados = ver_estados();         // Estados de los filósofos

    // Con la macro MOVER_A_COL nos colocamos en una columna a la derecha para imprimir los estados de los filósofos
    printf(COLOR "[%d]: %s" MOVER_A_COL "%s" RESET "\n", color, id, msg, estados);
    free(estados);
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
 * Función auxiliar que crea un nuevo hilo (un filósofo). Ejecutará la función "filosofo",
 * a la cual se le pasa el número que usará como identificador.
 * El identificador del hilo generado por pthread_create se devuelve por referencia a través
 * del parámetro "hilo".
 */
void crear_hilo(pthread_t * hilo, int i){
    int error;          // Comprobación de errores

    // El argumento debe enviarse como un puntero a void, y no como un entero. Para realizar una conversión segura,
    // pasamos primero el dato a intptr_t y después a un puntero a void.
    // Pasamos NULL como segundo argumento para no modificar los atributos por defecto del hilo
    if ((error = pthread_create(hilo, NULL, filosofo, (void *) (intptr_t) i)) != 0){
        fprintf(stderr, "Error %d al crear un filósofo: %s\n", error, strerror(error));
        exit(EXIT_FAILURE);
    }
}


/*
 * Función auxiliar que provoca que el hilo principal quede esperando hasta que finalice el hilo cuyo 
 * identificador se pasa como argumento.
 * Esta función se ejecuta para los N filósofos.
 */
void unirse_a_hilo(pthread_t hilo){
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


/*
 * Función auxiliar que inicializa las colas de mensajes que se usarán durante el programa:
 * las N colas de recepción de los filósofos y la cola de la región crítica.
 */
void crear_colas(){
    struct mq_attr attr;        // Atributos de las colas
    char * nombre_buz;          // Cadena con el nombre de una cola
    int i;                      // Variable de iteración

    // Los mensajes serán de un único carácter
    tam_msg = sizeof(char);

    // En todas las colas habrá a lo sumo un mensaje, pues su misión es solamente bloquear/desbloquear
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = tam_msg;      // Tamaño de cada mensaje

    // Creamos la cola de la región crítica. Se conceden todos los permisos (777) y se utiliza la configuración
    // establecida a través de attr. Se permite tanto lectura como escritura (O_RDWR).
    if ((cola_rc = mq_open("/BUZON_RC", O_CREAT | O_RDWR, 0777, &attr)) == -1)
        salir_con_error("No se ha podido crear la cola de la región crítica", 1);


    /* Creamos las colas de los filósofos, que tienen como nombre /BUZON_F1, /BUZON_F2, ..., 
     * /BUZON_FN, donde N es el número de filósofos (asumimos N < 1000).
     * Construimos una cadena para cada nombre de buzón.
     */
    nombre_buz = (char *) malloc(11 * sizeof(char));
    snprintf(nombre_buz, 9, "/BUZON_F");
    for (i = 0; i < N; i++){
        // Añadimos a nombre_buz el identificador del filósofo (a los sumo 3 caracteres 
        // terminados en el carácter nulo)
        snprintf(&nombre_buz[8], 4, "%d", i);       

        // Creamos la cola
        if ((colas_fil[i] = mq_open(nombre_buz, O_CREAT | O_RDWR, 0777, &attr)) == -1)
            salir_con_error("No se ha podido crear la cola de un filósofo", 1);
    }

    // Liberamos la cadena
    free(nombre_buz);
}

/*
 * Función auxiliar que cierra las colas de mensajes que se utilizan a lo largo del programa:
 * las N colas de recepción de los filósofos y la cola de la región crítica.
 */
void cerrar_colas(){
    int i;                      // Variable de iteración
    
    // Cerramos la cola de recepción de la región crítica
    if (mq_close(cola_rc)) salir_con_error("Error al cerrar la cola de la región crítica", 1);

    // Cerramos las colas individuales de los filósofos
    for (i = 0; i < N; i++){
        if (mq_close(colas_fil[i])) 
            salir_con_error("Error al cerrar la cola de la región crítica", 1);
    }
}

/*
 * Función auxiliar que destruye las colas de mensajes que se utilizan a lo largo del programa:
 * las N colas de recepción de los filósofos y la cola de la región crítica.
 */
void destruir_colas(){
    char * nombre_buz;          // Cadena con el nombre de una cola
    int i;                      // Variable de iteración

    // Destruimos la cola de la región crítica.
    mq_unlink("/BUZON_RC");

    /* Destruimos las colas de los filósofos, que tienen como nombre /BUZON_F1, /BUZON_F2, ..., 
     * /BUZON_FN, donde N es el número de filósofos (asumimos N < 1000).
     * Construimos una cadena para cada nombre de buzón.
     */
    nombre_buz = (char *) malloc(11 * sizeof(char));
    snprintf(nombre_buz, 9, "/BUZON_F");
    for (i = 0; i < N; i++){
        // Añadimos a nombre_buz el identificador del filósofo (a los sumo 3 caracteres 
        // terminados en el carácter nulo)
        snprintf(&nombre_buz[8], 4, "%d", i);       
        mq_unlink(nombre_buz);       // Destruimos el buzón del sistema
    }

    // Liberamos la cadena
    free(nombre_buz);
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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>



/*
 * Xiana Carrera Alonso
 * Sistemas Operativos II
 * Práctica 2 - Ejercicio 1
 *
 * Este programa implementa el problema del productor-consumidor usando procesos y espera activa.
 * Se define un buffer compartido por productor y consumidor. El productor introduce items en él, mientras que el
 * consumidor elimina items (libera posiciones del buffer).
 * Al no dar ninguna solución para el problema, pueden aparecer carreras críticas. Estas son ocasionadas por cuenta,
 * una varible (compartida entre los procesos) que registrará en todo momento el número de elementos presentes en el
 * buffer.
 * La velocidad de los procesos productor y consumidor es controlable a través del argumento "ralentizar" en las
 * funciones producir y consumir, respectivamente. El buffer empleado es una pila LIFO.
 * En este ejercicio el consumidor se crea antes que el productor.
 *
 * Debe compilarse con la opción -pthread.
 */


#define N 8                         // Tamaño del buffer compartido entre productor y consumidor
#define N_ITER 100                  // Número de iteraciones de cada proceso

#define VERDE "\033[32m"            // Color en el que imprimirá el productor
#define AZUL "\033[34m"             // Color en el que imprimirá el consumidor
#define RESET "\033[39m"            // Reseteado de color
#define CURSIVA "\033[3m"           // Letra en cursiva
#define RESET_CURS "\033[23m"       // Reseteado de cursiva


// Función del productor
void producir(int ralentizar);
// Función del consumidor
void consumir(int ralentizar);

// Función de impresión del contenido del buffer
void log_buffer(int proceso);

// Función de cierre del área de memoria compartida
void cerrar_mem_compartida();

// Función auxiliar de finalización con error
void cerrar_con_error(char * mensaje, int ver_errno);



int * cuenta = NULL;            // Número de items guardados en el buffer
char * buffer = NULL;           // Buffer gestionado por productor y consumidor


int main(int argc, char * argv[]){
    void * area_compartida = NULL;      // Puntero al área de memoria compartida entre procesos
    pid_t exit_wait;                    // Valor de retorno de waitpid
    int status;                         // Estado de waitpid
    pid_t error_fork;       // Variable que recoge la salida de los fork para corroborar la aparición de errores

    /*
     * Reservamos un área de memoria compartida y anónima (sin archivo de respaldo) a través de mmap. Cuando creemos
     * los procesos hijos (productor y consumidor), hererdarán dicha región.
     *
     * Esta región de memoria estará dividida en 2: en la posición inicial guardará el entero cuenta. El resto de la
     * región se destinará a almacenar el buffer (N chars).
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
    if ((area_compartida = mmap(NULL, (size_t) sizeof(int) + N * sizeof(char), PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, (off_t) 0)) == MAP_FAILED)
        // Si hay algún error, finalizamos la ejecución e imprimimos errno con un mensaje personalizado
        cerrar_con_error("Error: no se ha podido reservar un área de memoria compartida", 1);

    // Almacenamos referencias a la variable cuenta y al buffer de forma global (pues al fin y al cabo, son
    // compartidas por todos los procesos de forma "descontrolada")
    cuenta = (int *) area_compartida;
    buffer = (char *) (area_compartida + sizeof(int));          // El buffer comienza sizeof(int) posiciones después

    // El buffer estará inicialmente vacío, de forma que la cuenta será 0
    *cuenta = 0;
    // También guardamos en cada posición del buffer un carácter que representa "posición vacía". Se ha elegido
    // el carácter '_'
    memset(buffer, '_', (size_t) N * sizeof(char));

    printf("Se procede a iniciar los programas productor y consumidor. Se utilizará el código de colores:\n");
    printf("%s\tPRODUCTOR%s\n", VERDE, RESET);
    printf("%s\tCONSUMIDOR%s\n\n\n", AZUL, RESET);

    /*
     * Al crear el consumidor antes que el productor, aseguramos que ambos procesos empiecen aproximadamente a la vez.
     * Si comenzara el productor, este podría insertar un número elevado de elementos en el buffer antes de que el
     * consumidor llegara a estar disponible para eliminarlos. Si empieza el consumidor, quedará bloqueado hasta que
     * el productor empiece a actuar (pues no puede consumir nada mientras el buffer esté vacío)
     */

    if ((error_fork = fork()) == 0)         // Se crea el proceso consumidor
        consumir(0);
        // Pasamos un argumento indicando si no se debe ralentizar el proceso (0) o sí (!0, en cuyo caso el valor
        // será igual al número de segundos de parálisis)
    else if (error_fork == -1){         // Error en el fork
        // Cerramos la memoria compartida y salimos con un mensaje de error imprimido con fprintf
        cerrar_mem_compartida();
        cerrar_con_error("Error al crear el proceso hijo consumidor", 0);
    }

    if ((error_fork = fork()) == 0)         // Se crea el proceso productor
        producir(0);
        // Pasamos un argumento indicando si no se debe ralentizar el proceso (0) o sí (!0, en cuyo caso el valor
        // será igual al número de segundos de parálisis)
    else if (error_fork == -1){      // Error en el fork
        // Cerramos la memoria compartida y salimos con un mensaje de error imprimido con fprintf
        cerrar_mem_compartida();
        cerrar_con_error("Error al crear el proceso hijo productor", 0);
    }


    // Una vez creados los procesos productor y consumidor, el proceso padre no necesitará volver a usar la región
    // compartida de memoria. Se cierra ya para mejorar la eficiencia.
    cerrar_mem_compartida();


    // El proceso queda atrapado en un bucle hasta que todos sus hijos hayan finalizado (waitpid devolverá -1)
    while((exit_wait = waitpid(-1, &status, 0)) != -1){
        /*
        * Si WIFEXITED(status) es 0, el hijo no terminó con un exit() o volviendo del main. Sería el caso de
        * finalización por una señal.
        * Si WIFEXITED(status) es 1, podemos comprobar WEXITSTATUS para determinar cuál fue el valor del exit del hijo.
        *
        * Si hay error, cortamos la ejecución tras imprimir un mensaje descriptivo.
        */
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
            cerrar_con_error("Finalización incorrecta de un proceso hijo", 0);
    }

    // El proceso principal será el último en finalizar.
    printf("\n\n\n\nFinalizando ejecucion del problema del productor-consumidor...\n");
    exit(EXIT_SUCCESS);
}



/*
 * Función ejecutada por el productor, que introduce nuevos elementos en el buffer (una pila).
 * Puede dar lugar a carreras críticas debido a la variable cuenta: podrían insertarse items en posiciones
 * incorrectas del buffer.
 * Los mensajes del productor aparecen en color verde y en el lado izquierdo de la terminal.
 * @param ralentizar: Variable que activa (si es !0) un sleep de "ralentizar" segundos, con el objetivo de permitir
 *                    que las carreras críticas se localicen más fácilmente aumentando el tiempo que pasa el proceso
 *                    en la región crítica.
 */
void producir(int ralentizar){
    char item = 96;         // Carácter anterior a la primera letra del alfabeto (que sería la 97, la 'a')
    int i=0;                // Variable de iteración

    // Las letras del alfabeto en minúsculas ocupan las posiciones [97,122] (26 posiciones)

    while (i < N_ITER){         // Máximo de N_ITER iteraciones (no utilizamos bucles infinitos)
        // El productor comienza cada iteración indicando el valor actual de la variable cuenta
        printf("%s**INICIO** iteracion %d, cuenta = %d\t\t\t\t\t\t\t", VERDE, i, *cuenta);
        log_buffer(1);

        item = (item - 96) % 26 + 97;       // Generamos un nuevo elemento
        /*
         * Los elementos estarán en el intervalo de enteros [97, 122], que en ASCII se corresponden con las 26
         * letras del alfabeto inglés en minúsculas
         * En primer lugar, se realiza item - 96 = item - 97 + 1, pasando al intervalo [0, 25] y después, incrementando
         * item en una unidad (se pasa a [1, 26]). Entonces, se hace el módulo por 26 para que los items no se pasen de
         * los límites del abecedario. Por último, se suma 97 para volver al intervalo [97, 122].
         */
        while (*cuenta == N) ;              // Bucle de espera activa. Si el buffer está lleno, el productor no puede
                // actuar y queda esperando a que el consumidor libere alguna posición

        ////////// INICIO DE LA REGIÓN CRÍTICA

        /*
         * La región crítica está delimitada por las líneas buffer[*cuenta] = item y (*cuenta)++, que son las que
         * provocarán problemas: interrupciones en la actualización de cuenta por parte del consumidor provocarán que
         * los elementos insertados por el productor en la línea buffer[*cuenta] = item no se introduzcan en la
         * posición correcta. Aparecerán entonces errores de coherencia y el buffer quedará en un estado incorrecto.
         */

        buffer[*cuenta] = item;             // Guardamos el nuevo item en la posición indicada por cuenta

        // Imprimimos un mensaje que indica el estado en el que queda el buffer y la variable cuenta, así como
        // el contenido del primero
        printf("%sPosición %d -> item %c%s\t\t\t\t\t\t\t\t\t", VERDE, *cuenta, item, RESET);
        log_buffer(1);

        // Si el programador así lo indica, se ralentiza el productor en el punto realmente crítico del código, esto
        // es, entre la modificacíon del buffer y su registro a través de la actualización de la variable cuenta.
        if (ralentizar) sleep(ralentizar);

        // La segunda línea clave en el código es la actualización de cuenta, que refleja que hay un elemento más
        // en el buffer.
        (*cuenta)++;

        ////////// FIN DE LA REGIÓN CRÍTICA

        // Imprimimos el estado en el que quedan la variable cuenta y el buffer tras finalizar la iteración
        printf("%s//FIN// iteracion %d; cuenta = %d  %s\t\t\t\t\t\t\t", VERDE, i, *cuenta, RESET);
        log_buffer(1);
        i++;            // Pasamos a la siguiente iteración
    }

    // Una vez el proceso productor termina su trabajo, se cierra el área de región compartida para él empleando
    // munmap
    cerrar_mem_compartida();

    printf("\n%sFinalizando productor...%s\n", VERDE, RESET);
    exit(EXIT_SUCCESS);
}

/*
 * Función ejecutada por el consumidor, que elimina (sobreescribe con un guion bajo '_') elementos preexistentes en el
 * buffer. Lo utiliza como una estructura de pila LIFO.
 * Puede dar lugar a carreras críticas debido a la variable cuenta: podrían eliminarse items de posiciones incorrectas.
 * Los mensajes del consumidor aparecen en color azul y en el lado derecho de la terminal.
 * @param ralentizar: Variable que activa (si es !0) un sleep de "ralentizar" segundos, con el objetivo de permitir
 *                    que las carreras críticas se localicen más fácilmente aumentando el tiempo que pasa el proceso
 *                    en la región crítica.
 */
void consumir(int ralentizar){
    int i = 0;              // Contador de iteraciones

    while (i < N_ITER){         // Máximo de N_ITER iteraciones (no utilizamos bucles infinitos)
        // El consumidor comienza cada iteración imprimiendo el valor de la variable cuenta y el contenido del buffer,
        // con el objetivo de poder corroborar la ocurrencia de carreras críticas
        printf("\t\t\t\t\t%s**INICIO** iteracion %d, cuenta = %d\t\t", AZUL, i, *cuenta);
        log_buffer(0);

        while (*cuenta == 0) ;     // Bucle de espera activa
        // El consumidor únicamente podrá entrar en la región crítica y eliminar un item si hay elementos guardados
        // en el buffer. En caso contrario, quedará ejecutando el bucle hasta que se verifique dicha condición.

        ////////// INICIO DE LA REGIÓN CRÍTICA

        /*
         * La región crítica está delimitada por las líneas *(buffer + *cuenta - 1) = '_' y (*cuenta)++, que son las
         * que provocan problemas: interrupciones en la actualización de cuenta por parte del productor provocarán que
         * el consumidor no escriba el carácter '_' (que representa la eliminación de un elemento) en la posición
         * correcta. Por consiguiente, el buffer quedará en un estado incoherente e incorrecto.
         */

        // Para las posiciones del buffer estamos empleando el rango [0, N-1], de forma que la posición a eliminar
        // vendrá dada por *cuenta - 1.
        *(buffer + *cuenta - 1) = '_';

        // Imprimimos un mensaje de información acerca del estado actual del buffer y del elemento eliminado
        printf("\t\t\t\t\t%sPosición %d consumida\t\t\t\t", AZUL, *cuenta - 1);
        log_buffer(0);

        // Si el programador así lo indica, se ralentiza el productor en el punto realmente crítico del código, esto
        // es, entre la modificacíon del buffer y su registro a través de la actualización de la variable cuenta.
        if (ralentizar) sleep(ralentizar);

        // Decrementamos la cuenta para indicar que hay un elemento menos en el buffer
        (*cuenta)--;

        ////////// FIN DE LA REGIÓN CRÍTICA

        // Imprimimos el estado en el que quedan la variable cuenta y el buffer tras finalizar la iteración
        printf("\t\t\t\t\t%s//FIN// iteracion %d; cuenta = %d  %s\t\t", AZUL, i, *cuenta, RESET);
        log_buffer(0);
        i++;            // Incrementamos el contador de iteraciones
    }

    // Una vez el proceso productor termina su trabajo, se cierra el área de región compartida para él empleando
    // munmap
    cerrar_mem_compartida();

    printf("\n\t\t\t\t\t%sFinalizando consumidor...%s\n", AZUL, RESET);
    exit(EXIT_SUCCESS);
}

/*
 * Función que imprime los contenidos del buffer en una línea. Es empleada por el productor, que imprime en verde,
 * y por el consumidor, que imprime en azul. Toda la cadena aparece en cursiva.
 * @param proceso: Valdrá !0 si se trata del proceso productor, y 0 si es el consumidor.
 */
void log_buffer(int proceso){
    int i;              // Variable de iteración

    // Dependiendo del argumento proceso, se imprime en verde o en azul. Siempre se activa la cursiva.
    printf("%s%sbuffer = [", proceso? VERDE : AZUL, CURSIVA);
    for (i = 0; i < N - 1; i++)
        printf("%c ", *(buffer + i));           // Leemos uno a uno los caracteres contenidos en el buffer
    printf("%c]%s%s\n", *(buffer + i), RESET, RESET_CURS);
    // No utilizamos \n hasta el final para tratar de que el buffer se imprima de forma atómica
}

/*
 * Función que cierra la región de memoria compartida para el proceso que la llama. Se asume que esta comienza en la
 * posición a la que apunta cuenta y que ocupa un entero y N chars.
 */
void cerrar_mem_compartida(){
    // Utilizamos munmap, indicando el puntero a la región y el tamaño de esta (un entero y N caracteres)
    if (munmap((void *) cuenta, (size_t) sizeof(int) + N * sizeof(char)) == -1)
        cerrar_con_error("Error: no se ha podido cerrar la proyección del área compartida entre los procesos", 1);
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

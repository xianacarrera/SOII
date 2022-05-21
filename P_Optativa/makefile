# Opciones de compilación, mostrando todos los warnings (-Wall)
CC = gcc -Wall
# Opción de compilación -pthread
INCLUDE_PTHREAD = -pthread

# Ficheros fuente para los 4 ejercicios
SRCS_1 = filosofos1.c
SRCS_2 = filosofos2.c
SRCS_3 = filosofos3.c
SRCS_4 = filosofos4.c

# Nombre del ejecutable de cada ejercicio: nombre del fichero fuente sin extensión
OUTPUT_1 = $(SRCS_1:.c=)
OUTPUT_2 = $(SRCS_2:.c=)
OUTPUT_3 = $(SRCS_3:.c=)
OUTPUT_4 = $(SRCS_4:.c=)

# Archivos objeto (.o con un .c análogo como fichero fuente)
OBJS_1 = $(SRCS_1:.c=.o)
OBJS_2 = $(SRCS_2:.c=.o)
OBJS_3 = $(SRCS_3:.c=.o)
OBJS_4 = $(SRCS_4:.c=.o)


# Regla 1
# Creamos el ejecutable de cada programa y limpiamos el directorio de objetos
all: $(OUTPUT_1) $(OUTPUT_2) $(OUTPUT_3) $(OUTPUT_4) clean

# Regla 2
# Creamos el ejecutable de filosofos1.c
# $@ es el nombre del archivo que se está generando, $< es el primer prerrequisito
$(OUTPUT_1): $(OBJS_1) 
	$(CC) -o $@ $< $(INCLUDE_PTHREAD)
	
# Regla 3
# Creamos el ejecutable de filosofos2.c
$(OUTPUT_2): $(OBJS_2) 
	$(CC) -o $@ $< $(INCLUDE_PTHREAD)

# Regla 3
# Creamos el ejecutable de filosofos3.c
$(OUTPUT_3): $(OBJS_3) 
	$(CC) -o $@ $< $(INCLUDE_PTHREAD)

# Regla 4
# Creamos el ejecutable de filosofos4.c
$(OUTPUT_4): $(OBJS_4) 
	$(CC) -o $@ $< $(INCLUDE_PTHREAD)

# Regla 5
# Borra los ejecutables y ejecuta clean dentro del directorio actual
cleanall: clean 
	rm -f $(OUTPUT_1) $(OUTPUT_2) $(OUTPUT_3) $(OUTPUT_4)

# Regla 6
# Borra todos los archivos .o utilizando el wildcard * (match con cualquier carácter)
# dentro del directorio actual
clean: 
	rm -f *.o

#include <stdlib.h>
#include <stdio.h>

#define N
#define IZQUIERDO (i+N-1)%N
#define PENSANDO 0
#define HAMBRIENTO 1
#define COMIENDO 2

int estado[N];
sem_t mutex;
sem_t s[n];

void pensar(){
   sleep(1);
   // Inventado
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




int main(){
    int i;


    
    return 0;
}
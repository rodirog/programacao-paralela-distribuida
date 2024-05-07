#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <omp.h>

#define NUM_SENSORES 4

sem_t semEmpty;
sem_t semFull;

pthread_mutex_t mutexBuffer;

int dados_sensoriais[100];
int count = 0;

void* producer(void* args) {
    while (1) {
        // Produce
        int x = rand() % 1000;
        sleep((rand() % 5) + 1);

        // Add to the buffer
        sem_wait(&semEmpty);
        pthread_mutex_lock(&mutexBuffer);
        dados_sensoriais[count] = x;
        count++;
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);
    }
}

void* consumer(void* args) {
    while (1) {
        int y;

        // Remove from the buffer
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        y = dados_sensoriais[count - 1];
        count--;
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semEmpty);

        // Consume
        printf("Got %d\n", y);
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    pthread_t th[NUM_SENSORES];
    pthread_t th_consumer;
    pthread_mutex_init(&mutexBuffer, NULL);
    sem_init(&semEmpty, 0, 100);
    sem_init(&semFull, 0, 0);

    int i;
    for (i = 0; i < NUM_SENSORES; i++) {
        
        if (pthread_create(&th[i], NULL, &producer, NULL) != 0) {
            perror("Failed to create thread");
        }
    }

    if (pthread_create(&th_consumer, NULL, &consumer, NULL) != 0) {
            perror("Failed to create consumer thread");
        }

    for (i = 0; i < NUM_SENSORES; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }

    if (pthread_join(th_consumer, NULL) != 0) {
        perror("Failed to join consumer thread");
    }

    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}
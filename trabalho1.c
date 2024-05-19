#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <omp.h>

#define NUM_SENSORES 5
#define NUM_ATUADORES 5
#define NUM_UNIDADES_PROCES 3

typedef struct Task {
    int a, b;
} Task;

sem_t semEmpty;
sem_t semFull;

pthread_mutex_t mutexBuffer;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
pthread_t mutexAtuadores;

int dados_sensoriais[100];
int count = 0;

Task taskQueue[100];
int taskCount = 0;

// Tabela de atuadores
int atuadores[NUM_ATUADORES];


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

void executeTask(Task* task, pthread_t thread_id) {
    usleep(50000);
    // printf("The sum of %d and %d is %d\n", task->a, task->b);

    // Modificar o valor do atuador e atualizar o array atuadores
    pthread_mutex_lock(&mutexAtuadores); // Travar o mutex antes de modificar o array
    atuadores[task->a] = task->b; // Atualizar o valor do atuador no array
    pthread_mutex_unlock(&mutexAtuadores); // Destravar o mutex após modificar o array

    printf("Thread ID: %lu\n Atuador: %d nivel: %d\n",thread_id, task->a, task->b);
}

void submitTask(Task task) {
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = task;
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}

void* startThread(void* args) {
    while (1) {
        pthread_t self = pthread_self();
        Task task;

        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0) {
            pthread_cond_wait(&condQueue, &mutexQueue);
        }

        task = taskQueue[0];
        int i;
        for (i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task, self);
    }
}

void* consumer(void* args) {
    pthread_t th[NUM_UNIDADES_PROCES];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    srand(time(NULL));

    int i;
    // Create task execution threads
    for (i = 0; i < NUM_UNIDADES_PROCES; i++) {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0) {
            perror("Failed to create the thread");
        }
    }
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

        // srand(time(NULL));
        // Definir o atuador e o nível de atividade
        int atuador = y % NUM_ATUADORES;
        int nivel_atividade = rand() % 101; // valor aleatório entre 0 e 100
        
        Task t = {
            .a = atuador,
            .b = nivel_atividade
        };
        submitTask(t);

        }

    for (i = 0; i < NUM_UNIDADES_PROCES; i++) {
            if (pthread_join(th[i], NULL) != 0) {
                perror("Failed to join the thread");
            }
        }
        
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    pthread_t th[NUM_SENSORES];
    pthread_t th_consumer;
    pthread_t mutexAtuadores;
    pthread_mutex_init(&mutexBuffer, NULL);
    pthread_mutex_init(&mutexAtuadores, NULL);
    sem_init(&semEmpty, 0, 100);
    sem_init(&semFull, 0, 0);

    int i;

        // Inicialização dos atuadores
    for (i = 0; i < NUM_ATUADORES; i++) {
        atuadores[i] = 0; // Inicializando todos os atuadores com atividade 0
    }

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
    pthread_mutex_destroy(&mutexAtuadores);

    return 0;
}

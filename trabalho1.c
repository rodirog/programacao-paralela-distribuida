#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

#define NUM_SENSORES 4
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
pthread_mutex_t mutexAtuadores;
pthread_mutex_t mutexPrint;

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

    int atuador = task->a;
    int nivel = task->b;

    pid_t pid = fork();

    if (pid < 0) {
        // Erro ao criar o fork
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // Processo filho: muda o nível do atuador
        srand(time(NULL) ^ (getpid() << 16));
        nivel = rand() % 101;
        srand(time(NULL));
        int waitTime = (rand() % 1000001) + 2000000;  // Entre 2000000 e 3000000 microsegundos (2s a 3s)
        pthread_mutex_lock(&mutexAtuadores);
        atuadores[atuador] = nivel;
        
        // Gera um tempo de espera aleatório entre 2 e 3 segundos
        
        usleep(waitTime);
        pthread_mutex_unlock(&mutexAtuadores);
        // 20% de chance de falha
        int fail = rand() % 5 == 0;

        // Retorna 1 em caso de falha, 0 em caso de sucesso
        _exit(fail ? 1 : 0);
    } else {
        // Processo pai: envia a mudança ao painel

        // Espera o processo filho terminar
        int status;
        waitpid(pid, &status, 0);

        pthread_mutex_lock(&mutexPrint); 
        printf("Alterando: %d com valor %d\n", atuador, nivel);

        // Espera 1 segundo antes de prosseguir
        usleep(1000000);
        pthread_mutex_unlock(&mutexPrint);

        // 20% de chance de falha
        srand(time(NULL) ^ (getpid() << 16));
        int fail = rand() % 5 == 0;

        

        int child_failed = WIFEXITED(status) && WEXITSTATUS(status) == 1;


        if (fail || child_failed) {
            printf("Falha: %d\n", atuador);
        }
    }
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
        int dado;

        // Remove from the buffer
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        dado = dados_sensoriais[count - 1];
        count--;
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semEmpty);

        //Consume
        printf("Got %d\n", dado);

        // srand(time(NULL));
        // Definir o atuador e o nível de atividade
        int atuador = dado % NUM_ATUADORES;
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
    pthread_mutex_init(&mutexPrint, NULL);
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
    pthread_mutex_destroy(&mutexPrint);

    return 0;
}

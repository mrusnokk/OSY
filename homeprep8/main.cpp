#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

constexpr int N = 100;

sem_t mutex_sem;
sem_t empty_sem;
sem_t full_sem;

int buffer[N];
int in = 0;
int out = 0;

void *producer(void *arg)
{
    for (int i = 0; i < 20; i++)
    {
        int item = i;

        sem_wait(&empty_sem);
        sem_wait(&mutex_sem);

        buffer[in] = item;
        in = (in + 1) % N;
        printf("[Producent] Vyrobil a vlozil: %d\n", item);

        sem_post(&mutex_sem);
        sem_post(&full_sem);

        usleep(100000);
    }
    return nullptr;
}

void *consumer(void *arg)
{
    for (int i = 0; i < 20; ++i)
    {
        sem_wait(&full_sem);
        sem_wait(&mutex_sem);

        int item = buffer[out];
        out = (out + 1) % N;

        sem_post(&mutex_sem);
        sem_post(&empty_sem);

        printf("[Konzument] Vybral a zpracoval: %d\n", item);
        usleep(150000);
    }
    return nullptr;
}

int main()
{
    sem_init(&mutex_sem, 0, 1);
    sem_init(&empty_sem, 0, N);
    sem_init(&full_sem, 0, 0);

    pthread_t prod_thread, cons_thread;

    pthread_create(&prod_thread, nullptr, producer, nullptr);
    pthread_create(&cons_thread, nullptr, consumer, nullptr);

    pthread_join(prod_thread, nullptr);
    pthread_join(cons_thread, nullptr);

    sem_destroy(&mutex_sem);
    sem_destroy(&empty_sem);
    sem_destroy(&full_sem);

    return 0;
}
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_COUNT  100
#define MAX_THREADS 2

int buf[MAX_COUNT];
int count = 0;

typedef struct id_t{
        int id;
} ids;


pthread_mutex_t lock;
pthread_cond_t cond;


void producer(void* arg){

    ids* args = (ids*)arg;
    int id = args->id;
    while(1){
        pthread_mutex_lock(&lock);
        if (count == MAX_COUNT){
            printf("Buffer full, dropping producer %d\n", id);
        }else{
            buf[count] = 100;
            count ++;
            printf("Producer %d updated. count = %d\n", id, count);

        }
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
        usleep(100);

    }

}

void consumer(void* arg){

    ids* args = (ids*)arg;
    int id = args->id;
    while(1){
        pthread_mutex_lock(&lock);
        while (count == 0){
            printf("consumer %d waiting ...\n", id);
            pthread_cond_wait(&cond, &lock);
        }
        buf[count] = 0;
        count --;
        printf("consumer %d removed. count = %d\n", id, count);
        pthread_mutex_unlock(&lock);
    }

}


int main(){

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_t producer_thread[MAX_THREADS];
    pthread_t consumer_thread[MAX_THREADS];

    ids th_id[MAX_THREADS];

    int i =0;
    for (i=0;i<MAX_THREADS;i++){

        th_id[i].id = i;
        pthread_create(&producer_thread[i], NULL, producer, &th_id[i]);
        pthread_create(&consumer_thread[i], NULL, consumer, &th_id[i]);
    }

    for (i=0;i<MAX_THREADS;i++){
    pthread_join(producer_thread[i], NULL);
    pthread_join(consumer_thread[i], NULL);
    }
    return 0;
}

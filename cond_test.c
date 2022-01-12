//To run the code: ./cond_test 3
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t mylock;
pthread_cond_t cond_var;
int data = -1;

void* printer(void* arg) {
    int* repeat = (int*)arg;
    for (int i = 1; i <= *repeat; i++) {
        pthread_mutex_lock(&mylock);
        printf("Acquired Lock in printer Thread\n");
        while (data == -1) {
            pthread_cond_wait(&cond_var, &mylock);
            printf("Received Signal on printer Thread\n");
        }
        printf("Data for iteration %d: %d\n", i, data);
        data = -1;
        pthread_mutex_unlock(&mylock);
        printf("Released Lock in printer Thread\n");
    }
    pthread_exit(0);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <repeat>\n", argv[0]);
        exit(-1);
    }
    pthread_t tid;
    int repeat = atoi(argv[1]);
    pthread_cond_init(&cond_var, NULL);
    pthread_create(&tid, NULL, printer, &repeat);
    sleep(1);  // Let printer Thread go ahead and get blocked
    for (int i = repeat; i > 0; i--) {
        pthread_mutex_lock(&mylock);
        printf("Acquired Lock in Main Thread\n");
        data = i;
        sleep(1);  // Just to demonstrate the point, delaying signal
        pthread_cond_signal(&cond_var);
        printf("Sending Signal from Main Thread\n");
        //pthread_cond_broadcast: signals cond_wait_on all threads
        pthread_mutex_unlock(&mylock);
        printf("Released lock in Main Thread\n");
        sleep(1);  // Let printer Thread go ahead and get blocked
    }

    //Wait till thread completes the work
    pthread_join(tid, NULL);
    pthread_cond_destroy(&cond_var);
    pthread_mutex_destroy(&mylock);
}
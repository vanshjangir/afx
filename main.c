#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h> 

pthread_t monitor_t;
pthread_t executor_t;

void* monitor(void* arg){
    int rc;
    rc = pthread_kill(executor_t, SIGURG);
    if(rc != 0){
        printf("pthread_kill error");
        exit(1);
    }
    return NULL;
}

void handle_sigurg(int signum){
    if(signum == SIGURG){
        printf("Received sigurg");
        exit(0);
    }
}

void* executor(){
    int rc;
    struct sigaction sa;
    sa.sa_handler = handle_sigurg;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    rc = sigaction(SIGURG, &sa, NULL);
    if(rc != 0){
        printf("sigaction error in executor");
        exit(1);
    }

    while(1){
        printf("Executing...\n");
        sleep(1);
    }

    return NULL;
}

int cr_init(){
    int rc;
    
    rc = pthread_create(&executor_t, NULL, executor, NULL);
    if(rc != 0){
        printf("Error creating executor_t thread");
        return rc;
    }
    
    rc = pthread_create(&monitor_t, NULL, monitor, (void*)&executor_t);
    if(rc != 0){
        printf("Error creating monitor thread");
        return rc;
    }
    
    return 0;
}

int main(){
    int rc;
    rc = cr_init();
    if(rc != 0){
        printf("Error initializing croutines");
        exit(-1);
    }
    sleep(100);
    return 0;
}

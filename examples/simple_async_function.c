#include "../afx.h"
#include <stdio.h>

async(
    void, print1, (), {
        int i = 0;
        while(i++ < 4){
            printf("FIRST IS running\n");
            
            // usleep is being used to demonstrate a heavy task
            // but using anything that makes the thread sleep
            // can cause undefined behaviour
            usleep(1000*1000);
        }
    }
)

async(
    void, print2, (), {
        int i = 0;
        while(i++ < 4){
            printf("SECOND IS running\n");
            usleep(1000*1000);
        }
    }
)

int main(){

    int rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }
    
    afx(print1());
    afx(print2());
    sleep(1);
    return 0;
}

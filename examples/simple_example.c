#include <stdio.h>
#include "../afx.h"

#define SOME_VERY_LARGE_VALUE 1000000000

async_dec(void, print1(long long int))
async_dec(void, print2(long long int))

async(
    void, print1, (long long x), {
        while(1){
            x = 0;
            while(x++ < SOME_VERY_LARGE_VALUE){
                // some cpu intensive task
            }
            printf("First is running\n");
        }
    }
)

async(
    void, print2, (long long x), {
        while(1){
            x = 0;
            while(x++ < SOME_VERY_LARGE_VALUE/2){
                // some cpu intensive task
            }
            printf("Second is running\n");
        }
    }
)

int main(){

    int rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }

    afx(print1(1));
    afx(print2(1));
    sleep(20);
    return 0;
}

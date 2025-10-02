#include <stdio.h>
#include "../afx.h"

async_dec(void, one_second(unsigned int s));

async(
    void, one_second, (unsigned int s), {
        while(1){
            afx_sleep(s);
            printf("after one second\n");
        }
    }
)

int main(){
    int rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }

    afx(one_second(1));
    
    sleep(10);
    return 0;
}

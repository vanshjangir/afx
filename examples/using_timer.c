#include <stdio.h>
#include "../afx.h"

async_dec(void, one_second(unsigned int s));

async(
    void, one_second, (unsigned int s), {
        while(1){
            afx_sleep(s);
            printf("after %d second\n", s);
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

    // passing stack_size to override the default value
    afx(one_second(2), 512);
    
    sleep(10);
    return 0;
}

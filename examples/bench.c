#include "../afx.h"
#include <time.h>
#include <stdio.h>

double timespec_diff_us(struct timespec *start, struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    return (double)seconds * 1000000.0 + (double)nanoseconds / 1000.0;
}

async(
    void, func, (
        int a, int b, int c, int d, int e, int f,
        int g, int h, int i, int j, int k, int l
    ),{
        int x = 0;
        while(1){
            x += a + b + c + d + e + f + g + h + i + j + k + l;
        }
    }
)

void test_func(int num_funcs){
    double elapsed_us;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for(int i = 0; i < num_funcs; i++){
        afx(func(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9, i+10, i+11));
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed_us = timespec_diff_us(&start, &end);
    
    printf("Time to spawn %d async functions with 12 args: ", num_funcs);
    printf("%f microseconds\n", elapsed_us);
    printf("Average for one = %f microseconds\n", elapsed_us/(num_funcs));
}

int main(){

    int rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }
    test_func(1000000);
    return 0;
}

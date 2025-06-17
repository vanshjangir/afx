#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h> 

#define STACK_SIZE  (1024*64)

#define REG_R8      0
#define REG_R9      1
#define REG_R10     2
#define REG_R11     3
#define REG_R12     4
#define REG_R13     5
#define REG_R14     6
#define REG_R15     7
#define REG_RDI     8
#define REG_RSI     9
#define REG_RBP     10
#define REG_RBX     11
#define REG_RDX     12
#define REG_RAX     13
#define REG_RCX     14
#define REG_RSP     15
#define REG_RIP     16
#define REG_EFL     17

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rax;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t efl;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} cpu_context;

typedef struct {
    cpu_context cpu;
} afx_context;

typedef struct {
    int len;
    int cap;
    afx_context* ptr;
} afx_vector;

pthread_t monitor_t;
pthread_t executor_t;
uint64_t executor_rip;
uint64_t executor_rsp;
uint64_t stack_limit = STACK_SIZE;
int flag = 1;

afx_vector afxv;
int cur_f_index = -1;

void save_context(afx_context* cr_ctx, ucontext_t *cur_ctx){
    cr_ctx->cpu.r8 = cur_ctx->uc_mcontext.gregs[REG_R8];
    cr_ctx->cpu.r9 = cur_ctx->uc_mcontext.gregs[REG_R9];
    cr_ctx->cpu.r10 = cur_ctx->uc_mcontext.gregs[REG_R10];
    cr_ctx->cpu.r11 = cur_ctx->uc_mcontext.gregs[REG_R11];
    cr_ctx->cpu.r12 = cur_ctx->uc_mcontext.gregs[REG_R12];
    cr_ctx->cpu.r13 = cur_ctx->uc_mcontext.gregs[REG_R13];
    cr_ctx->cpu.r14 = cur_ctx->uc_mcontext.gregs[REG_R14];
    cr_ctx->cpu.r15 = cur_ctx->uc_mcontext.gregs[REG_R15];
    cr_ctx->cpu.rdi = cur_ctx->uc_mcontext.gregs[REG_RDI];
    cr_ctx->cpu.rsi = cur_ctx->uc_mcontext.gregs[REG_RSI];
    cr_ctx->cpu.rbp = cur_ctx->uc_mcontext.gregs[REG_RBP];
    cr_ctx->cpu.rbx = cur_ctx->uc_mcontext.gregs[REG_RBX];
    cr_ctx->cpu.rdx = cur_ctx->uc_mcontext.gregs[REG_RDX];
    cr_ctx->cpu.rax = cur_ctx->uc_mcontext.gregs[REG_RAX];
    cr_ctx->cpu.rcx = cur_ctx->uc_mcontext.gregs[REG_RCX];
    cr_ctx->cpu.rsp = cur_ctx->uc_mcontext.gregs[REG_RSP];
    cr_ctx->cpu.rip = cur_ctx->uc_mcontext.gregs[REG_RIP];
    cr_ctx->cpu.efl = cur_ctx->uc_mcontext.gregs[REG_EFL];
}

void restore_context(afx_context* cr_ctx, ucontext_t *cur_ctx){
    cur_ctx->uc_mcontext.gregs[REG_R8] = cr_ctx->cpu.r8;
    cur_ctx->uc_mcontext.gregs[REG_R9] = cr_ctx->cpu.r9;
    cur_ctx->uc_mcontext.gregs[REG_R10] = cr_ctx->cpu.r10;
    cur_ctx->uc_mcontext.gregs[REG_R11] = cr_ctx->cpu.r11;
    cur_ctx->uc_mcontext.gregs[REG_R12] = cr_ctx->cpu.r12;
    cur_ctx->uc_mcontext.gregs[REG_R13] = cr_ctx->cpu.r13;
    cur_ctx->uc_mcontext.gregs[REG_R14] = cr_ctx->cpu.r14;
    cur_ctx->uc_mcontext.gregs[REG_R15] = cr_ctx->cpu.r15;
    cur_ctx->uc_mcontext.gregs[REG_RDI] = cr_ctx->cpu.rdi;
    cur_ctx->uc_mcontext.gregs[REG_RSI] = cr_ctx->cpu.rsi;
    cur_ctx->uc_mcontext.gregs[REG_RBP] = cr_ctx->cpu.rbp;
    cur_ctx->uc_mcontext.gregs[REG_RBX] = cr_ctx->cpu.rbx;
    cur_ctx->uc_mcontext.gregs[REG_RDX] = cr_ctx->cpu.rdx;
    cur_ctx->uc_mcontext.gregs[REG_RAX] = cr_ctx->cpu.rax;
    cur_ctx->uc_mcontext.gregs[REG_RCX] = cr_ctx->cpu.rcx;
    cur_ctx->uc_mcontext.gregs[REG_RSP] = cr_ctx->cpu.rsp;
    cur_ctx->uc_mcontext.gregs[REG_RIP] = cr_ctx->cpu.rip;
    cur_ctx->uc_mcontext.gregs[REG_EFL] = cr_ctx->cpu.efl;
}

void print(int x){
    int y = 0;
    while(1){
        printf("Executing...%d-%d\n", x, x+y);
        y = x+y;
        usleep(100*1000);
    }
}

void make_context(void* fp, int x){
    if(afxv.len == afxv.cap){
        afxv.ptr = realloc(afxv.ptr, sizeof(afx_context)*afxv.cap*2);
        afxv.cap *= 2;
    }
    void* new_stack_base = malloc(STACK_SIZE);
    afxv.len++;
    afxv.ptr[afxv.len-1].cpu.rip = (uint64_t)fp;
    afxv.ptr[afxv.len-1].cpu.rdi = x;
    afxv.ptr[afxv.len-1].cpu.rsp = (uint64_t)new_stack_base + STACK_SIZE;
    stack_limit += STACK_SIZE;
}

void* monitor(void* arg){
    while(1){
        int rc;
        printf("Sending SIGURG\n");
        rc = pthread_kill(executor_t, SIGURG);
        if(rc != 0){
            printf("pthread_kill error");
            exit(1);
        }
        usleep(1000*1000);
    }
    return NULL;
}

void handle_sigurg(int signum, siginfo_t *info, void *ctx_ptr){
    if(signum == SIGURG){
        printf("Received sigurg\n");

        ucontext_t *ctx = (ucontext_t*)ctx_ptr;
        save_context(&afxv.ptr[cur_f_index], ctx);
        
        cur_f_index = (cur_f_index+1)%afxv.len;
        restore_context(&afxv.ptr[cur_f_index], ctx);
    }
}

void* executor(){
    int rc;

    struct sigaction sa;
    sa.sa_sigaction = handle_sigurg;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    
    rc = sigaction(SIGURG, &sa, NULL);
    if(rc != 0){
        printf("sigaction error in executor\n");
        exit(1);
    }

    asm volatile(
        "movq   %%rsp,  %0\n\t"
        : "=r"(executor_rsp)
    );

    // make context for functions
    make_context(&print, 1);
    make_context(&print, 2);

    sleep(-1);
    return NULL;
}

int afx_init(){
    int rc;
    afxv.len = 0;
    afxv.cap = 0;
    
    rc = pthread_create(&executor_t, NULL, executor, NULL);
    if(rc != 0){
        printf("Error creating executor thread");
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
    rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }
    sleep(100);
    return 0;
}

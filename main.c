#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h> 

#define STACK_SIZE  (1024*4)

typedef struct {
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
    uint64_t efl;
} cpu_context;

typedef struct {
    cpu_context cpu;
    void* stack;
} afx_context;

struct afx_list {
    afx_context* ptr;
    struct afx_list* next;
};
typedef struct afx_list afx_list;
afx_list* last = NULL;

uint64_t rbp_caller;
uint64_t rbp_callee;
uint64_t copy_size;
uint64_t executor_addr;
uint64_t rdi, rsi, rdx, rcx, r8, r9;
void* copy_src;
void* copy_dest;

int is_running = 0;
afx_list context_list;
afx_list* cur_node = NULL;

afx_context* get_new_context(){
    afx_context* new_ctx = malloc(sizeof(afx_context));
    if (new_ctx == NULL) {
        perror("malloc failed for new_ctx");
        exit(EXIT_FAILURE);
    }

    new_ctx->stack = malloc(STACK_SIZE);
     if (new_ctx->stack == NULL) {
        perror("malloc failed for new_ctx->stack");
        exit(EXIT_FAILURE);
    }
    new_ctx->stack += STACK_SIZE;
    return new_ctx;
}

void add_ctx_to_queue(afx_context* new_ctx){
    afx_list* new_node = (afx_list*)malloc(sizeof(afx_list));
    new_node->ptr = new_ctx;
    if(last == NULL){
        last = new_node;
        last->next = last;
        cur_node = last;
    } else {
        new_node->next = last->next;
        last->next = new_node;
    }
}

#define save_cpu_context {\
    asm volatile (\
        "movq   %%rdi,  %0\n\t"\
        "movq   %%rsi,  %1\n\t"\
        "movq   %%rdx,  %2\n\t"\
        "movq   %%rcx,  %3\n\t"\
        "movq   %%r8,   %4\n\t"\
        "movq   %%r9,   %5\n\t"\
        "movq   %%rbp,  %6\n\t"\
        : "=m"(rdi), "=m"(rsi), "=m"(rdx), "=m"(rcx),\
          "=m"(r8), "=m"(r9), "=m"(rbp_callee)\
    );\
    \
    afx_context* new_ctx = get_new_context();\
    new_ctx->cpu.rdi = rdi;\
    new_ctx->cpu.rsi = rsi;\
    new_ctx->cpu.rdx = rdx;\
    new_ctx->cpu.rcx = rcx;\
    new_ctx->cpu.r8 = r8;\
    new_ctx->cpu.r9 = r9;\
    \
    asm volatile (\
        "movq   %%rax, 48(%0)\n\t"\
        "movq   %%rbx, 56(%0)\n\t"\
        "movq   %%rbp, 64(%0)\n\t"\
        "movq   %%r10, 72(%0)\n\t"\
        "movq   %%r11, 80(%0)\n\t"\
        "movq   %%r12, 88(%0)\n\t"\
        "movq   %%r13, 96(%0)\n\t"\
        "movq   %%r14, 104(%0)\n\t"\
        "movq   %%r15, 112(%0)\n\t"\
        "movq   %%rsp, 120(%0)\n\t"\
        \
        "leaq   1f(%%rip), %%rax\n\t"\
        "movq   %%rax, 128(%0)\n\t"\
        "pushfq\n\t"\
        "popq   136(%0)\n\t"\
        :\
        : "r" (&new_ctx->cpu)\
        : "rax", "memory"\
    );\
    \
    if (rbp_caller > rbp_callee && rbp_caller - rbp_callee < STACK_SIZE) {\
        copy_size = 16;\
        new_ctx->stack -= copy_size;\
        copy_dest = (void*)(new_ctx->stack);\
        copy_src = (void*)(rbp_callee);\
        asm volatile (\
            "rep movsb"\
            : "+D"(copy_dest), "+S"(copy_src), "+c"(copy_size)\
            :: "memory"\
        );\
        copy_size = rbp_caller - rbp_callee - 16;\
        new_ctx->stack -= copy_size;\
        copy_dest = (void*)(new_ctx->stack);\
        copy_src = (void*)(rbp_callee + 16);\
        new_ctx->cpu.rbp = (uint64_t)new_ctx->stack;\
        new_ctx->cpu.rsp = (uint64_t)new_ctx->stack;\
        asm volatile (\
            "rep movsb"\
            : "+D"(copy_dest), "+S"(copy_src), "+c"(copy_size)\
            :: "memory"\
        );\
    } else {\
        printf("Invalid stack size while copying stack\n");\
        exit(-1);\
    }\
    \
    add_ctx_to_queue(new_ctx);\
    \
    asm volatile(\
        "leave\n\t"\
        "ret\n\t"\
        "1:\n\t"\
    );\
}

#define afx(fn)\
    asm ("movq %%rbp, %0\n\t": "=r"(rbp_caller));\
    __AFX_PREFIX_##fn;

#define async(ret_type, fn, args, body)\
    ret_type fn args{\
        body\
    }\
    ret_type __AFX_PREFIX_##fn args {\
        save_cpu_context\
        asm volatile(\
            "callq  %0\n\t"\
            ::"r"(fn)\
        );\
        asm volatile(\
            "jmp *%0\n\t"\
            ::"r"(executor_addr)\
        );\
    }

void save_context(afx_context* fn_ctx, ucontext_t *kernel_ctx){
    fn_ctx->cpu.r8 = kernel_ctx->uc_mcontext.gregs[REG_R8];
    fn_ctx->cpu.r9 = kernel_ctx->uc_mcontext.gregs[REG_R9];
    fn_ctx->cpu.r10 = kernel_ctx->uc_mcontext.gregs[REG_R10];
    fn_ctx->cpu.r11 = kernel_ctx->uc_mcontext.gregs[REG_R11];
    fn_ctx->cpu.r12 = kernel_ctx->uc_mcontext.gregs[REG_R12];
    fn_ctx->cpu.r13 = kernel_ctx->uc_mcontext.gregs[REG_R13];
    fn_ctx->cpu.r14 = kernel_ctx->uc_mcontext.gregs[REG_R14];
    fn_ctx->cpu.r15 = kernel_ctx->uc_mcontext.gregs[REG_R15];
    fn_ctx->cpu.rdi = kernel_ctx->uc_mcontext.gregs[REG_RDI];
    fn_ctx->cpu.rsi = kernel_ctx->uc_mcontext.gregs[REG_RSI];
    fn_ctx->cpu.rbp = kernel_ctx->uc_mcontext.gregs[REG_RBP];
    fn_ctx->cpu.rbx = kernel_ctx->uc_mcontext.gregs[REG_RBX];
    fn_ctx->cpu.rdx = kernel_ctx->uc_mcontext.gregs[REG_RDX];
    fn_ctx->cpu.rax = kernel_ctx->uc_mcontext.gregs[REG_RAX];
    fn_ctx->cpu.rcx = kernel_ctx->uc_mcontext.gregs[REG_RCX];
    fn_ctx->cpu.rsp = kernel_ctx->uc_mcontext.gregs[REG_RSP];
    fn_ctx->cpu.rip = kernel_ctx->uc_mcontext.gregs[REG_RIP];
    fn_ctx->cpu.efl = kernel_ctx->uc_mcontext.gregs[REG_EFL];
}

void restore_context(afx_context* fn_ctx, ucontext_t *kernel_ctx){
    kernel_ctx->uc_mcontext.gregs[REG_R8] = fn_ctx->cpu.r8;
    kernel_ctx->uc_mcontext.gregs[REG_R9] = fn_ctx->cpu.r9;
    kernel_ctx->uc_mcontext.gregs[REG_R10] = fn_ctx->cpu.r10;
    kernel_ctx->uc_mcontext.gregs[REG_R11] = fn_ctx->cpu.r11;
    kernel_ctx->uc_mcontext.gregs[REG_R12] = fn_ctx->cpu.r12;
    kernel_ctx->uc_mcontext.gregs[REG_R13] = fn_ctx->cpu.r13;
    kernel_ctx->uc_mcontext.gregs[REG_R14] = fn_ctx->cpu.r14;
    kernel_ctx->uc_mcontext.gregs[REG_R15] = fn_ctx->cpu.r15;
    kernel_ctx->uc_mcontext.gregs[REG_RDI] = fn_ctx->cpu.rdi;
    kernel_ctx->uc_mcontext.gregs[REG_RSI] = fn_ctx->cpu.rsi;
    kernel_ctx->uc_mcontext.gregs[REG_RBP] = fn_ctx->cpu.rbp;
    kernel_ctx->uc_mcontext.gregs[REG_RBX] = fn_ctx->cpu.rbx;
    kernel_ctx->uc_mcontext.gregs[REG_RDX] = fn_ctx->cpu.rdx;
    kernel_ctx->uc_mcontext.gregs[REG_RAX] = fn_ctx->cpu.rax;
    kernel_ctx->uc_mcontext.gregs[REG_RCX] = fn_ctx->cpu.rcx;
    kernel_ctx->uc_mcontext.gregs[REG_RSP] = fn_ctx->cpu.rsp;
    kernel_ctx->uc_mcontext.gregs[REG_RIP] = fn_ctx->cpu.rip;
    kernel_ctx->uc_mcontext.gregs[REG_EFL] = fn_ctx->cpu.efl;
}

void* monitor(void* arg){
    pthread_t executor_t = *(pthread_t*)(arg);
    while(1){
        int rc;
        printf("Sending SIGURG\n");
        if(last != NULL){
            rc = pthread_kill(executor_t, SIGURG);
            if(rc != 0){
                printf("pthread_kill error");
                exit(1);
            }
        }
        usleep(1000*1000);
    }
    return NULL;
}

void handle_sigurg(int signum, siginfo_t *info, void *ctx_ptr){
    if(signum == SIGURG){
        printf("Received sigurg\n");

        ucontext_t *kernel_ctx = (ucontext_t*)ctx_ptr;
        if(is_running == 1){
            save_context(cur_node->ptr, kernel_ctx);
        } else {
            cur_node = last;
            is_running = 1;
        }

        cur_node = cur_node->next;
        restore_context(cur_node->ptr, kernel_ctx);
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

    while(1){
        asm volatile("executor_loop_start:\n\t");
        asm volatile("leaq executor_loop_start(%%rip), %0" : "=r"(executor_addr));
        pause();
    }
    return NULL;
}

int afx_init(){
    int rc = 0;

    pthread_t *executor_tp = (pthread_t*)malloc(sizeof(pthread_t));
    rc = pthread_create(executor_tp, NULL, executor, NULL);
    if(rc != 0){
        printf("Error creating executor thread");
        return rc;
    }

    pthread_t monitor_t;
    rc = pthread_create(&monitor_t, NULL, monitor, (void*)executor_tp);
    if(rc != 0){
        printf("Error creating monitor thread");
        return rc;
    }

    return 0;
}

async(
    void, print, (int a, int b, int c, int d, int e, int f, int g, int h), {
        printf("a is %d\n", a);
        printf("b is %d\n", b);
        printf("c is %d\n", c);
        printf("d is %d\n", d);
        printf("e is %d\n", e);
        printf("f is %d\n", f);
        printf("g is %d\n", g);
        printf("h is %d\n", h);
        printf("sum is %d\n", a+b+c+d+e+f+g+h);
    }
)

int main(){
    int rc = 0;
    rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }

    print(1,2,3,4,5,6,7,8);
    afx(print(1,2,3,4,5,6,7,8));

    printf("HELLO\n");
   
    pause();
    return 0;
}

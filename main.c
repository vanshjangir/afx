#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h> 

#define STACK_SIZE  (1024*4)

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


long rbp_caller;
afx_list context_list;
afx_list* cur_f = NULL;


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
    return new_ctx;
}

void add_ctx_to_queue(afx_context* new_ctx){
    afx_list* new_node = (afx_list*)malloc(sizeof(afx_list));
    new_node->ptr = new_ctx;
    if(last == NULL){
        last = new_node;
        last->next = last;
    } else {
        new_node->next = last->next;
        last->next = new_node;
    }
}


#define save_cpu_context \
    do{\
        long rdi, rsi, rdx, rcx, r8, r9, rbp_callee; \
        __asm__ __volatile__ (\
            "movq   %%rdi,  %0\n\t"\
            "movq   %%rsi,  %1\n\t"\
            "movq   %%rdx,  %2\n\t"\
            "movq   %%rcx,  %3\n\t"\
            "movq   %%r8,   %4\n\t"\
            "movq   %%r9,   %5\n\t"\
            "movq   %%rbp,  %6\n\t"\
            : "=m"(rdi), "=m"(rsi), "=m"(rdx), "=m"(rcx), \
              "=m"(r8), "=m"(r9), "=m"(rbp_callee) \
        );\
        \
        afx_context* new_ctx = get_new_context();\
        new_ctx->cpu.rdi = rdi; \
        new_ctx->cpu.rsi = rsi; \
        new_ctx->cpu.rdx = rdx; \
        new_ctx->cpu.rcx = rcx; \
        new_ctx->cpu.r8 = r8; \
        new_ctx->cpu.r9 = r9; \
        \
        __asm__ __volatile__ (\
            "movq   %%rax, 48(%0)\n\t"  \
            "movq   %%rbx, 56(%0)\n\t"  \
            "movq   %%rbp, 64(%0)\n\t"  \
            "movq   %%r10, 72(%0)\n\t"  \
            "movq   %%r11, 80(%0)\n\t"  \
            "movq   %%r12, 88(%0)\n\t"  \
            "movq   %%r13, 96(%0)\n\t"  \
            "movq   %%r14, 104(%0)\n\t" \
            "movq   %%r15, 112(%0)\n\t" \
            "movq   %%rsp, 120(%0)\n\t" \
            \
            "leaq   1f(%%rip), %%rax\n\t" \
            "movq   %%rax, 128(%0)\n\t" \
            "1:\n\t" \
            "pushfq\n\t" \
            "popq   136(%0)\n\t" \
            :\
            : "r" (&new_ctx->cpu) \
            : "rax", "memory" \
        ); \
        \
        void* copy_dest = (void*)new_ctx->stack;\
        void* copy_src = (void*)(rbp_callee);\
        long copy_size = rbp_caller - rbp_callee;\
        \
        if (copy_size > 0 && copy_size < STACK_SIZE) {\
            __asm__ __volatile__ (\
                "rep movsb"\
                : "+D"(copy_dest), "+S"(copy_src), "+c"(copy_size)\
                :: "memory"\
            );\
        }\
        /*printf("rdi = %ld\n", cur_ctx->cpu.rdi);\*/\
        /*printf("rsi = %ld\n", cur_ctx->cpu.rsi);\*/\
        /*printf("rdx = %ld\n", cur_ctx->cpu.rdx);\*/\
        /*printf("rcx = %ld\n", cur_ctx->cpu.rcx);\*/\
        /*printf("r8 = %ld\n", cur_ctx->cpu.r8);\*/\
        /*printf("r9 = %ld\n", cur_ctx->cpu.r9);\*/\
        /*long stack_offset = rbp_caller - (long)copy_src;\*/\
        /*printf("seven = %ld\n", *(long*)(cur_ctx->stack + stack_offset + 16));\*/\
        /*printf("eight = %ld\n", *(long*)(cur_ctx->stack + stack_offset + 24));\*/\
        /*printf("nine = %ld\n", *(long*)(cur_ctx->stack + stack_offset + 32));\*/\
        /*printf("rbp_callee = %ld\n", rbp_callee);\*/\
        add_ctx_to_queue(new_ctx);\
    } while(0);\
    \
    __asm__ __volatile__(\
        "leave\n\t"\
        "ret\n\t"\
    );\
    

// Calls an async function
#define afx(fn)\
    __asm__ ("movq   %%rbp,  %0\n\t":   "=r"(rbp_caller));\
    __AFX_PREFIX_##fn;


// Defines an async function
#define async(ret_type, fn, body)\
    ret_type __AFX_PREFIX_##fn{\
        save_cpu_context\
    body\
    }

// Defines both async and normal function
#define async_plus(ret_type, fn, body)\
    ret_type fn{\
    body\
    }\
    ret_type __AFX_PREFIX_##fn{\
        save_cpu_context\
    body\
    }

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

void* monitor(void* arg){
    pthread_t executor_t = *(pthread_t*)(arg);
    while(1){
        int rc;
        printf("Sending SIGURG\n");
        if(last != NULL && last->next != last){
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

        ucontext_t *ctx = (ucontext_t*)ctx_ptr;
        save_context(cur_f->ptr, ctx);
        
        cur_f = cur_f->next;
        restore_context(cur_f->ptr, ctx);
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

    sleep(-1);
    return NULL;
}

int afx_init(){
    int rc;
    
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
    void, print(int a, int b, int c, int d, int e, int f, int g, int h, int i), {
        //while(1){
        //    printf("Executing...%d-%d\n", x, y);
        //    y = x + y;
        //    x = y - x;
        //    usleep(100*1000);
        //}
        printf("%d", a+b+c+d+e+f+g+h+i);
    }
)



int main(){
    int rc = 0;
    //rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }

    //print(1,2);
    afx(print(1,2,3,4,5,6,7,8,9));
    
    return 0;
}

#include "afx.h"

int _AFX_NUM_FUNC = 0;
int _afx_is_running = 0;
void* _afx_copy_src = NULL;
void* _afx_copy_dest = NULL;
afx_list_node* _afx_start = NULL;
afx_list_node* _afx_cur_node = NULL;

uint64_t _afx_rbp_caller = 0;
uint64_t _afx_rbp_callee = 0;
uint64_t _afx_copy_size = 0;
uint64_t _afx_executor_addr = 0;
uint64_t _afx_rdi, _afx_rsi, _afx_rdx, _afx_rcx, _afx_r8, _afx_r9;


afx_context* _afx_get_new_context(){
    afx_context* new_ctx = (afx_context*)malloc(sizeof(afx_context));
    if (new_ctx == NULL) {
        printf("malloc failed for new_ctx\n");
        exit(EXIT_FAILURE);
    }

    new_ctx->base = malloc(STACK_SIZE);
    new_ctx->stack = new_ctx->base;
    if (new_ctx->stack == NULL) {
        printf("malloc failed for new_ctx->stack\n");
        exit(EXIT_FAILURE);
    }
    new_ctx->stack = (char*)new_ctx->stack + STACK_SIZE;
    return new_ctx;
}

void _afx_add_ctx_to_queue(afx_context* new_ctx){
    afx_list_node* new_node = (afx_list_node*)malloc(sizeof(afx_list_node));
    new_node->ptr = new_ctx;
    if(_AFX_NUM_FUNC == 0){
        _afx_start = new_node;
        _afx_start->next = new_node;
        _afx_start->prev = new_node;
    } else {
        new_node->next = _afx_start;
        new_node->prev = _afx_start->prev;
        _afx_start->prev->next = new_node;
        _afx_start->prev = new_node;
        _afx_start = new_node;
    }
    _AFX_NUM_FUNC += 1;
}

void _afx_delete_context(){
    if(_AFX_NUM_FUNC == 1){
        free(_afx_cur_node->ptr->base);
        free(_afx_cur_node->ptr);
        _afx_cur_node = NULL;
        _afx_start = NULL;
        _afx_is_running = 0;
    } else {
        _afx_cur_node->prev->next = _afx_cur_node->next;
        _afx_cur_node->next->prev = _afx_cur_node->prev;
        free(_afx_cur_node->ptr->base);
        free(_afx_cur_node->ptr);
    }
    _AFX_NUM_FUNC -= 1;
}

void _afx_save_context(afx_context* fn_ctx, ucontext_t *kernel_ctx){
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

void _afx_restore_context(afx_context* fn_ctx, ucontext_t *kernel_ctx){
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

void* _afx_monitor(void* arg){
    pthread_t executor_t = *(pthread_t*)(arg);
    while(1){
        if(_AFX_NUM_FUNC > 0){
            int rc = pthread_kill(executor_t, SIGURG);
            if(rc != 0){
                printf("pthread_kill error\n");
                exit(1);
            }
        }
        usleep(10*1000);
    }
    return NULL;
}

void _afx_handle_sigurg(int signum, siginfo_t *info, void *ctx_ptr){
    if(signum == SIGURG){
        if(_AFX_NUM_FUNC == 0)
            return;
        
        ucontext_t *kernel_ctx = (ucontext_t*)ctx_ptr;
        if(_afx_is_running == 1){
            _afx_save_context(_afx_cur_node->ptr, kernel_ctx);
            _afx_cur_node = _afx_cur_node->next;
        } else {
            _afx_cur_node = _afx_start;
            _afx_is_running = 1;
        }

        _afx_restore_context(_afx_cur_node->ptr, kernel_ctx);
    }
}

void* _afx_executor(void* arg){
    int rc;

    struct sigaction sa;
    sa.sa_sigaction = _afx_handle_sigurg;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    rc = sigaction(SIGURG, &sa, NULL);
    if(rc != 0){
        printf("sigaction error in executor\n");
        exit(1);
    }

    while(1){
        asm volatile("executor_loop_start:\n\t");
        asm volatile(
            "leaq executor_loop_start(%%rip), %0"
            :"=r"(_afx_executor_addr)
        );
        pause();
    }
    return NULL;
}

int afx_init(){
    int rc = 0;

    pthread_t *executor_tp = (pthread_t*)malloc(sizeof(pthread_t));
    rc = pthread_create(executor_tp, NULL, _afx_executor, NULL);
    if(rc != 0){
        printf("Error creating executor thread\n");
        return rc;
    }

    pthread_t monitor_t;
    rc = pthread_create(&monitor_t, NULL, _afx_monitor, (void*)executor_tp);
    if(rc != 0){
        printf("Error creating monitor thread\n");
        return rc;
    }

    return 0;
}

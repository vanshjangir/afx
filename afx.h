#pragma once

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <ucontext.h>

#ifndef STACK_SIZE
#define STACK_SIZE  (1024*4)
#endif

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
    void* base;
} afx_context;

struct afx_list_node {
    afx_context* ctx;
    struct afx_list_node* next;
    struct afx_list_node* prev;
};
typedef struct afx_list_node afx_list_node;

extern pthread_mutex_t afx_mutex;

extern int afx_num_func;
extern int afx_is_running;
extern int afx_deletion_mark;

extern void* afx_copy_src;
extern void* afx_copy_dest;

extern afx_list_node* afx_last;
extern afx_list_node* afx_cur_node;

extern uint64_t afx_rbp_caller;
extern uint64_t afx_rbp_callee;
extern uint64_t afx_copy_size;
extern uint64_t afx_executor_addr;
extern uint64_t afx_rdi, afx_rsi, afx_rdx, afx_rcx, afx_r8, afx_r9;


afx_context* afx_get_new_context();
void afx_add_ctx_to_queue(afx_context* new_ctx);
void afx_delete_context();
void afx_mark_for_deletion();
void afx_save_context(afx_context* fn_ctx, ucontext_t *kernel_ctx);
void afx_restore_context(afx_context* fn_ctx, ucontext_t *kernel_ctx);
void afx_handle_sigurg(int signum, siginfo_t *info, void *ctx_ptr);
void* afx_monitor(void* arg);
void* afx_executor(void* arg);
int afx_init();


#define save_cpu_context {\
    asm volatile (\
        "movq   %%rdi,  %0\n\t"\
        "movq   %%rsi,  %1\n\t"\
        "movq   %%rdx,  %2\n\t"\
        "movq   %%rcx,  %3\n\t"\
        "movq   %%r8,   %4\n\t"\
        "movq   %%r9,   %5\n\t"\
        "movq   %%rbp,  %6\n\t"\
        : "=m"(afx_rdi), "=m"(afx_rsi), "=m"(afx_rdx), "=m"(afx_rcx),\
          "=m"(afx_r8), "=m"(afx_r9), "=m"(afx_rbp_callee)\
    );\
    \
    afx_context* new_ctx = afx_get_new_context();\
    new_ctx->cpu.rdi = afx_rdi;\
    new_ctx->cpu.rsi = afx_rsi;\
    new_ctx->cpu.rdx = afx_rdx;\
    new_ctx->cpu.rcx = afx_rcx;\
    new_ctx->cpu.r8 = afx_r8;\
    new_ctx->cpu.r9 = afx_r9;\
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
    if(afx_rbp_caller > afx_rbp_callee &&\
        afx_rbp_caller - afx_rbp_callee < STACK_SIZE){\
        \
        afx_copy_size = afx_rbp_caller - afx_rbp_callee - 16;\
        new_ctx->stack = (char*)new_ctx->stack - afx_copy_size;\
        afx_copy_dest = (void*)(new_ctx->stack);\
        afx_copy_src = (void*)(afx_rbp_callee + 16);\
        new_ctx->cpu.rbp = (uint64_t)new_ctx->stack;\
        new_ctx->cpu.rsp = (uint64_t)new_ctx->stack;\
        asm volatile (\
            "rep movsb"\
            : "+D"(afx_copy_dest), "+S"(afx_copy_src), "+c"(afx_copy_size)\
            :: "memory"\
        );\
    }\
    else{\
        printf("Invalid stack size while copying stack\n");\
        exit(-1);\
    }\
    \
    afx_add_ctx_to_queue(new_ctx);\
    \
    asm volatile(\
        "leave\n\t"\
        "ret\n\t"\
        "1:\n\t"\
    );\
}

#define afx(fn)\
    asm ("movq %%rbp, %0\n\t": "=r"(afx_rbp_caller));\
    __AFX_PREFIX_##fn;

#define async(ret_type, fn, args, body)\
    ret_type fn args{\
        body\
    }\
    __attribute__((noinline)) ret_type __AFX_PREFIX_##fn args {\
        save_cpu_context\
        asm volatile(\
            "callq  *%0\n\t"\
            ::"r"(fn)\
        );\
        afx_mark_for_deletion();\
        asm volatile(\
            "jmp *%0\n\t"\
            ::"r"(afx_executor_addr)\
        );\
    }

#define async_dec(ret_type, fn)\
    ret_type __AFX_PREFIX_##fn;

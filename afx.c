#include "afx.h"
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

async_dec(void, background_sleeper(void));

static void safe_lock(pthread_mutex_t*);
static void safe_unlock(pthread_mutex_t*);

static list_node* afx_list_head = NULL;
static list_node* afx_cur_node = NULL;
static list_node* afx_background_sleeper = NULL;

static list_node* afx_state_map[NUM_FUNC] = {0};
 
static int num_func = 0;
static int afx_deletion_mark = 0;
static int afx_is_running = 0;
 
static pthread_t* afx_executor_t = NULL;
static pthread_mutex_t afx_mutex;
static int afx_epoll_fd = 0;

void* afx_copy_src;
void* afx_copy_dest;

uint64_t afx_rbp_caller;
uint64_t afx_rbp_callee;
uint64_t afx_copy_size;
uint64_t afx_rdi, afx_rsi, afx_rdx, afx_rcx, afx_r8, afx_r9;

static list_node* create_node_safe() {
    list_node* new_node = malloc(sizeof(list_node));
    if(!new_node)
        goto alloc_error;

    new_node->ctx = malloc(sizeof(func_context));
    if(!new_node->ctx)
        goto alloc_error;

    new_node->ctx->base = aligned_alloc(PAGE_SIZE, STACK_SIZE);
    if(!new_node->ctx->base)
        goto alloc_error;
    
    new_node->ctx->stack = new_node->ctx->base + STACK_SIZE;
    new_node->ctx->state = RUNNABLE;
    return new_node;

alloc_error:
    printf("Allocation error in create_node_safe");
    exit(EXIT_FAILURE);
}

static void free_node_safe(list_node* node) {
    if (!node) return;
    free(node->ctx->base);
    free(node->ctx);
    free(node);
}

list_node* afx_add_new_node(){
    safe_lock(&afx_mutex);
    
    list_node* new_node = create_node_safe();
    
    if(afx_list_head == NULL){
        afx_list_head = new_node;
        afx_background_sleeper = new_node;
        afx_list_head->next = afx_list_head;
        afx_list_head->prev = afx_list_head;
    }
    else{
        list_node* last = afx_list_head->prev;
        last->next = new_node;
        new_node->prev = last;
        new_node->next = afx_list_head;
        afx_list_head->prev = new_node;
    }
    
    num_func += 1;

    safe_unlock(&afx_mutex);
    return new_node;
}

void afx_mark_for_deletion(){
    safe_lock(&afx_mutex);
    afx_deletion_mark = 1;
    safe_unlock(&afx_mutex);
}

static list_node* afx_delete_node(list_node* node){
    safe_lock(&afx_mutex);
    
    list_node* next_node;
    list_node* node_to_be_freed = node;
    if(node->next == node){
        next_node = NULL;
        afx_list_head = NULL;
        afx_cur_node = NULL;
    }
    else{
        next_node = node->next;
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    
    free_node_safe(node_to_be_freed);

    num_func -= 1;
    afx_is_running = 0;
    afx_deletion_mark = 0;
    
    safe_unlock(&afx_mutex);
    return next_node;
}

static void block_sigurg(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGURG);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

static void unblock_sigurg(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGURG);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

static void safe_lock(pthread_mutex_t* mutex){
    block_sigurg();
    pthread_mutex_lock(mutex);
}

static void safe_unlock(pthread_mutex_t* mutex){
    pthread_mutex_unlock(mutex);
    unblock_sigurg();
}

static void afx_save_context(func_context* fn_ctx, ucontext_t *kernel_ctx){
    gregset_t* reg = &(kernel_ctx->uc_mcontext.gregs);
    fn_ctx->cpu.r8 = (*reg)[REG_R8];
    fn_ctx->cpu.r9 = (*reg)[REG_R9];
    fn_ctx->cpu.r10 = (*reg)[REG_R10];
    fn_ctx->cpu.r11 = (*reg)[REG_R11];
    fn_ctx->cpu.r12 = (*reg)[REG_R12];
    fn_ctx->cpu.r13 = (*reg)[REG_R13];
    fn_ctx->cpu.r14 = (*reg)[REG_R14];
    fn_ctx->cpu.r15 = (*reg)[REG_R15];
    fn_ctx->cpu.rdi = (*reg)[REG_RDI];
    fn_ctx->cpu.rsi = (*reg)[REG_RSI];
    fn_ctx->cpu.rbp = (*reg)[REG_RBP];
    fn_ctx->cpu.rbx = (*reg)[REG_RBX];
    fn_ctx->cpu.rdx = (*reg)[REG_RDX];
    fn_ctx->cpu.rax = (*reg)[REG_RAX];
    fn_ctx->cpu.rcx = (*reg)[REG_RCX];
    fn_ctx->cpu.rsp = (*reg)[REG_RSP];
    fn_ctx->cpu.rip = (*reg)[REG_RIP];
    fn_ctx->cpu.efl = (*reg)[REG_EFL];
}

static void afx_restore_context(func_context* fn_ctx, ucontext_t *kernel_ctx){
    gregset_t* reg = &(kernel_ctx->uc_mcontext.gregs);
    (*reg)[REG_R8] = fn_ctx->cpu.r8;
    (*reg)[REG_R9] = fn_ctx->cpu.r9;
    (*reg)[REG_R10] = fn_ctx->cpu.r10;
    (*reg)[REG_R11] = fn_ctx->cpu.r11;
    (*reg)[REG_R12] = fn_ctx->cpu.r12;
    (*reg)[REG_R13] = fn_ctx->cpu.r13;
    (*reg)[REG_R14] = fn_ctx->cpu.r14;
    (*reg)[REG_R15] = fn_ctx->cpu.r15;
    (*reg)[REG_RDI] = fn_ctx->cpu.rdi;
    (*reg)[REG_RSI] = fn_ctx->cpu.rsi;
    (*reg)[REG_RBP] = fn_ctx->cpu.rbp;
    (*reg)[REG_RBX] = fn_ctx->cpu.rbx;
    (*reg)[REG_RDX] = fn_ctx->cpu.rdx;
    (*reg)[REG_RAX] = fn_ctx->cpu.rax;
    (*reg)[REG_RCX] = fn_ctx->cpu.rcx;
    (*reg)[REG_RSP] = fn_ctx->cpu.rsp;
    (*reg)[REG_RIP] = fn_ctx->cpu.rip;
    (*reg)[REG_EFL] = fn_ctx->cpu.efl;
}


int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int register_epoll(int fd, unsigned int flags){
    int rc = 0;
    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = fd;
    rc = epoll_ctl(afx_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if(rc == -1 && errno == EEXIST){
        rc = epoll_ctl(afx_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }
    return rc;
}

static void blocking_socket_prologue(int fd, unsigned int flags){
    block_sigurg();
    afx_state_map[fd % NUM_FUNC] = afx_cur_node;
    afx_cur_node->ctx->state = BLOCKED_ON_IO;
    make_nonblocking(fd);
    register_epoll(fd, flags);
    unblock_sigurg();
}

int afx_accept(int sock, struct sockaddr* addr, socklen_t* sock_len){
    blocking_socket_prologue(sock, EPOLLIN);
    afx_yield();
    
    block_sigurg();
    int rc = accept(sock, addr, sock_len);
    unblock_sigurg();
    return rc;
}

int afx_connect(int sock, const struct sockaddr *addr, socklen_t addrlen){
    block_sigurg();
    make_nonblocking(sock);
    int rc = connect(sock, addr, addrlen);
    unblock_sigurg();
    return rc;
}

int afx_send(int sock, char* buf, ssize_t size, int flags){
    blocking_socket_prologue(sock, EPOLLOUT | EPOLLONESHOT);
    afx_yield();
    
    block_sigurg();
    int rc = send(sock, buf, size, flags);
    unblock_sigurg();
    return rc;
}

int afx_recv(int sock, char* buf, ssize_t size, int flags){
    blocking_socket_prologue(sock, EPOLLIN | EPOLLONESHOT);
    afx_yield();
    
    block_sigurg();
    int rc = recv(sock, buf, size, flags);
    unblock_sigurg();
    return rc;
}

void afx_sleep(unsigned int sec){
    block_sigurg();
    
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec ts = {0};
    ts.it_value.tv_sec = sec;
    ts.it_value.tv_nsec = 0;
    timerfd_settime(tfd, 0, &ts, NULL);
    
    afx_state_map[tfd % NUM_FUNC] = afx_cur_node;
    afx_cur_node->ctx->state = BLOCKED_ON_TIMER;
    register_epoll(tfd, EPOLLIN | EPOLLONESHOT);
    unblock_sigurg();

    afx_yield();

    close(tfd);
}

void afx_usleep(unsigned int microseconds){
    block_sigurg();
    
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec ts = {0};
    ts.it_value.tv_sec = microseconds / 1000000;
    ts.it_value.tv_nsec = (microseconds % 1000000) * 1000;
    timerfd_settime(tfd, 0, &ts, NULL);
    
    afx_state_map[tfd % NUM_FUNC] = afx_cur_node;
    afx_cur_node->ctx->state = BLOCKED_ON_TIMER;
    register_epoll(tfd, EPOLLIN | EPOLLONESHOT);
    unblock_sigurg();

    afx_yield();

    close(tfd);
}

static void afx_handle_sigurg(int signum, siginfo_t *info, void *ctx_ptr){
    if(signum != SIGURG)
        return;

    if(afx_deletion_mark == 1){
        afx_cur_node = afx_delete_node(afx_cur_node);
    }
    if(num_func == 0){
        return;
    }

    ucontext_t *kernel_ctx = (ucontext_t*)ctx_ptr;
    list_node* to_be_restored = NULL;
    list_node* iter = NULL;
    
    if(afx_is_running == 1){
        afx_save_context(afx_cur_node->ctx, kernel_ctx);
        iter = afx_cur_node->next;
        while(iter->ctx->state != RUNNABLE){
            iter = iter->next;
        }
        to_be_restored = iter;

        if(to_be_restored == afx_background_sleeper){
            iter = iter->next;
            while(iter != afx_cur_node){
                if(iter->ctx->state == RUNNABLE){
                    to_be_restored = iter;
                    break;
                }
                iter = iter->next;
            }
        }
    }
    else{
        afx_cur_node = afx_list_head;
        to_be_restored = afx_cur_node;
        afx_is_running = 1;
    }
    
    afx_cur_node = to_be_restored;
    afx_restore_context(afx_cur_node->ctx, kernel_ctx);
}

static void* afx_executor(void* arg){
    int rc = 0;
    struct sigaction sa;
    
    sa.sa_sigaction = afx_handle_sigurg;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    
    rc = sigaction(SIGURG, &sa, NULL);
    if(rc != 0){
        printf("sigaction error in executor\n");
        exit(EXIT_FAILURE);
    }
    
    afx(background_sleeper());

    while(1){
        pause();
    }
    return NULL;
}

static void* afx_monitor(void *arg){
    while(1){
        if(num_func > 0){
            afx_yield();
        }
        usleep(10*1000);
    }
    return NULL;
}

static void* afx_poller(void *arg){
    int rc = 0;
    struct epoll_event events[10];
    afx_epoll_fd = epoll_create1(0);
    
    while(1){
        int n = epoll_wait(afx_epoll_fd, events, 10, -1);
        for(int i = 0; i < n; i++){
            afx_state_map[events[i].data.fd % NUM_FUNC]->ctx->state = RUNNABLE;
        }
    }
    return NULL;
}

void afx_yield(){
    int rc = pthread_kill(*afx_executor_t, SIGURG);
    if(rc != 0){
        printf("pthread_kill error\n");
        exit(EXIT_FAILURE);
    }
}

int afx_init(){
    int rc = 0;
    pthread_t monitor_t;
    pthread_t poller_t;
    afx_executor_t = (pthread_t*) malloc(sizeof(pthread_t));
    
    rc = pthread_create(afx_executor_t, NULL, afx_executor, NULL);
    if(rc != 0){
        printf("Error creating executor thread\n");
        return rc;
    }

    rc = pthread_create(&monitor_t, NULL, afx_monitor, NULL);
    if(rc != 0){
        printf("Error creating monitor thread\n");
        return rc;
    }

    rc = pthread_create(&poller_t, NULL, afx_poller, NULL);
    if(rc != 0){
        printf("Error creating poller thread\n");
        return rc;
    }

    return rc;
}


async(
    void, background_sleeper, (), {
        while(1){
            pause();
        }
    }
)

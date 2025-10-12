#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../afx.h"

async_dec(void, async_client(int));

async(
    void, async_client, (int client), {
        char buf[1024];
        char reply[] = "OK\n";

        int n = afx_recv(client, buf, sizeof(buf) - 1, 0);
        printf("[%ld] recv fd=%d\n", time(NULL), client);

        afx_send(client, reply, strlen(reply), 0);
        close(client);
    }
)

int create_server(){
    int opt = 1;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 10000);

    return sock;
}

int main(){
    int rc = afx_init();
    if(rc != 0){
        printf("Error initializing afx");
        exit(-1);
    }

    int sock = create_server();
    while(1){
        int client = accept(sock, NULL, NULL);
        if(client != -1){
            afx(async_client(client));
        }
    }

    return 0;
}

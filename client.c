#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const int MAXLINE = 1024;

int main(int argc, char* argv[])
{
    // 1. create socket
    int connect_fd = socket(AF_INET, SOCK_STREAM, 0);

    // server addr for connect
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6666);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    // 2. connect
    connect(connect_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 3. send
    char buffer[MAXLINE];
    fgets(buffer, MAXLINE, stdin);
    send(connect_fd, buffer, strlen(buffer), 0);
    
    // 4. recv
    recv(connect_fd, buffer, MAXLINE, 0);
    printf("recv data: %s", buffer);

    close(connect_fd);
    return 0;
}

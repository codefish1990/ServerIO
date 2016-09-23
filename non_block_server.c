#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

const int MAX_LINE = 1024;


int min(int x, int y){return (x<y)?x:y;}
int max(int x, int y){return (x>y)?x:y;}


// non blocking recv
int non_block_recv(int fd, char* need_recv_data, int max_recv_len, int flag)
{
    char buffer[MAX_LINE];
    int recv_len = 0;
    int has = 1;
    while (has)
    {
        int recv_ret = recv(fd, buffer, sizeof(buffer), 0);

        if (recv_ret < 0)
        {
            if (errno == EAGAIN)
            {
                printf("no data for read\n");
                sleep(10);
                continue;
            }
            else
            {
                printf("recv error\n");
                close(fd);
                return -1;
            }
        }
        else if (recv_ret == 0)
        {
            close(fd);
            printf("connect close\n");
            break;
        }
        else
        {
            if (recv_ret == sizeof(buffer))
            {
                has = 1;
                strncpy(need_recv_data+recv_len, buffer, min(sizeof(buffer), max_recv_len-recv_len));
                recv_len += min(sizeof(buffer), max_recv_len-recv_len);
                if (recv_len >= max_recv_len)
                {
                    break;
                }
            }
            else
            {
                has = 0;
                strncpy(need_recv_data+recv_len, buffer, min(recv_ret, max_recv_len-recv_len));
                recv_len += min(recv_ret, max_recv_len-recv_len);
            }
        }
    }// while
    need_recv_data[recv_len] = '\0';
    return recv_len;
}


// non block send
int non_block_send(int fd, const char* data_buffer, int buffer_len, int flag)
{
    int need_send_len = buffer_len;
    char* need_send_data = data_buffer;
    while (1)
    {
        int send_ret = send(fd, need_send_data, need_send_len, 0);
        if (send_ret < 0)
        {
            if (errno == EINTR)
            {
                printf("non block send EINTR\n");
                sleep(10);
                continue;
            }
            else if (errno == EAGAIN)
            {
                printf("no data to send\n");
                sleep(10);
                continue;
            }
            else
            {
                return -1;
            }
        }

        if (send_ret == need_send_len)
        {
            return need_send_len;
        }

        need_send_len -= send_ret;
        need_send_data += send_ret;
    }// while
}


int main()
{
    // 1. create socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); 
    int flags = fcntl(listen_fd, F_GETFL);
    fcntl(listen_fd, F_SETFL, flags|O_NONBLOCK);

    struct sockaddr_in server_addr; // (ip, port) <-> (listen_fd)
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(6666);

    // 2. bind socket and ip&port
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 3. listen port=6666
    listen(listen_fd, 10);

    // 4. wait client connect;
    //    read and write
    struct sockaddr_in client_addr;
    int addr_len;
    char buffer[MAX_LINE];
    char recv_data[MAX_LINE*10+1];

    while (1)
    {
        // 5. not wait
        int connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);
        
        if (connect_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("no client connect\n");
                sleep(10);
                continue;
            }
            else
            {
                printf("accept error\n");
                return -1;
            }
        }

        // 6. recv
        int recv_len = non_block_recv(connect_fd, recv_data, sizeof(recv_data)-1, 0);
        printf("recv data: %s", recv_data);

        // 7. send
        int send_len = non_block_send(connect_fd, recv_data, strlen(recv_data), 0);
    }

    close(listen_fd);
    return 0;
}

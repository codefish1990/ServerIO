#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/epoll.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

const int MAX_LINE = 1024;
const int SELECT_TIMEOUT = 60;


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


void set_socket_non_block(int socket_fd)
{
    int flags = fcntl(socket_fd, F_GETFL);
    fcntl(socket_fd, F_SETFL, flags|O_NONBLOCK);
}


int main()
{
    // 1. create socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); 
    set_socket_non_block(listen_fd);
    // int flags = fcntl(listen_fd, F_GETFL);
    // fcntl(listen_fd, F_SETFL, flags|O_NONBLOCK);

    struct sockaddr_in server_addr; // (ip, port) <-> (listen_fd)
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(6666);

    // 2. bind socket and ip&port
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 3. listen port=6666
    listen(listen_fd, 10);

    // epoll
    // (1). 
    struct epoll_event need_listen_event, active_events[16];

    // (2). create epoll_fd
    int epoll_fd = epoll_create(1024);

    // (3). listen socket fd
    need_listen_event.data.fd = listen_fd;
    need_listen_event.events = EPOLLIN;

    // (4). register fd and event to epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &need_listen_event);

    // client_addr and data_buffer
    struct sockaddr_in client_addr;
    int addr_len;
    char recv_data[MAX_LINE*10+1];
    char buffer[MAX_LINE];

    while (1)
    {
        // 5. epoll
        int epoll_ret = epoll_wait(epoll_fd, active_events, 16, 10);// timeout = 10s
        if (epoll_ret == -1)
        {
            printf("epoll error\n");
            return -1;
        }
        else if (epoll_ret == 0)
        {
            printf("epoll timeout\n");
            sleep(10);
            continue;
        }
        else
        {
            int i;
            for (i = 0; i < epoll_ret; ++i)
            {
                if (active_events[i].data.fd == listen_fd)
                {
                    int connect_fd = accept(listen_fd, (sockaddr*)&client_addr, &addr_len);
                    set_socket_non_block(connect_fd);
                    need_listen_event.data.fd = connect_fd;
                    need_listen_event.events = EPOLLIN;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connect_fd, &need_listen_event);
                }
                else if (active_events[i].events&EPOLLIN)
                {
                    int connect_fd = active_events[i].data.fd;
                    int recv_len = non_block_recv(connect_fd, recv_data, sizeof(recv_data)-1, 0);
                    printf("recv data: %s", recv_data);

                    need_listen_event.data.fd = connect_fd;
                    need_listen_event.events = EPOLLOUT;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connect_fd, &need_listen_event);
                }
                else if (active_events[i].events&EPOLLOUT)
                {
                    int connect_fd = active_events[i].data.fd;
                    int send_len = non_block_send(connect_fd, recv_data, strlen(recv_data), 0);
                    
                    need_listen_event.data.fd = connect_fd;
                    need_listen_event.events = EPOLLOUT;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connect_fd, &active_events[i]);
                    close(connect_fd);
                }
            }
        } // epoll end
    } // while end

    close(listen_fd);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
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


int update_select_maxfd(fd_set fds, int maxfd)
{
    int i;
    int new_maxfd = 0;
    for (i = 0; i <= maxfd; ++i)
    {
        if (FD_ISSET(i, &fds) && i >= new_maxfd)
        {
            new_maxfd = i;
        }
    }
    return new_maxfd;
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

    // select
    struct fd_set read_fdset;
    struct fd_set read_fdset_backup;
    struct fd_set write_fdset;
    struct fd_set write_fdset_backup;
    // struct fd_set except_fdset;
    struct timeval timeout;
    int max_read_fd = listen_fd;
    FD_ZERO(&read_fdset);
    FD_ZERO(&read_fdset_backup);
    FD_ZERO(&write_fdset);
    FD_ZERO(&write_fdset_backup);
    FD_SET(listen_fd, &read_fdset_backup);

    // client_addr and data_buffer
    struct sockaddr_in client_addr;
    int addr_len;
    char recv_data[MAX_LINE*10+1];
    char buffer[MAX_LINE];

    while (1)
    {
        // init select
        read_fdset = read_fdset_backup;
        write_fdset = write_fdset_backup;
        max_read_fd = update_select_maxfd(read_fdset, max_read_fd);
        timeout.tv_sec = SELECT_TIMEOUT; // timeout = 60s
        timeout.tv_usec = 0;
        
        // 4. select
        int select_ret = select(max_read_fd+1, &read_fdset, &write_fdset, NULL, &timeout);
        if (select_ret == -1)
        {
            printf("select error\n");
            return -1;
        }
        else if (select_ret == 0)
        {
            printf("select timeout\n");
            sleep(10);
            continue;
        }
        else
        {
            // client[0] = listen_fd; client[1] = connect_fd1; ...
            // for (int i = 0; i < client_index; ++i)
            // {
            //     if (client[0] == listen_fd)
            //     {
            //         // accept
            //     }
            //     else if (FS_ISSET(client[i], &read_fdset))
            //     {
            //         // recv
            //     }
            //     else if (FS_ISSET(client[i], &write_fdset))
            //     {
            //         // send
            //     }
            // }
            // check every read fd
            for (int i = 0; i <= max_read_fd; ++i)
            {
                if (!FD_ISSET(i, &read_fdset) || !FD_ISSET(i, &read_fdset))
                {
                    continue;
                }
                if (i == listen_fd)
                {
                    // 5.accept
                    int connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);

                    // add connect_fd
                    set_socket_non_block(connect_fd);
                    FD_SET(connect_fd, &read_fdset_backup);
                    // FD_SET(connect_fd, &write_fdset_backup);
                    
                    // update maxfd
                    if (connect_fd > max_read_fd)
                    {
                        max_read_fd = connect_fd;
                    }
                }
                else if (FD_ISSET(i, &read_fdset))
                {
                    // can read from kernel_memery_buffer
                    // can use mutilpthread
                    int recv_len = non_block_recv(i, recv_data, sizeof(recv_data)-1, 0);
                    if (recv_len < 0)
                    {
                        printf("recv data error\n");
                        return -1;
                    }
                    else
                    {
                        printf("recv data: %s", recv_data);
                    }

                    // send data
                    // has better 
                    int send_len = non_block_send(i, recv_data, strlen(recv_data)-1, 0);

                    // destructor
                    FD_CLR(i, &read_fdset_backup);
                    close(i);
                }
                else if (FD_ISSET(i, &write_fdset))
                {
                    // TO DO
                }
            }
            // check every write fd
            /*
            for (int i = 0; i <= max_write_fd; ++i)
            {
                // TO DO
            }
            */
        } // select end
    } // while end

    close(listen_fd);
    return 0;
}

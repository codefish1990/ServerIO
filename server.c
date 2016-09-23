#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

const int MAX_LINE = 1024 * 4;
const int MAX_CONNECT = 1024;
const int WORK_PTHREAD_NUM = 8;

// global param for method3 
pthread_mutex_t g_mutex;
int g_connect_fds[MAX_CONNECT];
int g_current_index = -1;

/* for method2
void* thread_handler(void* arg)
{
    int connect_fd = *(int*)arg;
    
    // process
    char buffer[MAX_LINE];
    int buffer_recv = recv(connect_fd, buffer, sizeof(buffer), 0);
    printf("tid: %d, %s\n", pthread_self(), buffer);
    strncpy(buffer, "Hello World!", 12);
    send(connect_fd, buffer, strlen(buffer), 0);

    close(connect_fd);
    pthread_exit(0);
}
*/

void* thread_handler(void* arg)
{
    int connect_fd = -1;
    char buffer[MAX_LINE];
    int buffer_recv = 0;

    while (1)
    {
        connect_fd = -1;
        if (g_current_index >= 0)
        {
            pthread_mutex_lock(&g_mutex);
            if (g_current_index >= 0)
            {
                connect_fd = g_connect_fds[g_current_index--];
            }
            pthread_mutex_unlock(&g_mutex);
        }

        if (connect_fd == -1)
        {
            continue;
        }

        buffer_recv = recv(connect_fd, buffer, sizeof(buffer), 0);
        printf("tid: %d, %s\n", pthread_self(), buffer);

        strncpy(buffer, "Hello World!", 12);
        send(connect_fd, buffer, strlen(buffer), 0);

        close(connect_fd);
    } //while
}

int main()
{
    pthread_mutex_init(&g_mutex, NULL);
    memset(g_connect_fds, 0, MAX_CONNECT);
    g_current_index = -1;

    pthread_t tid[WORK_PTHREAD_NUM];
    for (int i = 0; i < WORK_PTHREAD_NUM; ++i)
    {
        pthread_create(&tid[i], NULL, thread_handler, NULL);
    }

    // 1. create socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); 

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
    int buffer_len;
    while (1)
    {
        // 5. wait, util has client connect
        int connect_fd = accept(listen_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);
        
        /* method1: fork 
        if (!fork())
        {
            buffer_len = recv(connect_fd, buffer, sizeof(buffer), 0);
            
            // process data
            printf("%s\n", buffer);
            strncpy(buffer, "hello world!", 12);

            send(connect_fd, buffer, strlen(buffer), 0);

            close(connect_fd);
            return 0;
        }
        */

        /* method2: pthread
        pthread_t tid;
        pthread_create(&tid, NULL, thread_handler, (void*)&connect_fd);
        */

        /* method3: pthread pool
         pass
        */
        pthread_mutex_lock(&g_mutex);
        if (++g_current_index < MAX_CONNECT)
        {
            g_connect_fds[g_current_index] = connect_fd;
        }
        pthread_mutex_unlock(&g_mutex);
    }

    close(listen_fd);
    return 0;
}

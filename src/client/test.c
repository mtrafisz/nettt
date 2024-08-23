#include "../nettt/nettt.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

volatile sig_atomic_t running = true;
void sigint_handler(int signum) {
    running = false;
}

// I love nonblocking IO :)
void* send_thread_handle(void* data) {
    ConnectionContext* ctx = (ConnectionContext*)data;

    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (running) {
        fd_set read_fds;
        struct timeval tv;
        int retval;

        // Set up the file descriptor set
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        // Set timeout to 0.5 seconds
        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        // Wait for input on stdin
        retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
            break;
        } 

        // Data is available on stdin
        if (!FD_ISSET(STDIN_FILENO, &read_fds)) continue;

        char buffer[64] = {0};
        if (fgets(buffer, 63, stdin) != NULL) {
            if (send(ctx->sockfd, buffer, strlen(buffer), 0) == -1) {
                perror("send()");
                break;
            }
        } else {
            // Handle fgets error if needed
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("fgets()");
                break;
            }
        }

        // If retval == 0, the select timed out, so we loop back and try again
    }

    return NULL;
}

int main(void) {
    signal(SIGINT, sigint_handler);
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(NETTT_PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    ConnectionContext ctx = {
        .active = true,
        .sockfd = sockfd,
        .user_context = NULL,
        .remote_address = netaddress_from_sockaddrin(serv_addr),
    };

    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_thread_handle, &ctx);

    // wait for message:
    Message msg = {0};
    do {
        if (!message_read(&msg, &ctx)) {
            break;
        }

        if (msg.id == NETTT_MSG_STA) {
            if (strcmp(msg.data, "ALIVE") == 0) continue;
        }

        const char* format = "[%s:%s]\n";
        printf(format, message_type_to_string(msg.id), msg.data);

        if (msg.id == NETTT_MSG_END) break;
    } while (running);

    running = false;
    pthread_join(send_thread, NULL);
    close(sockfd);
    
    return 0;
}

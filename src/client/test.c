#include "../nettt/nettt.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t running = true;
void sigint_handler(int signum) {
    running = false;
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
        .user_context = NULL
    };

    // wait for message:
    Message msg = {0};
    do {
        if (!message_read(&msg, &ctx)) {
            // putchar('.');
            // fflush(stdout);
            // continue;

            fprintf(stderr, "Kill me Baby!\n");
            break;
        }

        const char* format = "Received message [%s:%s]\n";
        printf(format, message_type_to_string(msg.id), msg.data);
        if (msg.id == NETTT_MSG_END) break;
    } while (running);

    close(sockfd);
    return 0;
}

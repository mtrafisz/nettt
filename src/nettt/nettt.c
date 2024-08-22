#include "nettt.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

const char* nettt_msg_type_strings[] = {
    "AOK",
    "STA",
    "ERR",
    "MOV",
    "END",
    "INV"
};
const char* nettt_game_state_strings[] = {
    "WAITING",
    "PLAYING",
    "WON_X",
    "WON_O",
    "DRAW",
    "INVALID"
};
const char  nettt_player_symbols[] = {' ', 'X', 'O', '?'};
const char* nettt_msg_fmt = "%.3s:%s;";

const char* game_state_to_string(GameState state) {
    if (state >= NETTT_STATE_INVALID || state < NETTT_STATE_WAITING) return nettt_game_state_strings[NETTT_STATE_INVALID];

    return nettt_game_state_strings[state];
}

GameState game_state_from_string(const char* str) {
    for (int i = 0; i < NETTT_STATE_INVALID; ++i) {
        if (strcmp(str, nettt_game_state_strings[i]) == 0) {
            return i;
        }
    }

    return NETTT_STATE_INVALID;
}

const char* message_type_to_string(MessageIdentifier id) {
    if (id >= NETTT_MSG_OTHER || id < NETTT_MSG_AOK) return nettt_msg_type_strings[NETTT_MSG_OTHER];

    return nettt_msg_type_strings[id];
}

MessageIdentifier message_type_from_string(const char* str) {
    for (int i = 0; i < NETTT_MSG_OTHER; ++i) {
        if (strcmp(str, nettt_msg_type_strings[i]) == 0) {
            return i;
        }
    }

    return NETTT_MSG_OTHER;
}

bool message_read(Message* msg, int sockfd) {
    struct timeval tval_start, tval_last, tval_result;
    gettimeofday(&tval_start, NULL);

    char buffer[64] = {0};
    ssize_t bytes_read = 0;

    do {
        gettimeofday(&tval_last, NULL);
        
        if ((bytes_read = read(sockfd, buffer, sizeof(buffer))) < 0) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("read");
                return false;
            }
        }

        timersub(&tval_last, &tval_start, &tval_result);
    } while (bytes_read <= 0 && (tval_result.tv_sec * 1000 + tval_result.tv_usec / 1000) < NETTT_TIMEOUT_MS);

    printf("Bytes read: %ld\n", bytes_read);
    printf("elapsed: %ld\n", tval_result.tv_sec * 1000 + tval_result.tv_usec / 1000);

    if (bytes_read < 5) {
        return false;
    }

    buffer[bytes_read] = '\0';

    char* msg_id = strtok(buffer, ":");
    char* msg_data = strtok(NULL, ";");

    if (!msg_id) return false;

    msg->id = message_type_from_string(msg_id);
    const int msg_data_size = NETTT_MESSAGE_SIZE - sizeof(msg->id);
    memset(msg->data, 0, msg_data_size);
    if (msg_data && strlen(msg_data) > 0 && strlen(msg_data) < msg_data_size) strcpy(msg->data, msg_data);

    return true;
}

void handle_sigpipe(int sig) {
    printf("Client disconnected unexpectedly\n");
}

// TODO: also timer / EWOULDBLOCK handling for write? Does it make sense?
bool message_write(Message* msg, int sockfd) {
    char buffer[64] = {0};
    snprintf(buffer, sizeof(buffer), nettt_msg_fmt, message_type_to_string(msg->id), msg->data);

    if (write(sockfd, buffer, strlen(buffer)) < 0) {
        if (errno == EPIPE) {
            // error message from handle_sigpipe
            // why does sigpipe unhandled fucking crash the whole program?
            return false;
        }

        perror("write");
        return false;
    }

    return true;
}

void message_reset(Message* msg) {
    msg->id = NETTT_MSG_ERR;
    // memset(&msg->data, 0, NETTT_MESSAGE_SIZE - sizeof(msg->id)); // doesn't work?
    for (int i = 0; i <  NETTT_MESSAGE_SIZE - sizeof(msg->id); ++i) msg->data[i] = 0;
}

void _acceptor_thrd(void* ctx) {
    ServerContext* server_ctx = (ServerContext*)ctx;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (true) {
        int client_sockfd = accept(server_ctx->sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECANCELED || errno == EINVAL) {  // TODO: Why does SIGINT trigger errno 22 (EINVAL)?
                continue;
            } else {
                perror("accept");
                break;
            }
        }

        int flags = fcntl(client_sockfd, F_GETFL, 0);
        if (flags < 0) {
            perror("fcntl");
            break;
        }

        ConnectionContext conn_ctx = {client_sockfd, server_ctx->user_context};
        pthread_t handler_thread;
        if (pthread_create(&handler_thread, NULL, (void*(*)(void*))server_ctx->handler, &conn_ctx) != 0) {
            perror("pthread_create");
            break;
        }
        pthread_detach(handler_thread);
    }
}

bool server_init(ServerContext* ctx, ConnectionHandler handler, void* user_context) {
#ifdef _WIN32
#error "WINDOWS NOT SUPPORTED FOR NOW"
#endif
    signal(SIGPIPE, handle_sigpipe);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;   
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(NETTT_PORT);

    if (bind(sockfd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind");
        goto err_close_socket;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl");
        goto err_close_socket;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        goto err_close_socket;
    }
    ctx->handler = handler;
    ctx->sockfd = sockfd;
    ctx->user_context = user_context;
    ctx->running = false;

    return true;
err_close_socket:
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return false;
}

bool server_start(ServerContext* ctx) {
    if (listen(ctx->sockfd, NETTT_BACKLOG) < 0) {
        perror("listen");
        return false;
    }

    if (pthread_create(&ctx->acceptor, NULL, (void*(*)(void*))_acceptor_thrd, ctx) != 0) {
        perror("pthread_create");
        return false;
    }

    ctx->running = true;
    return true;
}

void server_stop(ServerContext* ctx) {
    ctx->running = false;
    shutdown(ctx->sockfd, SHUT_RDWR);
    close(ctx->sockfd);
    pthread_cancel(ctx->acceptor);
    printf("Server stopped\n");
}

char player_to_char(Player p) {
    if (p >= NETTT_INVALID_PLAYER || p < NETTT_EMPTY) return nettt_player_symbols[NETTT_INVALID_PLAYER];

    return nettt_player_symbols[p];
}

Player player_from_char(char c) {
    for (int i = 0; i < NETTT_INVALID_PLAYER; ++i) {
        if (c == nettt_player_symbols[i]) {
            return i;
        }
    }

    return NETTT_INVALID_PLAYER;
}

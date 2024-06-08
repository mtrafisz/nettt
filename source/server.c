#include "../include/nettt.h"

#define MAX_GAMES 10

#ifdef _WIN32
#define createGameRoutine(gameIndex) CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gameRoutine, &gameIndex, 0, NULL)
#define gameRoutineJoin(thread) WaitForSingleObject(thread, INFINITE)
#else
#define createGameRoutine(gameIndex) pthread_create(&gameList[gameIndex].thread, NULL, (void* (*)(void*))gameRoutine, &gameIndex)
#define gameRoutineJoin(thread) pthread_join(thread, NULL)
#endif

enum {
    LOG_INFO,
    LOG_ERROR,
};

#define log(level, msg) printf("[%s] %s\n", level == LOG_INFO ? "INFO" : "ERROR", msg);
#define log_a(level, fmt, ...) printf("[%s] " fmt "\n", level == LOG_INFO ? "INFO" : "ERROR", __VA_ARGS__);

typedef struct {
    socket_t sockfdX;
    ipv4addr addrX;
    socket_t sockfdO;
    ipv4addr addrO;

    Player board[3][3];
    GameState state;

    thread_t thread;
} Game;

Game gameList[MAX_GAMES] = {0};

void resetGame(Game* game) {
    game->sockfdX = INVALID_SOCKET;
    game->sockfdO = INVALID_SOCKET;
    memset(&game->addrX, 0, sizeof(ipv4addr));
    memset(&game->addrO, 0, sizeof(ipv4addr));
    memset(game->board, 0, sizeof(game->board));
    game->state = PLAYING;
}

void gameRoutine(int* gameIndex) {
    int idx = *gameIndex;
    Game* game = &gameList[idx];
    
    Message recvMsg;

    NetttSendMessage(game->sockfdX, (Message) {.type = OK, .data = "X"});
    NetttSendMessage(game->sockfdO, (Message) {.type = OK, .data = "O"});

    while (game->state == PLAYING) {
        if (NetttReceiveMessage(game->sockfdX, &recvMsg)) {
            if (recvMsg.type != MOVE) {
                if (recvMsg.type == LEAVE) {
                    NetttSendMessage(game->sockfdO, (Message) {.type = END, .data = "Opponent left"});
                    break;
                }
                NetttSendMessage(game->sockfdX, (Message) {.type = ERR, .data = "Invalid message type"});
            }

            int move = atoi(recvMsg.move);
            if (move < 1 || move > 9) {
                NetttSendMessage(game->sockfdX, (Message) {.type = ERR, .data = "Invalid move"});
            }
        }
    }
}

void addClient(socket_t clientSockfd, ipv4addr clientAddr) {
    int gameIndex = -1;
    char player = NONE;
    
    for (int i = 0; i < MAX_GAMES; i++) {
        if (gameList[i].sockfdX == INVALID_SOCKET) {
            gameIndex = i;
            player = X;
            break;
        }
        if (gameList[i].sockfdO == INVALID_SOCKET) {
            gameIndex = i;
            player = O;
            break;
        }
    }

    if (gameIndex == -1) {
        log(LOG_ERROR, "No available games");
        return;
    }

    Game* game = &gameList[gameIndex];
    if (player == X) {
        game->sockfdX = clientSockfd;
        game->addrX = clientAddr;
    } else {
        game->sockfdO = clientSockfd;
        game->addrO = clientAddr;
    }

    log_a(LOG_INFO, "Player %c joined game %d", player, gameIndex);

    if (game->sockfdO != INVALID_SOCKET && game->sockfdX != INVALID_SOCKET) {
        log_a(LOG_INFO, "Game %d started", gameIndex);
        game->thread = createGameRoutine(gameIndex);
    }

    return;
}

int main(void) {
    int retCode = 0;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log(LOG_ERROR, "WSAStartup failed");
        return 1;
    }
#endif
    socket_t sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        log(LOG_ERROR, "Failed to create socket");
        return 1;
    }

    ipv4addr localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(NETTT_PORT);

    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        log(LOG_ERROR, "Failed to bind socket");
        retCode = 1;
        goto cleanup_destroy_socket;
    }

    if (listen(sockfd, MAX_GAMES) == SOCKET_ERROR) {
        log(LOG_ERROR, "Failed to listen on socket");
        retCode = 1;
        goto cleanup_destroy_socket;
    }

    for (int i = 0; i < MAX_GAMES; i++) resetGame(&gameList[i]);
    log_a(LOG_INFO, "Server started on port %d", NETTT_PORT);

    for (;;) {
        ipv4addr clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        socket_t clientSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSockfd == INVALID_SOCKET) {
            log(LOG_ERROR, "Failed to accept client");
            continue;
        }

        addClient(clientSockfd, clientAddr);
    }

cleanup_destroy_socket:
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return retCode;
}

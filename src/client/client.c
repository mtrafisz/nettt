#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <unistd.h>
#include "../nettt/nettt.h"
#include <raylib/src/raylib.h>
#include <pthread.h>

volatile sig_atomic_t running;
void sigint_handle(int sig) {
    running = false;
}

typedef struct _game {
    Player board[3][3];
    Player local_player;
    bool my_turn;
    GameState state;
    ConnectionContext conn_ctx;
} Game;

int init_game(Game* game) {
    assert(game);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            game->board[j][i] = NETTT_EMPTY; 
        }
    }

    game->my_turn = false;
    game->state = NETTT_STATE_WAITING;
    game->local_player = NETTT_INVALID_PLAYER;

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        log_errno_msg("Failed to create socket");
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serv_addr.sin_port = htons(NETTT_PORT);
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        log_errno_msg("Couldn't connect to server");
        close(sockfd);
        return 1;
    }

    ConnectionContext conn_ctx = {
        .active = true,     // used only in server
        .sockfd = sockfd,
        // .user_context = &game,
        .user_context = NULL,
        .remote_address = netaddress_from_sockaddrin(serv_addr),
    };

    game->conn_ctx = conn_ctx;

    return 0;
}

void* net_thread_handle(void* ctx) {
    Game* game = (Game*)ctx;

    Message msg = {0};
    running = true;

    while (running) {
        if (!message_read(&msg, &game->conn_ctx)) {
            log_message(LEVEL_FATAL, "Timeout or error while reading message");
            running = false;
            break;
        }

        TraceLog(LOG_INFO, "received message: %s:%s", message_type_to_string(msg.id), msg.data);

        switch (msg.id) {
        case NETTT_MSG_STA: {
            if (strcmp(msg.data, "WAITING") == 0) continue;
            if (strcmp(msg.data, "ALIVE") == 0) {
                if (game->state == NETTT_STATE_WAITING) game->state = NETTT_STATE_PLAYING;
                continue;
            }
            game->state = game_state_from_string(msg.data);
        } break;
        case NETTT_MSG_ERR: {
            log_message(LEVEL_ERROR, "Error: %s", msg.data);
        } break;
        case NETTT_MSG_END: running = false; break;
        case NETTT_MSG_MOV: {
            int idx = strtol(msg.data, NULL, 10);
            int idx_x = idx % 3;
            int idx_y = idx / 3;
            game->board[idx_y][idx_x] = game->local_player == NETTT_X ? NETTT_O : NETTT_X;
        } break;
        case NETTT_MSG_AOK: {
            game->local_player = player_from_char(msg.data[0]);
            if (game->local_player == NETTT_X) game->my_turn = true;
        } break;
        default:
            log_message(LEVEL_FATAL, "Received invalid message");
            running = false;
        }
    }
}

int main(void) {
    signal(SIGINT, sigint_handle);
    logger_init(stdout);
    
    Game game = {0};
    if (init_game(&game) != 0) return 1;

    pthread_t network_thread;
    pthread_create(&network_thread, NULL, net_thread_handle, &game);

    InitWindow(300, 300, "Super Ultra Hyper Miracle Romantic Window");
    SetTargetFPS(60);

    char text[64] = {0};

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            sprintf(text, "%s", game_state_to_string(game.state));
            DrawText(text, 50, 50, 30, BLACK);
        EndDrawing();
    }

    running = false;        // in case I forget about it in loop
    game.conn_ctx.active = false;

    shutdown(game.conn_ctx.sockfd, SHUT_RDWR);
    close(game.conn_ctx.sockfd);
    CloseWindow();

    return 0;
}

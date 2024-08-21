#include "../nettt/nettt.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define NETTT_MAX_GAMES NETTT_BACKLOG

volatile sig_atomic_t running = 1;
void sigint_handler(int signum) {
    running = 0;
}

typedef struct _game {
    GameState state;
    Player board[3][3];
    int current_player;
    ConnectionContext players[2];
} Game;

bool game_init(Game* game) {
    game->state = NETTT_WAITING;
    game->current_player = NETTT_X;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            game->board[i][j] = NETTT_EMPTY;
        }
    }

    return true;
}

typedef struct _game_ctx {
    Game games[NETTT_MAX_GAMES];
    int num_games;
} GameContext;

void game_ctx_init(GameContext* ctx) {
    ctx->num_games = 0;
    for (int i = 0; i < NETTT_MAX_GAMES; ++i) {
        game_init(&ctx->games[i]);
    }
}

void* game_thread_handler(void* ctx) {
    Game* game = (Game*)ctx;

    printf("Game thread started\n");
    printf("Game state: %s\n", game_state_to_string(game->state));

    int sockX = game->players[0].sockfd;
    int sockO = game->players[1].sockfd;

    Message msg = (Message) {
        .id = NETTT_AOK,
        .data = {0}
    };

    msg.data[0] = 'X';
    message_write(&msg, sockX);

    msg.data[0] = 'O';
    message_write(&msg, sockO);

    

    shutdown(sockX, SHUT_RDWR);
    close(sockX);
    shutdown(sockO, SHUT_RDWR);
    close(sockO);

    game_init(game);
}

// make sure client won't timeout & check if client is still listening - 2 in 1
void* wait_thread_handler(void* ctx) {
    Game* game = (Game*)ctx;
    const char* state = game_state_to_string(game->state);

    Message msg = (Message) {
        .id = NETTT_STA,
    };
    strcpy(msg.data, state);

    while (game->state == NETTT_WAITING) {
        if (!message_write(&msg, game->players[0].sockfd)) {
            // client left - reset game and leave.
            game_init(game);
            break;
        }

        sleep((NETTT_TIMEOUT_MS / 2) / 1000);
    }
}

void initial_connection_handler(void* conn_ctx) {
    printf("New connection\n");

    ConnectionContext* ctx = (ConnectionContext*)conn_ctx;
    GameContext* game_ctx = (GameContext*)ctx->user_context;
    
    Message msg = {0};

    int gid = -1;

    for (int i = 0; i < NETTT_MAX_GAMES; ++i) {
        if (game_ctx->games[i].state == NETTT_WAITING) {
            gid = i;
            break;
        } else {
            printf("Game %d is in state %s\n", i, game_state_to_string(game_ctx->games[i].state));
        }
    }

    if (gid == -1) {
        fprintf(stderr, "No available games\n");
        msg.id = NETTT_ERR;
        strncpy(msg.data, "No available games", 18);

        goto send;
    }

    int pid = game_ctx->games[gid].current_player;
    game_ctx->games[gid].players[pid - 1] = *ctx;

    msg.id = NETTT_AOK;
    if (pid == NETTT_X) {
        game_ctx->games[gid].current_player = NETTT_O;  // as in "next player will be"

        pthread_t wait_thread;
        pthread_create(&wait_thread, NULL, wait_thread_handler, &game_ctx->games[gid]);
        pthread_detach(wait_thread);
    } else if (pid == NETTT_O) {
        game_ctx->games[gid].current_player = NETTT_X;  // as in "reset hacky solution and use as intended"

        game_ctx->games[gid].state = NETTT_PLAYING;
        pthread_t game_thread;
        pthread_create(&game_thread, NULL, game_thread_handler, &game_ctx->games[gid]);
        pthread_detach(game_thread);
    } else {
        printf("Invalid player id '%c' (%d)\n", player_to_char(pid), pid);
        msg.id = NETTT_ERR;
    }

    printf("Player %c joined game %d\n", player_to_char(pid), gid);

send:
    if (!message_write(&msg, ctx->sockfd)) {
        fprintf(stderr, "Failed to send message\n");
    }
    return;
fatal_err:
    close(ctx->sockfd);
    close(game_ctx->games[gid].players[0].sockfd);
    close(game_ctx->games[gid].players[1].sockfd);
    return;
}

int main(void) {
    signal(SIGINT, sigint_handler);
    ServerContext ctx = {0};
    GameContext game_ctx = {0};

    game_ctx_init(&game_ctx);

    if (!server_init(&ctx, initial_connection_handler, &game_ctx)) {
        fprintf(stderr, "Failed to initialize server context\n");
        return 1;
    }

    if (!server_start(&ctx)) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    while (running) {
        sleep(1);
    }
    server_stop(&ctx);

    return 0;
}

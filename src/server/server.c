#include "../nettt/nettt.h"
#include <logger/logger.h>

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
    int idx;
} Game;

bool game_init(Game* game) {
    game->state = NETTT_STATE_WAITING;
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

void* alive_thread_handler(void* ctx) {
    Game* game = (Game*)ctx;
    Message msg = (Message) {
        .id = NETTT_MSG_STA,
    };
    strcpy(msg.data, "ALIVE");

    while (game->state == NETTT_STATE_PLAYING) {
        if (!message_write(&msg, &game->players[0]) || !message_write(&msg, &game->players[1])) {
            game->state = NETTT_STATE_INVALID;
            // ^ Should break game_thread loop and perform cleanup.
            log_message(LOG_TRACE, "write failed in %s for game [%d] - ending game", __FUNCTION__, game->idx);
            log_message(LOG_TRACE, "connection status: x - %d, o - %d", game->players[0].active, game->players[1].active);
        }
        sleep(NETTT_TIMEOUT_MS / 2000);
    }

    return NULL;
}

void* game_thread_handler(void* ctx) {
    Game* game = (Game*)ctx;

    game->state = NETTT_STATE_PLAYING;
    sleep(NETTT_TIMEOUT_MS / 2000);     // wait for wait-thread-handle to exit

    log_message(LOG_INFO, "game thread [%d] started", game->idx);

    pthread_t keep_alive_thread;
    pthread_create(&keep_alive_thread, NULL, alive_thread_handler, game);

    int sockX = game->players[0].sockfd;
    int sockO = game->players[1].sockfd;

    Message msg = (Message) {
        .id = NETTT_MSG_AOK,
        .data = {0}
    };

    msg.data[0] = 'X';
    message_write(&msg, &game->players[0]);

    msg.data[0] = 'O';
    message_write(&msg, &game->players[1]);

    // GAME LOOP

    while (game->state == NETTT_STATE_PLAYING) {
        /* GAMING B-) */
        sleep(1);
    }

    // GAME END

    game->state = NETTT_STATE_WAITING;      // kill keep-alive thread before closing connections to avoid EPIPE or SIGPIPE
    pthread_join(keep_alive_thread, NULL);

    message_reset(&msg);
    msg.id = NETTT_MSG_END;

    log_message(LOG_TRACE, "game [%d]: loop broken", game->idx);
    log_message(LOG_TRACE, "connection status: x - %d, o - %d", game->players[0].active, game->players[1].active);

    if (game->players[0].active) message_write(&msg, &game->players[0]);
    if (game->players[1].active) message_write(&msg, &game->players[1]);

    shutdown(sockX, SHUT_RDWR);
    close(sockX);
    shutdown(sockO, SHUT_RDWR);
    close(sockO);

    log_message(LOG_INFO, "game thread [%d] ending", game->idx);
    game_init(game);
}

// make sure client won't timeout & check if client is still listening - 2 in 1
void* wait_thread_handler(void* ctx) {
    sleep(NETTT_TIMEOUT_MS / 2000);     // let main thread finish everything related to this client; 

    Game* game = (Game*)ctx;
    const char* state = game_state_to_string(game->state);

    Message msg = (Message) {
        .id = NETTT_MSG_STA,
    };
    strcpy(msg.data, state);

    while (game->state == NETTT_STATE_WAITING) {
        if (!message_write(&msg, &game->players[0])) {
            // client left - reset game and leave.
            game_init(game);
            break;
        }

        sleep(NETTT_TIMEOUT_MS / 2000);
    }
}

void initial_connection_handler(void* conn_ctx) {
    ConnectionContext* ctx = (ConnectionContext*)conn_ctx;
    GameContext* game_ctx = (GameContext*)ctx->user_context;
    
    Message msg = {0};

    int gid = -1;

    for (int i = 0; i < NETTT_MAX_GAMES; ++i) {
        if (game_ctx->games[i].state != NETTT_STATE_WAITING) continue;

        gid = i;
        break;
    }

    if (gid == -1) {
        log_message(LOG_ERROR, "client tried to connect, but no games are avaliable");
        msg.id = NETTT_MSG_ERR;
        strncpy(msg.data, "No available games", 18);

        goto send;
    }

    int pid = game_ctx->games[gid].current_player;
    game_ctx->games[gid].players[pid - 1] = *ctx;

    msg.id = NETTT_MSG_AOK;

    if (pid == NETTT_X) {
        game_ctx->games[gid].current_player = NETTT_O;  // as in "next player will be"

        pthread_t wait_thread;
        pthread_create(&wait_thread, NULL, wait_thread_handler, &game_ctx->games[gid]);
        pthread_detach(wait_thread);
    } else if (pid == NETTT_O) {
        game_ctx->games[gid].current_player = NETTT_X;  // as in "reset hacky solution and use as intended"

        game_ctx->games[gid].state = NETTT_STATE_PLAYING;
        game_ctx->games[gid].idx = gid;

        pthread_t game_thread;
        pthread_create(&game_thread, NULL, game_thread_handler, &game_ctx->games[gid]);
        pthread_detach(game_thread);
    } else {
        log_message(LOG_ERROR, "Invalid player id: %d", pid);
        msg.id = NETTT_MSG_ERR;
    }

    log_message(LOG_INFO, "%s joined game %d as %c", netaddress_to_str(ctx->remote_address), gid, player_to_char(pid));

send:
    if (!message_write(&msg, ctx)) {
        log_message(LOG_ERROR, "Failed to send message");
    }

    return;
fatal_err:
    close(ctx->sockfd);
    close(game_ctx->games[gid].players[0].sockfd);
    close(game_ctx->games[gid].players[1].sockfd);
    return;
}

int main(void) {
    logger_init(stdout);
    set_log_append_dt(false);
    set_log_level(LOG_TRACE);

    signal(SIGINT, sigint_handler);
    ServerContext ctx = {0};
    GameContext game_ctx = {0};

    game_ctx_init(&game_ctx);

    if (!server_init(&ctx, initial_connection_handler, &game_ctx)) {
        log_message(LOG_FATAL, "Failed to initialize server context");
        return 1;
    }

    if (!server_start(&ctx)) {
        log_message(LOG_FATAL, "Failed to start server");
        return 1;
    }

    while (running) {
        sleep(1);
    }
    server_stop(&ctx);

    return 0;
}

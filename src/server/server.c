#include "../nettt/nettt.h"
#include <logger/logger.h>

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>

#define NETTT_MAX_GAMES NETTT_BACKLOG

volatile sig_atomic_t running = 1;
void sigint_handler(int signum) {
    puts("");
    log_message(LOG_INFO, "SIGINT received, ending games and shutting down...");
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

        if (i < 2) {
            game->players[i].active = false;
            game->players[i].remote_address.ip = 0;
            game->players[i].remote_address.port = 0;
            game->players[i].sockfd = -1;
            game->players[i].user_context = NULL;
        }
    }

    return true;
}

typedef struct _game_ctx {
    Game games[NETTT_MAX_GAMES];
} GameContext;

void game_ctx_init(GameContext* ctx) {
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

GameState game_update_state(Game* game) {
    bool draw = true;

    for (Player p = NETTT_X; p <= NETTT_O; ++p) {
        for (int i = 0; i < 3; ++i) {
            if (game->board[i][0] == p && game->board[i][1] == p && game->board[i][2] == p) return NETTT_STATE_WON_X + p - 1;  
            if (game->board[0][i] == p && game->board[1][i] == p && game->board[2][i] == p) return NETTT_STATE_WON_X + p - 1;  
        }

        if (game->board[0][0] == p && game->board[1][1] == p && game->board[2][2] == p) return NETTT_STATE_WON_X + p - 1;
        if (game->board[0][2] == p && game->board[1][1] == p && game->board[2][0] == p) return NETTT_STATE_WON_X + p - 1;
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (game->board[j][i] == NETTT_EMPTY) draw = false;
        }
    }

    if (draw) return NETTT_STATE_DRAW;
    return NETTT_STATE_PLAYING;
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
        if (!message_read(&msg, &game->players[game->current_player - 1])) {
            break;
        }

        switch (msg.id) {
        case NETTT_MSG_STA:
            continue;
        case NETTT_MSG_END:
        case NETTT_MSG_ERR:
            goto game_end;
        case NETTT_MSG_MOV: {
            if (strlen(msg.data) != 1) { /* error */ assert(false); }
            int idx = strtol(msg.data, NULL, 10);
            log_message(LOG_TRACE, "requested move for index %d", idx);
            if (idx < 0 || idx > 8) { /* error */ assert(false); }
            int idx_x = idx % 3;
            int idx_y = idx / 3;
            if (game->board[idx_y][idx_x] != NETTT_EMPTY) { /* error */ assert(false); }
            
            message_write(&msg, game->current_player == NETTT_X ? &game->players[1] : &game->players[0]);
            
            game->board[idx_y][idx_x] = game->current_player;
            log_message(LOG_TRACE, "%dx%d set to %c", idx_y, idx_x, player_to_char(game->current_player));
            GameState new_state = game_update_state(game);

            message_reset(&msg);
            msg.id = NETTT_MSG_STA;
            sprintf(msg.data, "%s", game_state_to_string(new_state));

            message_write(&msg, &game->players[0]);
            message_write(&msg, &game->players[1]);
            
            if (new_state != NETTT_STATE_PLAYING) goto game_end;

            game->current_player = game->current_player == NETTT_X ? NETTT_O : NETTT_X;
        } break;
        default:
            log_message(LOG_TRACE, "id: %d, data: %s", msg.id, msg.data);
            message_reset(&msg);
            msg.id = NETTT_MSG_ERR;
            sprintf(msg.data, "UNEXPECTED MESSAGE");
            message_write(&msg, &game->players[game->current_player - 1]);
        }

        usleep(NETTT_PROBE_TIMEOUT_MS * 1000);
    }
game_end: // for "break" out of switch
    // GAME END

    game->state = NETTT_STATE_WAITING;      // kill keep-alive thread before closing connections to avoid redundant EPIPEs and SIGPIPEs
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

    // search for empty game

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

    // add player to game

    int pid = game_ctx->games[gid].current_player;
    game_ctx->games[gid].players[pid - 1] = *ctx;

    msg.id = NETTT_MSG_AOK;

    if (pid == NETTT_X) {
        game_ctx->games[gid].current_player = NETTT_O;  // as in "next player will be"

        // run wait thread

        pthread_t wait_thread;
        pthread_create(&wait_thread, NULL, wait_thread_handler, &game_ctx->games[gid]);
        pthread_detach(wait_thread);
    } else if (pid == NETTT_O) {
        game_ctx->games[gid].current_player = NETTT_X;  // as in "reset hacky solution and use as intended"

        game_ctx->games[gid].state = NETTT_STATE_PLAYING;
        game_ctx->games[gid].idx = gid;

        // start game

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
    GameContext game_ctx = {0}; // passed to threads via ctx->user_ctx void*

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

    // kill game threads
    for (int i = 0; i < NETTT_MAX_GAMES; ++i) {
        game_ctx.games[i].state = NETTT_STATE_INVALID;
    }
    usleep(NETTT_TIMEOUT_MS * 1000);

    server_stop(&ctx);

    return 0;
}

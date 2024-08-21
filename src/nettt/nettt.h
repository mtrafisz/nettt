#ifndef _NETTT_H_
#define _NETTT_H_

#ifdef _WIN32
#warning Get off of Your gaming OS, and run linux like a normal person
#error Windows is not supported yet
#endif

#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define NETTT_PORT              (6666)
#define NETTT_BACKLOG           (100)
#define NETTT_TIMEOUT_MS        (3 * 1000)

typedef enum _nettt_game_state {
    NETTT_WAITING,
    NETTT_PLAYING,
    NETTT_WON_X,
    NETTT_WON_O,
    NETTT_DRAW,

    NETTT_INVALID
} GameState;

const char* game_state_to_string(GameState state);
GameState game_state_from_string(const char* str);

typedef enum _nettt_msg_type {
    NETTT_AOK,
    NETTT_STA,
    NETTT_ERR,
    NETTT_MOV,
    NETTT_END,

    NETTT_NONE
} MessageIdentifier;

const char* message_type_to_string(MessageIdentifier id);
MessageIdentifier message_type_from_string(const char* str);

typedef struct _nettt_msg {
    MessageIdentifier id;
    char data[27];
} Message;

bool message_read(Message* msg, int sockfd);
bool message_write(Message* msg, int sockfd);

typedef struct _conn_ctx {
    int sockfd;
    void* user_context;
} ConnectionContext;

typedef void (*ConnectionHandler)(void* conn_ctx);

typedef struct {
    int sockfd;
    ConnectionHandler handler;
    void* user_context;
    pthread_t acceptor;
    bool running;
} ServerContext;

bool server_init(ServerContext* ctx, ConnectionHandler handler, void* user_context);
bool server_start(ServerContext* ctx);
void server_stop(ServerContext* ctx);

typedef enum _nettt_player {
    NETTT_EMPTY,
    NETTT_X,
    NETTT_O,

    NETTT_INVALID_PLAYER
} Player;

char player_to_char(Player p);
Player player_from_char(char c);

#endif

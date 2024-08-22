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
#define NETTT_MESSAGE_SIZE      (32)

typedef enum _nettt_game_state {
    NETTT_STATE_WAITING,
    NETTT_STATE_PLAYING,
    NETTT_STATE_WON_X,
    NETTT_STATE_WON_O,
    NETTT_STATE_DRAW,

    NETTT_STATE_INVALID
} GameState;

const char* game_state_to_string(GameState state);
GameState game_state_from_string(const char* str);

typedef enum _nettt_msg_type {
    NETTT_MSG_AOK,
    NETTT_MSG_STA,
    NETTT_MSG_ERR,
    NETTT_MSG_MOV,
    NETTT_MSG_END,

    NETTT_MSG_OTHER
} MessageIdentifier;

const char* message_type_to_string(MessageIdentifier id);
MessageIdentifier message_type_from_string(const char* str);

typedef struct _nettt_msg {
    MessageIdentifier id;
    char data[NETTT_MESSAGE_SIZE - sizeof(MessageIdentifier)];  // fancy ... ?
} Message;

bool message_read(Message* msg, int sockfd);
bool message_write(Message* msg, int sockfd);
void message_reset(Message* msg);

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

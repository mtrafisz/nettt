#ifndef NET_TTT_H
#define NET_TTT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

typedef SOCKET socket_t ;
typedef HANDLE thread_t;

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#define INVALID_SOCKET -1

typedef int socket_t;
typedef pthread_t thread_t;

#endif // OS

#define NETTT_PORT 6969
#define NETTT_MSG_MAX_LEN 64
#define SEPARATOR ":"

typedef struct sockaddr_in ipv4addr;

typedef enum _e_player {
    NONE = 0,
    X = 'X',
    O = 'O'
} Player;

typedef enum _e_game_state {
    PLAYING,
    DRAW,
    X_WON = 'X',
    O_WON = 'O',
} GameState;

typedef enum _e_message_type {
    LEAVE,
    OK,
    ERR,
    MOVE,
    STATE,
    END
} MessageType;

typedef struct _s_message {
    MessageType type;
    union {
        char player_str[2];
        char state[16];   // magic number, but the states are somewhat set in stone, and the longest one is 8 bytes
        char move[2];     // 1-9
        char reason[32];  // error message
        char data[NETTT_MSG_MAX_LEN];
    };
} Message;

int NetttSendMessage(socket_t sockfd, Message msg);
int NetttRecvMessage(socket_t sockfd, Message *msg);
const char* game_state_to_string(GameState state);
GameState string_to_game_state(const char* str);

#endif // NET_TTT_H
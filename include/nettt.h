#ifndef NET_TTT_H
#define NET_TTT_H

// #include "elosno.c"

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <string.h>

#define TTT_PORT 6969
#define TTT_MSG_LEN 17

typedef enum
{
    NONE,
    X,
    O
} Player;

typedef enum
{
    PLAYING,
    X_WON,
    O_WON,
    DRAW
} GameState;

typedef enum
{
    LEAVE, // client -> server: msg with no data
    OK,    // server -> client: "OK"
    ERR,   // server -> client: "ERR"
    MOVE,  // client -> server: "MOVE.<field>"                                  ex: "MOVE.1" -> set field 1 to player
    // unused:
    // GET_BOARD, // client -> server: "GET_BOARD"
    // BOARD,     // server -> client: "BOARD.row[0,0]row[0,1]row[0,2]row[1,0]..."    ex: "BOARD.XXONNONNN"
    GET_STATE, // client -> server: "GET_STATE"
    STATE,     // server -> client: "STATE.state"                                  ex: "STATE.X_WON"
    END        // server -> client: "END.reason"                                   ex: "END.DISCONNECTED"
} MessageType; // so: "COMMAND.DATA" -> max 17 bytes

void send_message(SOCKET sock, MessageType type, char *data);
void recv_message(SOCKET sock, MessageType *type, char *data);

#ifdef NET_TTT_IMPLEMENTATION

static char *message_type_to_string(MessageType type)
{
    switch (type)
    {
    case LEAVE:
        return "LEAVE";
    case OK:
        return "OK";
    case ERR:
        return "ERR";
    case MOVE:
        return "MOVE";
    // case GET_BOARD:
    //    return "GET_BOARD";
    // case BOARD:
    //    return "BOARD";
    case GET_STATE:
        return "GET_STATE";
    case STATE:
        return "STATE";
    default:
        return "";
    }
}

static MessageType string_to_message_type(char *str)
{
    if (strcmp(str, "OK") == 0)
        return OK;
    else if (strcmp(str, "ERR") == 0)
        return ERR;
    else if (strcmp(str, "MOVE") == 0)
        return MOVE;
    // else if (strcmp(str, "GET_BOARD") == 0)
    // return GET_BOARD;
    // else if (strcmp(str, "BOARD") == 0)
    // return BOARD;
    else if (strcmp(str, "GET_STATE") == 0)
        return GET_STATE;
    else if (strcmp(str, "STATE") == 0)
        return STATE;
    else if (strcmp(str, "END") == 0)
        return END;
    else if (strcmp(str, "LEAVE") == 0)
        return LEAVE;
    else
        return -1;
}

static GameState string_to_game_state(char *str)
{
    if (strcmp(str, "PLAYING") == 0)
        return PLAYING;
    else if (strcmp(str, "X_WON") == 0)
        return X_WON;
    else if (strcmp(str, "O_WON") == 0)
        return O_WON;
    else if (strcmp(str, "DRAW") == 0)
        return DRAW;
    else
        return -1;
}

static char *game_state_to_string(GameState state)
{
    switch (state)
    {
    case PLAYING:
        return "PLAYING";
    case X_WON:
        return "X_WON";
    case O_WON:
        return "O_WON";
    case DRAW:
        return "DRAW";
    default:
        return "";
    }
}

void send_message(SOCKET sock, MessageType type, char *data)
{
    char msg[TTT_MSG_LEN];
    if (type != LEAVE)
        sprintf(msg, "%s.%s", message_type_to_string(type), data);
    else
        sprintf(msg, "%s", message_type_to_string(type));
    send(sock, msg, TTT_MSG_LEN, 0);
}

void recv_message(SOCKET sock, MessageType *type, char *data)
{
    char msg[TTT_MSG_LEN];
    int n = recv(sock, msg, TTT_MSG_LEN, 0);

    if (n == 0 || strcmp(msg, "LEAVE") == 0)
    {
        *type = LEAVE;
        return;
    }

    char *token = strtok(msg, ".");
    *type = string_to_message_type(token);
    token = strtok(NULL, ".");
    strcpy(data, token);
}

static char player_to_char(Player player)
{
    switch (player)
    {
    case X:
        return 'X';
    case O:
        return 'O';
    default:
        return ' ';
    }
}

#endif

#endif

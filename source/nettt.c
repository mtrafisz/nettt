#include "../include/nettt.h"

const char* message_types[] = {
    "LEAVE",
    "OK",
    "ERR",
    "MOVE",
    "STATE",
    "END"
};

const char* game_states[] = {
    "PLAYING",
    "X_WON",
    "O_WON",
    "DRAW"
};

/* Conversion utilities */

const char* message_type_to_string(MessageType type) {
    if (type < 0 || type >= sizeof(message_types) / sizeof(message_types[0])) 
        return "INVALID";
    
    return message_types[type];
}

MessageType string_to_message_type(const char* str) {
    for (int i = 0; i < sizeof(message_types) / sizeof(message_types[0]); i++) {
        if (strcmp(str, message_types[i]) == 0) 
            return i;
    }

    return -1;
}

const char* game_state_to_string(GameState state) {
    if (state < 0 || state >= sizeof(game_states) / sizeof(game_states[0])) 
        return "INVALID";
    
    return game_states[state];
}

GameState string_to_game_state(const char* str) {
    for (int i = 0; i < sizeof(game_states) / sizeof(game_states[0]); i++) {
        if (strcmp(str, game_states[i]) == 0) 
            return i;
    }

    return -1;
}

int NetttSendMessage(socket_t sockfd, Message msg) {
    const char* type_str = message_type_to_string(msg.type);
    const char* msg_str = msg.data;
    if (strlen(msg_str) + strlen(type_str) + 2 > NETTT_MSG_MAX_LEN) {
        fprintf(stderr, "Message too long\n");
        return 0;
    }

    char buffer[NETTT_MSG_MAX_LEN];
    sprintf(buffer, "%s:%s", type_str, msg_str);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("send");
        return 0;
    }

    return 1;
}

int NetttRecvMessage(socket_t sockfd, Message *msg) {
    char buffer[NETTT_MSG_MAX_LEN];
    int n = recv(sockfd, buffer, NETTT_MSG_MAX_LEN, 0);
    if (n < 0) {
        perror("recv");
        return 0;
    }

    buffer[n] = '\0';
    char* type_str = strtok(buffer, ":");
    char* data = strtok(NULL, ":");
    if (type_str == NULL || data == NULL) {
        fprintf(stderr, "Invalid message\n");
        return 0;
    }

    msg->type = string_to_message_type(type_str);
    strcpy(msg->data, data);

    return 1;
}

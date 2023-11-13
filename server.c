#define NET_TTT_IMPLEMENTATION
#include "nettt.h"

#define log(msg) printf("[INFO] " msg "\n")
#define log_a(fmt, ...) printf("[INFO] " fmt "\n", __VA_ARGS__)

typedef struct
{
    SOCKET cSocketX;
    SOCKADDR_IN cAddrX;
    SOCKET cSocketO;
    SOCKADDR_IN cAddrO;

    Player board[3][3];
    GameState state;

    HANDLE hThread;
} Game;

Game gameList[10] = {0};

GameState check_game_state(Player board[3][3])
{
    for (int i = 0; i < 3; i++)
    {
        if (board[i][0] != NONE && board[i][0] == board[i][1] && board[i][1] == board[i][2])
        {
            if (board[i][0] == X)
                return X_WON;
            else
                return O_WON;
        }

        if (board[0][i] != NONE && board[0][i] == board[1][i] && board[1][i] == board[2][i])
        {
            if (board[0][i] == X)
                return X_WON;
            else
                return O_WON;
        }
    }

    if (board[0][0] != NONE && board[0][0] == board[1][1] && board[1][1] == board[2][2])
    {
        if (board[0][0] == X)
            return X_WON;
        else
            return O_WON;
    }

    if (board[0][2] != NONE && board[0][2] == board[1][1] && board[1][1] == board[2][0])
    {
        if (board[0][2] == X)
            return X_WON;
        else
            return O_WON;
    }

    for (int i = 0; i < 9; i++)
    {
        if (board[i / 3][i % 3] == NONE)
            return PLAYING;
    }

    return DRAW;
}

void reset_game(Game *game)
{
    game->cSocketX = INVALID_SOCKET;
    game->cSocketO = INVALID_SOCKET;
    game->state = PLAYING;
    game->hThread = NULL;

    for (int i = 0; i < 9; i++)
    {
        game->board[i / 3][i % 3] = NONE;
    }
}

void game_thread(int *game_id)
{
    int id = *game_id;
    char msg[TTT_MSG_LEN];
    MessageType type;

    // start the game
    send_message(gameList[id].cSocketX, OK, "X");
    send_message(gameList[id].cSocketO, OK, "O");

    log_a("Game %d started", id);

    while (gameList[id].state == PLAYING)
    {
        // X move
        recv_message(gameList[id].cSocketX, &type, msg);
        if (type != MOVE)
        {
            if (type == LEAVE)
            {
                log_a("Player X left the game in game %d", id);
                gameList[id].state = DRAW;
            }
            else
            {
                printf("Invalid message from X: %s. Client bug, or message corrupted. Aborting the game.\n", message_type_to_string(type));
                gameList[id].state = DRAW;
            }

            break;
        }

        int move = atoi(msg) - 1;

        // invalid moves somehow
        if (move < 0 || move > 8 || gameList[id].board[move / 3][move % 3] != NONE)
        {
            printf("Invalid move from X, client failed to verify move or messge corrupted. Aborting the game.\n");
            gameList[id].state = DRAW;
            break;
        }

        // move is ok - update board, check state and send messages
        gameList[id].board[move / 3][move % 3] = X;
        send_message(gameList[id].cSocketX, OK, "X");

        gameList[id].state = check_game_state(gameList[id].board);
        send_message(gameList[id].cSocketX, STATE, game_state_to_string(gameList[id].state));
        send_message(gameList[id].cSocketO, STATE, game_state_to_string(gameList[id].state));

        Sleep(100); // so the client has time to read the message
        send_message(gameList[id].cSocketO, MOVE, msg);

        if (gameList[id].state != PLAYING)
            break;

        // O move
        recv_message(gameList[id].cSocketO, &type, msg);
        if (type != MOVE)
        {
            if (type == LEAVE)
            {
                log_a("Player O left the game in game %d", id);
                gameList[id].state = DRAW;
            }
            else
            {
                printf("Invalid message from O: %s. Client bug, or message corrupted. Aborting the game.\n", message_type_to_string(type));
                gameList[id].state = DRAW;
            }
            break;
        }

        move = atoi(msg) - 1;

        // invalid moves somehow
        if (move < 0 || move > 8 || gameList[id].board[move / 3][move % 3] != NONE)
        {
            printf("Invalid move from O, client failed to verify move or messge corrupted. Aborting the game.\n");
            gameList[id].state = DRAW;
            break;
        }

        gameList[id].board[move / 3][move % 3] = O;
        send_message(gameList[id].cSocketO, OK, "O");

        gameList[id].state = check_game_state(gameList[id].board);
        send_message(gameList[id].cSocketX, STATE, game_state_to_string(gameList[id].state));
        send_message(gameList[id].cSocketO, STATE, game_state_to_string(gameList[id].state));

        Sleep(100); // so the client has time to read the message
        send_message(gameList[id].cSocketX, MOVE, msg);

        if (gameList[id].state != PLAYING)
            break;
    }

    log_a("Game %d ended with state %s", id, game_state_to_string(gameList[id].state));

    closesocket(gameList[id].cSocketX);
    closesocket(gameList[id].cSocketO);

    reset_game(&gameList[id]);
}

void add_client(SOCKET cSocket, SOCKADDR_IN cAddr)
{
    int game_id = -1;
    char player = ' ';

    // search for free game
    for (int i = 0; i < 10; i++)
    {
        if (gameList[i].cSocketX == INVALID_SOCKET)
        {
            game_id = i;
            player = 'X';
            break;
        }
        else if (gameList[i].cSocketO == INVALID_SOCKET)
        {
            game_id = i;
            player = 'O';
            break;
        }
    }

    // couldn't find free game
    if (game_id == -1)
    {
        printf("Couldn't add client - no free games\n");
        return;
    }

    // place client in game
    if (player == 'X')
    {
        gameList[game_id].cSocketX = cSocket;
        gameList[game_id].cAddrX = cAddr;
    }
    else
    {
        gameList[game_id].cSocketO = cSocket;
        gameList[game_id].cAddrO = cAddr;
    }

    // game full -> spawn thread
    if (gameList[game_id].cSocketX != INVALID_SOCKET && gameList[game_id].cSocketO != INVALID_SOCKET)
    {
        gameList[game_id].hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)game_thread, &game_id, 0, NULL);
    }

    // printf("Added client %s:%d\n", inet_ntoa(cAddr.sin_addr), ntohs(cAddr.sin_port));
    log_a("Added client %s:%d", inet_ntoa(cAddr.sin_addr), ntohs(cAddr.sin_port));
}

int main(void)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET hSocket;
    if ((hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        printf("socket creation failed\n");
        return 1;
    }

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TTT_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(hSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("couldn't bind socket\n");
        return 1;
    }

    if (listen(hSocket, 10) == SOCKET_ERROR)
    {
        printf("couldn't listen on socket\n");
        return 1;
    }

    for (int i = 0; i < 10; i++)
    {
        reset_game(&gameList[i]);
    }

    printf("Server started\n");

    while (1)
    {
        SOCKADDR_IN clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET cSocket = accept(hSocket, (SOCKADDR *)&clientAddr, &clientAddrSize);
        if (cSocket == INVALID_SOCKET)
        {
            printf("couldn't accept client\n");
            continue;
        }

        add_client(cSocket, clientAddr);
    }

    closesocket(hSocket);
    WSACleanup();
    return 0;
}

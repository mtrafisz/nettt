#define NET_TTT_IMPLEMENTATION
#include "include/nettt.h"
#define C_ONSOLE_IMPLEMENTATION
#include "include/elosno.c"

void display_board(Player board[3][3], Player player)
{
    clear_console();

    printf("Input 'q' to quit\n");
    printf("You are ");
    set_fg_color(player == X ? BLUE : RED);
    printf("%c\n", player_to_char(player));
    set_fg_color(WHITE);

    for (int i = 0; i < 3; i++)
    {
        char row[3];
        for (int j = 0; j < 3; j++)
        {
            if (board[i][j] != NONE)
                row[j] = player_to_char(board[i][j]);
            else
                row[j] = '0' + i * 3 + j + 1;
        }

        for (int j = 0; j < 3; j++)
        {
            switch (row[j])
            {
            case 'X':
                set_fg_color(BLUE);
                break;
            case 'O':
                set_fg_color(RED);
                break;
            default:
                set_fg_color(WHITE);
                break;
            }

            printf(" %c ", row[j]);

            set_fg_color(WHITE);
            if (j < 2)
                putchar('|');
            else
                putchar('\n');
        }

        if (i < 2)
            printf("---+---+---\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <server_addr>\n", argv[0]);
        return 1;
    }

    int return_code = 0;

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
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(hSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("couldn't connect to specified server\n");
        return_code = 1;
        goto connect_failed;
    }

    char msg[TTT_MSG_LEN];
    MessageType type;

    printf("Waiting for other player...\n");

    recv_message(hSocket, &type, msg);
    if (type != OK)
    {
        printf("couldn't start the game - invalid server response\n");
        return_code = 1;
        goto error_invalid_msg;
    }

    Player board[3][3] = {0};
    GameState state = PLAYING;
    Player player = NONE;

    if (strcmp(msg, "X") == 0)
        player = X;
    else
        player = O;

    display_board(board, player);

    // X moves first
    if (player == O)
        goto player_o_start;

    while (state == PLAYING)
    {
        char *input_str = malloc(3);
        printf("Enter move: ");
        fgets(input_str, 3, stdin);

        if (input_str[0] == 'q')
        {
            send_message(hSocket, LEAVE, "");
            break;
        }

        if (input_str[0] < '1' || input_str[0] > '9')
        {
            printf("Invalid input_str\n");
            continue;
        }

        int move = input_str[0] - '1';

        if (board[move / 3][move % 3] != NONE)
        {
            printf("Invalid move\n");
            continue;
        }

        send_message(hSocket, MOVE, input_str);

        recv_message(hSocket, &type, msg);
        if (type != OK)
        {
            printf("couldn't make a move - invalid server response: %s\n", message_type_to_string(type));
            return_code = 1;
            goto error_invalid_msg;
        }

        board[move / 3][move % 3] = player;

        display_board(board, player);

        // state after our move
        recv_message(hSocket, &type, msg);
        if (type != STATE)
        {
            printf("couldn't get game state - invalid server response: %s\n", message_type_to_string(type));
            return_code = 1;
            goto error_invalid_msg;
        }

        state = string_to_game_state(msg);
        if (state != PLAYING)
            break;

    player_o_start:
        printf("Waiting for opponent's move...\n");

        // state after opponent's move
        recv_message(hSocket, &type, msg);
        if (type != STATE)
        {
            if (type == LEAVE)
            {
                printf("Opponent left the game\n");
                state = player == X ? X_WON : O_WON; // we won
                break;
            }

            printf("couldn't get game state - invalid server response: %s\n", message_type_to_string(type));
            return_code = 1;
            goto error_invalid_msg;
        }

        state = string_to_game_state(msg);

        // opponent's move
        recv_message(hSocket, &type, msg);
        if (type != MOVE)
        {
            printf("couldn't get opponent's move - invalid server response: %s\n", msg);
            return_code = 1;
            goto error_invalid_msg;
        }

        move = atoi(msg) - 1;
        board[move / 3][move % 3] = player == X ? O : X;

        display_board(board, player);
    }

    display_board(board, player);
    if (state == X_WON)
        printf("X won!\n");
    else if (state == O_WON)
        printf("O won!\n");
    else if (state == DRAW)
        printf("Draw!\n");
    else
        printf("State is broken :(\n");

error_invalid_msg:
    closesocket(hSocket);

connect_failed:
    WSACleanup();

    return return_code;
}

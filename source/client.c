#include "../include/nettt.h"
#define C_ONSOLE_IMPLEMENTATION
#include "../include/elosno.c"

Player board[3][3] = {0};
Player player = NONE;

void display_board()
{
    clear_console();

    printf("Input 'q' to quit\n");
    printf("You are ");
    set_fg_color(player == X ? BLUE : RED);
    printf("%c\n", player);
    set_fg_color(WHITE);

    for (int i = 0; i < 3; i++)
    {
        char row[3];
        for (int j = 0; j < 3; j++)
        {
            if (board[i][j] != NONE)
                row[j] = board[i][j];
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


int main (int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        perror("WSAStartup");
        return 1;
    }
#endif
    if (argc < 2) {
        printf("Usage: %s <server_ip>:<server_port>\n", argv[0]);
        return 1;
    }

    char* server_ip = strtok(argv[1], ":");
    char* server_port_str = strtok(NULL, ":");
    int server_port = atoi(server_port_str);

    socket_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        perror("socket");
        return 1;
    }

    ipv4addr serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(server_port);
    serverAddr.sin_addr.s_addr = inet_addr(server_ip);

    ipv4addr localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(0);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        perror("bind");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        perror("connect");
        return 1;
    }

    Message msg;
    GameState gameState = PLAYING;

    puts("Connected to server\nWaiting for other player...");

    NetttRecvMessage(sockfd, &msg);
    if (msg.type != OK) {
        printf("Server error: %s\n", msg.reason);
        return 1;
    }

    if (msg.player_str[0] == X) player = X;
    else {
        player = O;
        goto player_o_start;
    }
    
    display_board();

    while (gameState == PLAYING) {
        char move[3];
        printf("Your move: ");
        fgets(move, 3, stdin);

        if (move[0] == 'q') {
            NetttSendMessage(sockfd, (Message) {
                .type = LEAVE
            });
            break;
        }

        if (move[0] < '1' || move[0] > '9') {
            puts("Invalid move");
            continue;
        }

        move[1] = '\0';
        if (board[(move[0] - '1') / 3][(move[0] - '1') % 3] != NONE) {
            puts("Invalid move");
            continue;
        }

        NetttSendMessage(sockfd, (Message) {
            .type = MOVE, .move = { move[0] }
        });
        NetttRecvMessage(sockfd, &msg);
        if (msg.type != STATE) {
            printf("Server error: %s\n", msg.reason);
            return 1;
        }
        board[(move[0] - '1') / 3][(move[0] - '1') % 3] = player;
        gameState = string_to_game_state(msg.state);
        display_board();

        if (gameState != PLAYING) {
            printf("Game over: %s (%d)\n", game_state_to_string(gameState), gameState);
            break;
        }

    player_o_start:
        display_board();

        NetttRecvMessage(sockfd, &msg);
        if (msg.type == MOVE) {
            board[(msg.move[0] - '1') / 3][(msg.move[0] - '1') % 3] = player == X ? O : X;
            display_board();
        }

        NetttRecvMessage(sockfd, &msg);
        if (msg.type == STATE) {
            gameState = string_to_game_state(msg.state);
            display_board();
        }

        if (gameState != PLAYING) break;        
    }

    if (gameState == PLAYING) puts("Disconnected from server");
    else if (gameState == DRAW) puts("It's a draw!");
    else if (gameState == player) puts("You won!");
    else puts("You lost!");
    

#ifdef _WIN32
    WSACleanup();
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    return 0;
}

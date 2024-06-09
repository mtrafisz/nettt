#include "../include/nettt.h"

#define MAX_GAMES 10

#ifdef _WIN32
#define createGameRoutine(gameIndex) CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gameRoutine, &gameIndex, 0, NULL)
#define gameRoutineJoin(thread) WaitForSingleObject(thread, INFINITE)
#else
#define createGameRoutine(gameIndex) pthread_create(&gameList[gameIndex].thread, NULL, (void* (*)(void*))gameRoutine, &gameIndex)
#define gameRoutineJoin(thread) pthread_join(thread, NULL)
#endif

enum {
	LOG_INFO,
	LOG_ERROR,
};

#define log(level, msg) printf("[%s] %s\n", level == LOG_INFO ? "INFO" : "ERROR", msg);
#define log_a(level, fmt, ...) printf("[%s] " fmt "\n", level == LOG_INFO ? "INFO" : "ERROR", __VA_ARGS__);

typedef struct {
	socket_t clientSockfd[2];
	ipv4addr clientAddr[2];

	Player board[3][3];
	GameState state;

	thread_t thread;
} Game;

Game gameList[MAX_GAMES] = { 0 };

void debugPrintGame(Game* game) {
	printf("Game %p\n", game);
	printf("Client 0: %d\n", game->clientSockfd[0]);
	printf("Client 1: %d\n", game->clientSockfd[1]);
	printf("Board:\n");
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			printf("%c ", game->board[i][j]);
		}
		putchar('\n');
	}
	printf("State: %d\n", game->state);
}

GameState checkGameState(Player board[3][3]) {
	for (int i = 0; i < 3; i++) {
		if (board[i][0] != NONE && board[i][0] == board[i][1] && board[i][0] == board[i][2]) {
			return board[i][0] == X ? X_WON : O_WON;
		}
		if (board[0][i] != NONE && board[0][i] == board[1][i] && board[0][i] == board[2][i]) {
			return board[0][i] == X ? X_WON : O_WON;
		}
	}

	if (board[0][0] != NONE && board[0][0] == board[1][1] && board[0][0] == board[2][2]) {
		return board[0][0] == X ? X_WON : O_WON;
	}
	if (board[0][2] != NONE && board[0][2] == board[1][1] && board[0][2] == board[2][0]) {
		return board[0][2] == X ? X_WON : O_WON;
	}

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (board[i][j] == NONE) return PLAYING;
		}
	}

	return DRAW;
}

void resetGame(Game* game) {
	for (int i = 0; i < 2; i++) {
		game->clientSockfd[i] = INVALID_SOCKET;
		game->clientAddr[i].sin_family = AF_INET;
		game->clientAddr[i].sin_addr.s_addr = htonl(INADDR_ANY);
		game->clientAddr[i].sin_port = htons(0);
	}

	for (int i = 0; i < 9; i++) {
		game->board[i / 3][i % 3] = NONE;
	}
	game->state = PLAYING;
	game->thread = 0;
}

void gameRoutine(int* gameIndex) {
	int gidx = *gameIndex;
	int currentPlayer = 0;
	Game* game = &gameList[*gameIndex];
	Message msg;

	NetttSendMessage(game->clientSockfd[0], (Message) {
		.type = OK, .player_str = { X, '\0' }
	});
	NetttSendMessage(game->clientSockfd[1], (Message) {
		.type = OK, .player_str = { O, '\0' }
	});

	while (game->state == PLAYING) {
		if (!NetttRecvMessage(game->clientSockfd[currentPlayer], &msg)) {
			debugPrintGame(game);
			break;
		}

		if (msg.type != MOVE) {
			NetttSendMessage(game->clientSockfd[currentPlayer], (Message) {
				.type = ERR, .reason = "Invalid message type"
			});
			continue;
		}

		int move = msg.move[0] - '1';
		if (move < 0 || move > 8 || game->board[move / 3][move % 3] != NONE) {
			NetttSendMessage(game->clientSockfd[currentPlayer], (Message) {
				.type = ERR, .reason = "Invalid move"
			});
			continue;
		}

		game->board[move / 3][move % 3] = currentPlayer == 0 ? X : O;
		game->state = checkGameState(game->board);

		NetttSendMessage(game->clientSockfd[!currentPlayer], (Message) {
			.type = MOVE, .move = { msg.move[0] }
		});

		const char* game_state_str = game_state_to_string(game->state);
		msg.type = STATE;
		strcpy(msg.state, game_state_str);
		NetttSendMessage(game->clientSockfd[0], msg);
		NetttSendMessage(game->clientSockfd[1], msg);

		if (game->state != PLAYING) {
			break;
			debugPrintGame(game);
		}
		currentPlayer = 1 - currentPlayer;
	}

	NetttSendMessage(game->clientSockfd[0], (Message) {
		.type = END, .data = ""
	});
	NetttSendMessage(game->clientSockfd[1], (Message) {
		.type = END, .data = ""
	});

	log_a(LOG_INFO, "Game %d ended", gidx);
	resetGame(game);
}

void addClient(socket_t clientSockfd, ipv4addr clientAddr) {
	int gameIndex = -1;
	char player = NONE;

	for (int i = 0; i < MAX_GAMES; i++) {
		if (gameList[i].clientSockfd[0] == INVALID_SOCKET) {
			gameIndex = i;
			player = X;
			break;
		}
		if (gameList[i].clientSockfd[1] == INVALID_SOCKET) {
			gameIndex = i;
			player = O;
			break;
		}
	}

	if (gameIndex == -1) {
		log(LOG_ERROR, "No available games");
		return;
	}

	gameList[gameIndex].clientSockfd[player == X ? 0 : 1] = clientSockfd;
	gameList[gameIndex].clientAddr[player == X ? 0 : 1] = clientAddr;

	log_a(LOG_INFO, "Client connected to game %d as %c", gameIndex, player);

	if (player == O) {
		createGameRoutine(gameIndex);
		log_a(LOG_INFO, "Game %d started", gameIndex);
	}
}

int main(void) {
	int retCode = 0;

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		log(LOG_ERROR, "WSAStartup failed");
		return 1;
	}
#endif
	socket_t sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		log(LOG_ERROR, "Failed to create socket");
		return 1;
	}

	ipv4addr localAddr;
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddr.sin_port = htons(NETTT_PORT);

	if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
		log(LOG_ERROR, "Failed to bind socket");
		retCode = 1;
		goto cleanup_destroy_socket;
	}

	if (listen(sockfd, MAX_GAMES) == SOCKET_ERROR) {
		log(LOG_ERROR, "Failed to listen on socket");
		retCode = 1;
		goto cleanup_destroy_socket;
	}

	for (int i = 0; i < MAX_GAMES; i++) {
		resetGame(&gameList[i]);
		// debugPrintGame(&gameList[i]);
	}
	log_a(LOG_INFO, "Server started on port %d", NETTT_PORT);

	for (;;) {
		ipv4addr clientAddr;
		int clientAddrSize = sizeof(clientAddr);
		socket_t clientSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);
		if (clientSockfd == INVALID_SOCKET) {
			log(LOG_ERROR, "Failed to accept client");
			continue;
		}

		addClient(clientSockfd, clientAddr);
	}

cleanup_destroy_socket:
#ifdef _WIN32
	closesocket(sockfd);
	WSACleanup();
#else
	close(sockfd);
#endif
	return retCode;
}

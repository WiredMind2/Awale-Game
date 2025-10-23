/* Serveur sockets TCP
 * affichage de ce qui arrive sur la socket
 *    socket_server port (port > 1024 sauf root)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

typedef enum
{
    CMD_UNKNOWN = 0,
    CMD_LISTER_JOUEURS,
    CMD_DEFIER,
    CMD_JOUER,
    CMD_GET_BOARD,
    CMD_QUITTER
} client_command_t;

#define MAX_CLIENTS 100
#define MAX_CHALLENGES 100
#define MAX_GAMES 100

struct board
{
    int pits[12];
    int score[2];
    bool current_player; // 0 or 1
    char pseudoA[100];
    char pseudoB[100];
};

struct client_info
{
    char pseudo[100];
    char ip[INET_ADDRSTRLEN];
};

struct challenge
{
    char challenger[100];
    char opponent[100];
};

struct move
{
    char playerA[100];
    char playerB[100];
    int pit_index;
};

struct board_state
{
    int pits[12];
    int score[2];
    int current_player; // 0 or 1
    char pseudoA[100];
    char pseudoB[100];
    int game_exists; // 1 if game exists, 0 if not
};

// Shared memory structure
struct shared_data
{
    struct client_info clients[MAX_CLIENTS];
    int client_count;
    struct challenge challenges[MAX_CHALLENGES];
    int challenge_count;
    struct board boards[MAX_GAMES];
    int boards_count;
};

// Global pointer to shared memory
struct shared_data *shared_mem = NULL;

// Function declarations
void start_game(const char* playerA, const char* playerB);
void handle_list_players(int scomm);
void handle_quit_command(int scomm, const char* pseudo);
void handle_challenge_command(int scomm, const char* pseudo);
void handle_play_command(int scomm, const char* pseudo);
void handle_get_board_command(int scomm, const char* pseudo);
int save_client_info(int scomm, const char* pseudo);

// Function implementations
void start_game(const char* playerA, const char* playerB)
{
    if (shared_mem->boards_count >= MAX_GAMES)
    {
        printf("Max games reached, cannot start a new game.\n");
        return;
    }

    struct board *new_board = &shared_mem->boards[shared_mem->boards_count];
    for (int i = 0; i < 12; i++)
    {
        new_board->pits[i] = 4; // Initialize each pit with 4 seeds
    }
    new_board->score[0] = 0;
    new_board->score[1] = 0;
    new_board->current_player = 0; // Player A starts

    strncpy(new_board->pseudoA, playerA, sizeof(new_board->pseudoA) - 1);
    new_board->pseudoA[sizeof(new_board->pseudoA) - 1] = '\0'; // Ensure null-termination
    strncpy(new_board->pseudoB, playerB, sizeof(new_board->pseudoB) - 1);
    new_board->pseudoB[sizeof(new_board->pseudoB) - 1] = '\0'; // Ensure null-termination

    shared_mem->boards_count++;
    printf("Game started between %s and %s\n", playerA, playerB);
}

void find_game(const char* playerA, const char* playerB, struct board* boards, int boards_count, struct board** out_board)
{
    for (int i = 0; i < boards_count; i++)
    {
        if ((strcmp(boards[i].pseudoA, playerA) == 0 && strcmp(boards[i].pseudoB, playerB) == 0) ||
            (strcmp(boards[i].pseudoA, playerB) == 0 && strcmp(boards[i].pseudoB, playerA) == 0))
        {
            *out_board = &boards[i];
            return;
        }
    }
    *out_board = NULL; // No game found
}

void handle_list_players(int scomm)
{
    // Send the list of connected players
    char list[1024] = "Connected players:\n";
    for (int i = 0; i < shared_mem->client_count; i++)
    {
        strncat(list, shared_mem->clients[i].pseudo, sizeof(list) - strlen(list) - 1);
        strncat(list, " (", sizeof(list) - strlen(list) - 1);
        strncat(list, shared_mem->clients[i].ip, sizeof(list) - strlen(list) - 1);
        strncat(list, ")\n", sizeof(list) - strlen(list) - 1);
    }
    write(scomm, list, strlen(list));
}

void handle_quit_command(int scomm, const char* pseudo)
{
    printf("Client %s quitting\n", pseudo);
    close(scomm);
    exit(0);
}

void handle_challenge_command(int scomm, const char* pseudo)
{
    // Handle challenge command
    char opponent[100];
    ssize_t n = read(scomm, opponent, sizeof(opponent) - 1);
    opponent[n] = '\0';
    
    printf("DEBUG: %s wants to challenge '%s'\n", pseudo, opponent);
    
    // Prevent self-challenges
    if (strcmp(pseudo, opponent) == 0)
    {
        printf("ERROR: %s cannot challenge themselves!\n", pseudo);
        return;
    }
    
    printf("%s challenges %s\n", pseudo, opponent);

    if (shared_mem->challenge_count >= MAX_CHALLENGES)
    {
        printf("Max challenges reached, cannot store more challenges.\n");
        return;
    }

    // Debug: Print current challenges
    printf("DEBUG: Current challenges (%d):\n", shared_mem->challenge_count);
    for (int i = 0; i < shared_mem->challenge_count; i++)
    {
        printf("  [%d] %s -> %s\n", i, shared_mem->challenges[i].challenger, shared_mem->challenges[i].opponent);
    }

    // Check if there's a mutual challenge (opponent has already challenged this player)
    bool mutual_challenge_found = false;
    int mutual_challenge_index = -1;
    
    printf("DEBUG: Looking for mutual challenge: %s -> %s\n", opponent, pseudo);
    
    for (int i = 0; i < shared_mem->challenge_count; i++)
    {
        printf("DEBUG: Checking challenge[%d]: '%s' -> '%s'\n", i, shared_mem->challenges[i].challenger, shared_mem->challenges[i].opponent);
        if (strcmp(shared_mem->challenges[i].challenger, opponent) == 0 && strcmp(shared_mem->challenges[i].opponent, pseudo) == 0)
        {
            printf("DEBUG: MUTUAL CHALLENGE MATCH FOUND at index %d!\n", i);
            printf("Mutual challenge found! %s already challenged %s. Starting game...\n", opponent, pseudo);
            mutual_challenge_found = true;
            mutual_challenge_index = i;
            break;
        }
    }

    if (mutual_challenge_found)
    {
        // Remove the mutual challenge and start the game
        printf("DEBUG: Removing mutual challenge at index %d\n", mutual_challenge_index);
        for (int i = mutual_challenge_index; i < shared_mem->challenge_count - 1; i++)
        {
            shared_mem->challenges[i] = shared_mem->challenges[i + 1];
        }
        shared_mem->challenge_count--;
        printf("DEBUG: Challenge count reduced to %d\n", shared_mem->challenge_count);
        
        start_game(pseudo, opponent);
    }
    else
    {
        printf("DEBUG: No mutual challenge found, checking for duplicates\n");
        // Check if this player has already challenged the same opponent
        bool duplicate_found = false;
        for (int i = 0; i < shared_mem->challenge_count; i++)
        {
            if (strcmp(shared_mem->challenges[i].challenger, pseudo) == 0 && strcmp(shared_mem->challenges[i].opponent, opponent) == 0)
            {
                printf("Challenge from %s to %s already exists.\n", pseudo, opponent);
                duplicate_found = true;
                break;
            }
        }
        
        if (!duplicate_found)
        {
            // Add new challenge
            printf("DEBUG: Adding new challenge at index %d\n", shared_mem->challenge_count);
            strncpy(shared_mem->challenges[shared_mem->challenge_count].challenger, pseudo, sizeof(shared_mem->challenges[shared_mem->challenge_count].challenger) - 1);
            shared_mem->challenges[shared_mem->challenge_count].challenger[sizeof(shared_mem->challenges[shared_mem->challenge_count].challenger) - 1] = '\0';
            strncpy(shared_mem->challenges[shared_mem->challenge_count].opponent, opponent, sizeof(shared_mem->challenges[shared_mem->challenge_count].opponent) - 1);
            shared_mem->challenges[shared_mem->challenge_count].opponent[sizeof(shared_mem->challenges[shared_mem->challenge_count].opponent) - 1] = '\0';
            shared_mem->challenge_count++;
            printf("Challenge from %s to %s recorded. Waiting for %s to challenge back.\n", pseudo, opponent, opponent);
            printf("DEBUG: Challenge count increased to %d\n", shared_mem->challenge_count);
        }
    }
}

void handle_play_command(int scomm, const char* pseudo)
{
    // Handle play command
    struct move player_move;
    ssize_t n = read(scomm, &player_move, sizeof(player_move));
    if (n == sizeof(player_move))
    {
        int pit_index = player_move.pit_index;
        // Validate pit_index
        if (pit_index < 0 || pit_index >= 12)
        {
            printf("Invalid pit index %d from %s\n", pit_index, pseudo);
            return;
        }

        // Find the corresponding board
        struct board* game_board = NULL;
        find_game(player_move.playerA, player_move.playerB, shared_mem->boards, shared_mem->boards_count, &game_board);
        if (game_board == NULL)
        {
            printf("No game found for players %s and %s\n", player_move.playerA, player_move.playerB);
            return;
        }

        // Check if it's the player's turn
        if ((game_board->current_player == 0 && strcmp(game_board->pseudoA, pseudo) != 0) ||
            (game_board->current_player == 1 && strcmp(game_board->pseudoB, pseudo) != 0))
        {
            printf("It's not %s's turn\n", pseudo);
            return;
        }

        // Process the move
        {
            printf("%s plays pit %d\n", pseudo, player_move.pit_index);
            game_board->current_player = !game_board->current_player; // Switch turn
            int seeds = game_board->pits[pit_index];
            game_board->pits[pit_index] = 0;
            for (int off = 0; off < seeds; off++)
            {
                // One seed in each pit
                int *pit = &game_board->pits[(pit_index + 1 + off) % 12];
                if (0 < *pit && *pit <= 2)
                {
                    // Capture
                    game_board->score[game_board->current_player] += *pit + 1;
                    *pit = 0; // Mark as captured
                }
                else
                {
                    // Normal sowing
                    (*pit)++;
                }
            }
        }
    }
    else
    {
        printf("Error reading pit index from %s\n", pseudo);
    }
}

void handle_get_board_command(int scomm, const char* pseudo)
{
    // Read the player names to find the game
    char playerA[100], playerB[100];
    ssize_t n1 = read(scomm, playerA, sizeof(playerA) - 1);
    ssize_t n2 = read(scomm, playerB, sizeof(playerB) - 1);
    
    if (n1 <= 0 || n2 <= 0)
    {
        printf("Error reading player names from %s\n", pseudo);
        return;
    }
    
    playerA[n1] = '\0';
    playerB[n2] = '\0';
    
    printf("Board state requested by %s for game %s vs %s\n", pseudo, playerA, playerB);
    
    struct board_state board_state;
    struct board* game_board = NULL;
    
    // Find the game
    find_game(playerA, playerB, shared_mem->boards, shared_mem->boards_count, &game_board);
    
    if (game_board != NULL)
    {
        // Game exists, copy the board state
        for (int i = 0; i < 12; i++)
        {
            board_state.pits[i] = game_board->pits[i];
        }
        board_state.score[0] = game_board->score[0];
        board_state.score[1] = game_board->score[1];
        board_state.current_player = game_board->current_player;
        strncpy(board_state.pseudoA, game_board->pseudoA, sizeof(board_state.pseudoA) - 1);
        board_state.pseudoA[sizeof(board_state.pseudoA) - 1] = '\0';
        strncpy(board_state.pseudoB, game_board->pseudoB, sizeof(board_state.pseudoB) - 1);
        board_state.pseudoB[sizeof(board_state.pseudoB) - 1] = '\0';
        board_state.game_exists = 1;
        
        printf("Sending board state for active game\n");
    }
    else
    {
        // No game found
        board_state.game_exists = 0;
        printf("No game found between %s and %s\n", playerA, playerB);
    }
    
    // Send the board state back to client
    if (write(scomm, &board_state, sizeof(board_state)) < 0)
    {
        perror("Error sending board state");
    }
}

int save_client_info(int scomm, const char* pseudo)
{
    if (shared_mem->client_count >= MAX_CLIENTS)
    {
        printf("Max clients reached, cannot store more client info.\n");
        return -1; // Error: max clients reached
    }

    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    if (getpeername(scomm, (struct sockaddr *)&addr, &addr_size) < 0)
    {
        perror("getpeername failed");
        return -1; // Error: couldn't get peer address
    }

    // Convert IP address to string
    if (inet_ntop(AF_INET, &addr.sin_addr, shared_mem->clients[shared_mem->client_count].ip, sizeof(shared_mem->clients[shared_mem->client_count].ip)) == NULL)
    {
        perror("inet_ntop failed");
        return -1; // Error: couldn't convert IP to string
    }

    // Copy pseudo with proper null termination
    strncpy(shared_mem->clients[shared_mem->client_count].pseudo, pseudo, sizeof(shared_mem->clients[shared_mem->client_count].pseudo) - 1);
    shared_mem->clients[shared_mem->client_count].pseudo[sizeof(shared_mem->clients[shared_mem->client_count].pseudo) - 1] = '\0';

    shared_mem->client_count++;
    printf("Client %s (%s) registered successfully\n", pseudo, shared_mem->clients[shared_mem->client_count - 1].ip);
    return 0; // Success
}

int main(int argc, char **argv)
{
    int sockfd, scomm, pid;
    client_command_t command;
    struct sockaddr_in serv_addr;

    // Initialize shared memory
    shared_mem = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_mem == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    
    // Initialize shared memory values
    shared_mem->client_count = 0;
    shared_mem->challenge_count = 0;
    shared_mem->boards_count = 0;
    printf("Shared memory initialized successfully\n");

    if (argc != 2)
    {
        printf("usage: socket_server port\n");
        exit(0);
    }

    printf("server starting...\n");

    /* ouverture du socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("impossible d'ouvrir le socket\n");
        exit(0);
    }

    /* initialisation des parametres */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    /* effecture le bind */
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("impossible de faire le bind\n");
        exit(0);
    }

    /* petit initialisation */
    listen(sockfd, 1);

    /* attend la connection d'un client */

    while (1)
    {
        scomm = accept(sockfd, NULL, NULL);
        pid = fork();
        if (pid == 0) /* c’est le fils */
        {
            printf("connection accepted\n");
            close(sockfd); /* socket inutile pour le fils */
            if (scomm < 0)
            {
                perror("accept");
                exit(1);
            }
            /* traiter la communication */

            /* reception du pseudo */
            char pseudo[100];
            int n = read(scomm, pseudo, sizeof(pseudo) - 1);
            pseudo[n] = '\0'; // Null-terminate the string
            printf("Received pseudo: %s\n", pseudo);
            
            // Store client info
            if (save_client_info(scomm, pseudo) < 0)
            {
                close(scomm);
                exit(1);
            }

            /* traiter la communication */
            while (1)
            {
                if (read(scomm, &command, sizeof(command)) == sizeof(command))
                {
                    if (command == CMD_LISTER_JOUEURS)
                    {
                        handle_list_players(scomm);
                    }
                    else if (command == CMD_QUITTER)
                    {
                        handle_quit_command(scomm, pseudo);
                    }
                    else if (command == CMD_DEFIER)
                    {
                        handle_challenge_command(scomm, pseudo);
                    }
                    else if (command == CMD_JOUER)
                    {
                        handle_play_command(scomm, pseudo);
                    }
                    else if (command == CMD_GET_BOARD)
                    {
                        handle_get_board_command(scomm, pseudo);
                    }
                    else
                    {
                        printf("Unknown command from %s: %d\n", pseudo, command);
                    }
                }
                else
                {
                    printf("connection closed\n");
                    close(scomm);
                    exit(0); /* on force la terminaison du fils */
                }
            }
        }
        else /* c’est le pere */
        {
            close(scomm); /* socket inutile pour le pere */
        }
    }

    close(sockfd);

    return 1;
}

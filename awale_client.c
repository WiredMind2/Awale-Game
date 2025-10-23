/* Client sockets TCP pour le jeu Awale
 * Connexion au serveur et gestion des commandes
 * Usage: awale_client <server_ip> <port> <pseudo>
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
#include <netdb.h>

typedef enum
{
    CMD_UNKNOWN = 0,
    CMD_LISTER_JOUEURS,
    CMD_DEFIER,
    CMD_JOUER,
    CMD_GET_BOARD,
    CMD_QUITTER
} client_command_t;

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

// Function declarations
void print_menu();
void send_list_players_command(int sockfd);
void send_challenge_command(int sockfd, const char* opponent);
void send_play_command(int sockfd, const char* playerA, const char* playerB, int pit_index);
void send_get_board_command(int sockfd, const char* playerA, const char* playerB);
void send_get_board_command_silent(int sockfd, const char* playerA, const char* playerB);
void send_quit_command(int sockfd);
void display_board(const struct board_state* board);
int get_user_choice();
void clear_input_buffer();

// Function implementations
void print_menu()
{
    printf("\n=== AWALE GAME CLIENT ===\n");
    printf("1. Lister les joueurs connectés\n");
    printf("2. Défier un joueur\n");
    printf("3. Jouer un coup\n");
    printf("4. Voir l'état du plateau\n");
    printf("5. Quitter\n");
    printf("Votre choix: ");
}

void send_list_players_command(int sockfd)
{
    client_command_t command = CMD_LISTER_JOUEURS;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande lister joueurs");
        return;
    }
    
    // Read response
    char response[1024];
    ssize_t n = read(sockfd, response, sizeof(response) - 1);
    if (n > 0)
    {
        response[n] = '\0';
        printf("Réponse du serveur:\n%s\n", response);
    }
    else
    {
        printf("Aucune réponse du serveur.\n");
    }
}

void send_challenge_command(int sockfd, const char* opponent)
{
    client_command_t command = CMD_DEFIER;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande défier");
        return;
    }
    
    if (write(sockfd, opponent, strlen(opponent)) < 0)
    {
        perror("Erreur envoi nom adversaire");
        return;
    }
    
    printf("Défi envoyé à %s\n", opponent);
}

void send_play_command(int sockfd, const char* playerA, const char* playerB, int pit_index)
{
    client_command_t command = CMD_JOUER;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande jouer");
        return;
    }
    
    struct move player_move;
    strncpy(player_move.playerA, playerA, sizeof(player_move.playerA) - 1);
    player_move.playerA[sizeof(player_move.playerA) - 1] = '\0';
    strncpy(player_move.playerB, playerB, sizeof(player_move.playerB) - 1);
    player_move.playerB[sizeof(player_move.playerB) - 1] = '\0';
    player_move.pit_index = pit_index;
    
    if (write(sockfd, &player_move, sizeof(player_move)) < 0)
    {
        perror("Erreur envoi coup");
        return;
    }
    
    printf("Coup joué: fosse %d\n", pit_index);
}

void send_quit_command(int sockfd)
{
    client_command_t command = CMD_QUITTER;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande quitter");
        return;
    }
    printf("Commande de déconnexion envoyée.\n");
}

void send_get_board_command(int sockfd, const char* playerA, const char* playerB)
{
    client_command_t command = CMD_GET_BOARD;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande plateau");
        return;
    }
    
    // Send player names
    if (write(sockfd, playerA, strlen(playerA)) < 0)
    {
        perror("Erreur envoi nom joueur A");
        return;
    }
    
    if (write(sockfd, playerB, strlen(playerB)) < 0)
    {
        perror("Erreur envoi nom joueur B");
        return;
    }
    
    // Read board state response
    struct board_state board;
    ssize_t n = read(sockfd, &board, sizeof(board));
    if (n == sizeof(board))
    {
        if (board.game_exists)
        {
            printf("État du plateau pour la partie %s vs %s:\n", playerA, playerB);
            display_board(&board);
        }
        else
        {
            printf("Aucune partie trouvée entre %s et %s.\n", playerA, playerB);
        }
    }
    else
    {
        printf("Erreur lors de la réception de l'état du plateau.\n");
    }
}

void send_get_board_command_silent(int sockfd, const char* playerA, const char* playerB)
{
    client_command_t command = CMD_GET_BOARD;
    if (write(sockfd, &command, sizeof(command)) < 0)
    {
        perror("Erreur envoi commande plateau");
        return;
    }
    
    // Send player names
    if (write(sockfd, playerA, strlen(playerA)) < 0)
    {
        perror("Erreur envoi nom joueur A");
        return;
    }
    
    if (write(sockfd, playerB, strlen(playerB)) < 0)
    {
        perror("Erreur envoi nom joueur B");
        return;
    }
    
    // Read board state response
    struct board_state board;
    ssize_t n = read(sockfd, &board, sizeof(board));
    if (n == sizeof(board))
    {
        if (board.game_exists)
        {
            display_board(&board);
        }
        else
        {
            printf("❌ Aucune partie trouvée entre %s et %s.\n", playerA, playerB);
        }
    }
    else
    {
        printf("Erreur lors de la réception de l'état du plateau.\n");
    }
}

void display_board(const struct board_state* board)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                    PLATEAU AWALE                          \n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Joueur B: %s (Score: %d)                    %s\n", 
           board->pseudoB, board->score[1], 
           (board->current_player == 1) ? "← À TOI!" : "");
    printf("\n");
    
    // Top row (Player B's pits: 11, 10, 9, 8, 7, 6)
    printf("   ┌────┬────┬────┬────┬────┬────┐\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[11], board->pits[10], board->pits[9], 
           board->pits[8], board->pits[7], board->pits[6]);
    printf("   │ 11 │ 10 │ 9  │ 8  │ 7  │ 6  │\n");
    printf("   ├────┼────┼────┼────┼────┼────┤\n");
    printf("   │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │\n");
    printf("   │%2d  │%2d  │%2d  │%2d  │%2d  │%2d  │\n",
           board->pits[0], board->pits[1], board->pits[2], 
           board->pits[3], board->pits[4], board->pits[5]);
    printf("   └────┴────┴────┴────┴────┴────┘\n");
    printf("\n");
    printf("%s Joueur A: %s (Score: %d)\n", 
           (board->current_player == 0) ? "À TOI! →" : "",
           board->pseudoA, board->score[0]);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Tour du joueur: %s\n", 
           (board->current_player == 0) ? board->pseudoA : board->pseudoB);
    
    // Add helpful playing instructions
    if (board->current_player == 0)
    {
        printf("💡 %s peut jouer les fosses 0 à 5 (rangée du bas)\n", board->pseudoA);
    }
    else
    {
        printf("💡 %s peut jouer les fosses 6 à 11 (rangée du haut)\n", board->pseudoB);
    }
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}

int get_user_choice()
{
    int choice;
    if (scanf("%d", &choice) != 1)
    {
        clear_input_buffer();
        return -1;
    }
    clear_input_buffer();
    return choice;
}

void clear_input_buffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void print_game_board()
{
    printf("\n=== PLATEAU DE JEU AWALE ===\n");
    printf("   [11][10][ 9][ 8][ 7][ 6]  <- Joueur B\n");
    printf("B                             A\n");
    printf("   [ 0][ 1][ 2][ 3][ 4][ 5]  <- Joueur A\n");
    printf("=============================\n");
    printf("Les fosses sont numérotées de 0 à 11\n");
    printf("Joueur A: fosses 0-5, Joueur B: fosses 6-11\n\n");
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char pseudo[100];
    
    if (argc != 4)
    {
        printf("Usage: %s <server_ip> <port> <pseudo>\n", argv[0]);
        exit(1);
    }
    
    strncpy(pseudo, argv[3], sizeof(pseudo) - 1);
    pseudo[sizeof(pseudo) - 1] = '\0';
    
    printf("=== CLIENT AWALE ===\n");
    printf("Connexion au serveur %s:%s avec le pseudo '%s'\n", argv[1], argv[2], pseudo);
    
    /* Création du socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Erreur ouverture socket");
        exit(1);
    }
    
    /* Résolution du nom du serveur */
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "Erreur: serveur %s introuvable\n", argv[1]);
        exit(1);
    }
    
    /* Configuration de l'adresse du serveur */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    /* Connexion au serveur */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erreur connexion");
        exit(1);
    }
    
    printf("Connexion établie avec le serveur!\n");
    
    /* Envoi du pseudo */
    if (write(sockfd, pseudo, strlen(pseudo)) < 0)
    {
        perror("Erreur envoi pseudo");
        exit(1);
    }
    
    printf("Pseudo envoyé. Vous êtes maintenant connecté!\n");
    print_game_board();
    
    /* Boucle principale du client */
    while (1)
    {
        print_menu();
        int choice = get_user_choice();
        
        switch (choice)
        {
            case 1: // Lister joueurs
                send_list_players_command(sockfd);
                break;
                
            case 2: // Défier un joueur
            {
                printf("Entrez le pseudo de l'adversaire à défier: ");
                char opponent[100];
                if (fgets(opponent, sizeof(opponent), stdin) != NULL)
                {
                    // Remove newline if present
                    size_t len = strlen(opponent);
                    if (len > 0 && opponent[len-1] == '\n')
                        opponent[len-1] = '\0';
                    
                    if (strlen(opponent) > 0)
                    {
                        send_challenge_command(sockfd, opponent);
                    }
                    else
                    {
                        printf("Pseudo invalide.\n");
                    }
                }
                break;
            }
            
            case 3: // Jouer un coup
            {
                printf("Entrez le pseudo du joueur A: ");
                char playerA[100];
                if (fgets(playerA, sizeof(playerA), stdin) != NULL)
                {
                    size_t len = strlen(playerA);
                    if (len > 0 && playerA[len-1] == '\n')
                        playerA[len-1] = '\0';
                }
                
                printf("Entrez le pseudo du joueur B: ");
                char playerB[100];
                if (fgets(playerB, sizeof(playerB), stdin) != NULL)
                {
                    size_t len = strlen(playerB);
                    if (len > 0 && playerB[len-1] == '\n')
                        playerB[len-1] = '\0';
                }
                
                if (strlen(playerA) > 0 && strlen(playerB) > 0)
                {
                    // First, get and display the current board state
                    printf("\n🎮 État actuel du plateau:\n");
                    send_get_board_command_silent(sockfd, playerA, playerB);
                    
                    // Now ask for the move
                    printf("\nMaintenant, choisissez votre coup:\n");
                    printf("Entrez le numéro de la fosse à jouer (0-11): ");
                    int pit_index = get_user_choice();
                    
                    if (pit_index >= 0 && pit_index < 12)
                    {
                        send_play_command(sockfd, playerA, playerB, pit_index);
                        
                        // After the move, show the updated board
                        printf("\n🎮 Plateau après votre coup:\n");
                        send_get_board_command_silent(sockfd, playerA, playerB);
                    }
                    else
                    {
                        printf("Numéro de fosse invalide (doit être entre 0 et 11).\n");
                    }
                }
                else
                {
                    printf("Noms de joueurs invalides.\n");
                }
                break;
            }
            
            case 4: // Voir l'état du plateau
            {
                printf("Entrez le pseudo du joueur A: ");
                char playerA[100];
                if (fgets(playerA, sizeof(playerA), stdin) != NULL)
                {
                    size_t len = strlen(playerA);
                    if (len > 0 && playerA[len-1] == '\n')
                        playerA[len-1] = '\0';
                }
                
                printf("Entrez le pseudo du joueur B: ");
                char playerB[100];
                if (fgets(playerB, sizeof(playerB), stdin) != NULL)
                {
                    size_t len = strlen(playerB);
                    if (len > 0 && playerB[len-1] == '\n')
                        playerB[len-1] = '\0';
                }
                
                if (strlen(playerA) > 0 && strlen(playerB) > 0)
                {
                    send_get_board_command(sockfd, playerA, playerB);
                }
                else
                {
                    printf("Noms de joueurs invalides.\n");
                }
                break;
            }
            
            case 5: // Quitter
                send_quit_command(sockfd);
                printf("Au revoir!\n");
                close(sockfd);
                exit(0);
                break;
                
            default:
                printf("Choix invalide. Veuillez choisir entre 1 et 5.\n");
                break;
        }
    }
    
    close(sockfd);
    return 0;
}
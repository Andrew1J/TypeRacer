#include "client.h"
#include "../trprotocol/trprotocol.h"


int main() {
    int sd = do_connect();  // Client connection

    char *username = get_send_usrname(sd);  // Username prompt and processing

    // Is this client the host?
    struct TRPacket *host_pkt = recv_urhost_pkt(sd);
    int am_host = host_pkt->host;
    free(host_pkt);

    struct TRPacket *text_pkt = recv_typetext_pkt(sd);  // Receive Text to be typed
    char *type_text = text_pkt->text;
    free(text_pkt);  // free the packet, but not the text

    if (am_host) { // if is host prompt start game
        char line[10];
        printf("Start Game? [Y/N]: "); // Prompt
    	fgets(line, 10, stdin); // Read from STDIN

        if (strcmp(line,"N\n")==0 || strcmp(line,"Y\n")!=0) return 0; // If N, end client

        struct TRPacket *rstart_pkt = calloc(1, sizeof(struct TRPacket));
        rstart_pkt->type = 4;
        send_rstart_pkt(sd, rstart_pkt); // Send race start packet
        free(rstart_pkt);
    }

    // Receive game start packet
    struct TRPacket *rstart = recv_rstart_pkt(sd);

    setup_curses();
    refresh();

    int state = 1;

    char *typed = calloc(1, strlen(type_text));  // Text typed by player
    int text_position = 0;  // Current position in text
    int typed_ch;  // Last typed character
    int text_len = strlen(type_text);  // Total text length

    // Player Statistics
    int total_typed = 0;
    int wpm = 0;
    int accuracy = 100;
    int num_errors = 0;

    // Main Loop
    while (state) {
        if (state == 1) {  // pregame
            draw_static_elements(username);

            // Draw the text to be typed
            attron(COLOR_PAIR(3));
            mvprintw(3, 0, "%s", type_text);

            mvchgat(3, 0, 1, A_UNDERLINE, 0, NULL);

            state++;
        }
        else if (state == 2) {
            typed_ch = getch();

            switch (typed_ch) {
                case ERR:  // nothing to getch
                    continue;
                    break;

                // Handle backspace for all terminals (maybe)
                case KEY_BACKSPACE:
                case KEY_DC:
                case 127:
                case '\b':
                    if (text_position == 0) break;

                    typed[--text_position] = 0;
                    break;

                default:
                    typed[text_position] = typed_ch;
                    text_position++;
            }

            mvprintw(3, 0, "");
            for (int i=0; i<strlen(typed); i++) {
                if (typed[i] == type_text[i]) {
                    attron(COLOR_PAIR(4));
                    printw("%c", type_text[i]);
                } else {
                    attron(COLOR_PAIR(1));
                    printw("%c", type_text[i]);
                }
            }

            //
            // attron(COLOR_PAIR(2));
            // mvprintw(3, 0, "%s", typed);

            int curr_x, curr_y;
            getyx(stdscr, curr_y, curr_x);

            attron(COLOR_PAIR(3));
            printw("%s", type_text+text_position);

            mvchgat(curr_y, curr_x, 1, A_UNDERLINE, 0, NULL);

            // game
            // IF SPACE SEND progress packet to server


            // RECEIVE progress of other clients from server
            // SLEEP
            // REDRAW screen based on keyboard input
        }


        if (typed_ch == 27) { // QUIT game (escape char)
            state = 0;
        }
        refresh();
    }


    endwin();

    return 0;
}


/**
 * Prompts the user for a username and sends it to
 * the server.
 *
 * @return The username inputted
 */
char * get_send_usrname(int sockfd) {
    char *username = calloc(1024, sizeof(char));
    struct TRPacket *uname_pkt = calloc(1, sizeof(struct TRPacket));

    // Prompt USERNAME
    printf("Username: "); // Prompt
    fgets(username, 1024, stdin); // Read from STDIN
    *strrchr(username, '\n') = 0;
    uname_pkt->type = 0;
    uname_pkt->uname_length = strlen(username);
    uname_pkt->username = username;
    send_usr_pkt(sockfd, uname_pkt);

    free(uname_pkt);

    return username;
}


/**
 * Prompts the user for connection information
 * and connects to the server specified.
 *
 * @return A file descriptor for communication with the server
 */
int do_connect() {
    printf("Welcome to TypeRacer!\n\n");

    printf("Hostname (leave blank for localhost): ");
    char *hostname = calloc(6, sizeof(char));  // see https://stackoverflow.com/questions/8724954/what-is-the-maximum-number-of-characters-for-a-host-name-in-unix
    fgets(hostname, 256, stdin);
    *strrchr(hostname, '\n') = 0;
    if (strlen(hostname) == 0) {
        memcpy(hostname, "localhost", 10);
    }

    printf("Port (leave blank for default): ");
    char *port = calloc(6, sizeof(char));
    fgets(port, 6, stdin);
    *strrchr(port, '\n') = 0;
    if (strlen(port) == 0) {
        memcpy(port, "9001", 5);
    }

    int sd = client_connect(hostname, port);
    free(hostname);
    free(port);

    return sd;
}


/**
 * Connects to the server
 *
 * @param host Hostname/IP address string
 * @param port port number string
 * @return File descriptor for the connected server
 */
int client_connect(char *host, char *port) {
    struct addrinfo *hints, *results;

    hints = calloc(1,sizeof(struct addrinfo));
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_PASSIVE;

    getaddrinfo(host, port, hints, &results);

    //create socket
    int sd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

    int conn = connect(sd, results->ai_addr, results->ai_addrlen);
    if (conn) {
        printf("[client] couldn't connect\n");
        exit(EXIT_FAILURE);
    }

    free(hints);
    freeaddrinfo(results);

    return sd;
}


/**
 * Sets up the terminal with no echoing,
 * no cursor, and non-blocking getch
 */
void setup_curses() {
    // Initialize Curses
    initscr();
    noecho();  // don't echo typed characters
    curs_set(FALSE);  // no cursor
    // nodelay(stdscr, TRUE);  // non-blocking getch

    // Initialize Colors
    if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
}


/**
 * Draws static elements (Typetext header,
 * username footer)
 *
 * @param username This username
 */
void draw_static_elements(char *username) {
    // Terminal dimensions
    int row, col;
    getmaxyx(stdscr, row, col);

    // Draw Typeracer text
    attron(COLOR_PAIR(5));
    mvprintw(0, 2, "TypeRacer");

    // Draw username text
    attron(COLOR_PAIR(3));
    mvprintw(row-2, 2, "%s", username);
}


/**
 * Draws dynamic elements (Accuracy, WPM,
 * typed text, errors, etc)
 *
 * @param type_text Text to be typed
 * @param user_typed Text the user has typed
 */
void draw_dynamic_elements(char *type_text, char *user_typed, int wpm, int accuracy) {
    // Terminal dimensions
    int row, col;
    getmaxyx(stdscr, row, col);

    attron(COLOR_PAIR(2));
    mvprintw(0, col-12, "Accuracy: N/A");
    mvprintw(0, col-22, "WPM: %d", wpm);

    attron(COLOR_PAIR(3));
    mvprintw(3, 2, "%s", type_text);
}

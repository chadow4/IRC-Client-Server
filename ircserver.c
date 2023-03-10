#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define PORT 8081
#define MAX_CLIENTS 100

// Maillon pour stocker les informations sur les clients

typedef struct client_t {
    int sockfd;
    char *pseudo;
    int pseudoSet;
} client_t;

// Liste chainé pour stocker les clients
typedef struct node_t {
    client_t *client;
    struct node_t *next;
} node_t;

// Liste of pseudos
typedef struct pseudos_list {
    char *pseudo;
    char *password;
    struct pseudos_list *suiv;

} pseudos_list;


node_t *head = NULL;


void stop(char *s) {
    perror(s);
    exit(1);
}

void red() {
    printf("\033[1;31m");
}

void yellow() {
    printf("\033[1;33m");
}


void reset() {
    printf("\033[0m");
}

/*
 * SendToAllClients Function
 * broadcast message for all clients
*/
void SendToAllClients(char *buffer, int sockfd) {
    printf("%s\n", buffer);
    node_t *sending_node = head;
    while (sending_node != NULL) {
        client_t *sending_client = sending_node->client;
        int sending_sockfd = sending_client->sockfd;
        if (sending_sockfd != sockfd && sending_sockfd > -1) {
            if (send(sending_sockfd, buffer, strnlen(buffer, 1024), 0) < 0) {
                stop("send");
            }
        }
        sending_node = (node_t *) sending_node->next;
    }
}

/*
 * checkIfPseudoExistInRegisterFile Function
 * check if the user exist in login.dat file and return -1 if exist
*/
int checkIfPseudoExistInRegisterFile(char *pseudo) {
    FILE *fileRead;
    fileRead = fopen("login.dat", "rb");
    if (fileRead == NULL) {
        printf("Error opening file\n");
        return 1;
    }
    char *pseudoReading = calloc(sizeof(char), 32 + 1);
    while (fread(pseudoReading, 32, sizeof(char), fileRead)) {
        if (strcmp(pseudo, pseudoReading) == 0) {
            return -1;
        }
        fseek(fileRead, 256, SEEK_CUR);

    }
    fclose(fileRead);
    return 0;
}

/*
 * checkerRegisterFile Function
 * return pseudo if it is in the login.dat file
*/

char *checkerRegisterFile(char *pseudo, char *password) {
    FILE *fileRead;
    fileRead = fopen("login.dat", "rb");
    if (fileRead == NULL) {
        printf("Error opening file\n");
        return NULL;
    }
    char *pseudoReading = calloc(sizeof(char), 32 + 1);
    char *passwordReading = calloc(sizeof(char), 256 + 1);
    while (fread(pseudoReading, 32, sizeof(char), fileRead)) {
        if (strcmp(pseudo, pseudoReading) == 0) {
            fread(passwordReading, 256, sizeof(char), fileRead);
            if (strcmp(password, passwordReading) == 0) {
                return pseudoReading;
            } else {
                return NULL;
            }
        } else {
            fseek(fileRead, 256, SEEK_CUR);
        }
    }
    fclose(fileRead);
    return NULL;
}

/*
 * checkIfPseudoExistInLC Function
 * verify if the pseudo is in LinkedList
*/

int checkIfPseudoExistInLC(char *pseudo) {
    node_t *pseudoNode = head;
    while (pseudoNode != NULL) {
        client_t *client = pseudoNode->client;
        if (strcmp(client->pseudo, pseudo) == 0) {
            return -1;
        }
        pseudoNode = (node_t *) pseudoNode->next;
    }
    return 0;
}
/*
 * checkIfSocketIsNull Function
 * verify if one socket is null in the LinkedList
*/
client_t *checkIfSocketIsNull() {
    node_t *check_socket_node = head;
    while (check_socket_node != NULL) {
        client_t *client = check_socket_node->client;
        if (client->sockfd == -1) {
            return client;
        }
        check_socket_node = (node_t *) check_socket_node->next;
    }
    return NULL;
}

/*
 * disconnectClient Function
 * If the client is disconnected, the socket is set to -1 and the name is reset by replacing all characters with null characters (\0)
*/
void disconnectClient(char *pseudo) {
    node_t *disconnect_node = head;
    char buffer[1024] = {0};
    while (disconnect_node != NULL) {
        client_t *client = disconnect_node->client;
        if (strcmp(client->pseudo, pseudo) == 0) {
            yellow();
            printf("%s with socket %d is disconnected\n", client->pseudo, client->sockfd);
            reset();
            snprintf(buffer, sizeof(buffer),
                     "you are now disconnected because your pseudo is now registered by an other client.");
            if (send(client->sockfd, buffer, 1024, 0) < 0) {
                stop("send");
            }
            memset(client->pseudo, '\0', 256);
            close(client->sockfd);
            client->sockfd = -1;
            client->pseudoSet = 0;
        }
        disconnect_node = (node_t *) disconnect_node->next;
    }
}

/*
 * sendPrivateMessage Function
 * this function send unicast message
 */

void sendPrivateMessage(char *buffer, char *pseudo) {
    node_t *sending_node = head;
    while (sending_node != NULL) {
        client_t *client = sending_node->client;
        if (strcmp(client->pseudo, pseudo) == 0) {
            printf("sended to %s : %s\n",client->pseudo,buffer);
            if (send(client->sockfd, buffer, 1024, 0) < 0) {
                stop("send");
            }
        }
        sending_node = (node_t *) sending_node->next;
    }
}


int main(int argc, char const *argv[]) {
    int server_fd, newsockfd, valread;
    struct sockaddr_in address;
    int opt = 1;
    char buffer[1024] = {0};
    char tempbuff[1024 + 256 +32] = {0};
    fd_set read_fds; // Set of file descriptors for select function
    int fdmax; // Maximum file descriptor number
    int addrlen = sizeof(address);

    // create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        stop("Socket Failed");
    }

    // Option of socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        stop("setsockopt");
    }

    // replacing all characters with null characters (\0).
    memset((char *) &address, 0, sizeof(address)); // remplissage de serveraddr de 0

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding of socket
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        stop("bind failed");
    }

    // start of listening for incoming connections
    if (listen(server_fd, 5) < 0) {
        stop("listen");
    }

    // Add server socket to set
    FD_SET(server_fd, &read_fds);
    fdmax = server_fd;
    yellow();
    printf("Server started on port : %d\n ", PORT);
    reset();

    while (1) {
        // Reset the set of file descriptors
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        node_t *current_node_reset = head;

        while (current_node_reset != NULL) {
            client_t *current_client = current_node_reset->client;
            if (current_client->sockfd > 0) {
                FD_SET(current_client->sockfd, &read_fds);
            }
            current_node_reset = (node_t *) current_node_reset->next;
        }

        // Wait for an activity on any of the sockets
        int activity = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            stop("select error");
        }

        // If something happened on the server socket, it's probably a new connection
        if (FD_ISSET(server_fd, &read_fds)) {
            if ((newsockfd = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                stop("accept");
            }
            yellow();
            printf("unknown with socket %d connected !\n", newsockfd);
            reset();
            // Add new client to the list of client file descriptors

            if (checkIfSocketIsNull() == NULL) {

                // create new client
                client_t *new_client = (client_t *) malloc(sizeof(client_t));
                new_client->sockfd = newsockfd;
                new_client->pseudo = calloc(sizeof(char), 256);
                new_client->pseudoSet = 0;

                // adding client in the linkedList
                node_t *new_node = (node_t *) malloc(sizeof(node_t));
                new_node->client = new_client;
                new_node->next = (struct node_t *) head;
                head = new_node;

                // Update maximum file descriptor number
                if (newsockfd > fdmax) {
                    fdmax = newsockfd;
                }
            } else {
                // create client  if one socket is egal to -1
                client_t *client_existing = checkIfSocketIsNull();
                client_existing->sockfd = newsockfd;
                client_existing->pseudo = calloc(sizeof(char), 256);
                client_existing->pseudoSet = 0;
            }

        }
        // Else, check the other file descriptors
        node_t *current_node = head;
        while (current_node != NULL) {
            client_t *current_client = current_node->client;
            if (FD_ISSET(current_client->sockfd, &read_fds)) {
                memset(buffer, '\0', 1024);
                valread = recv(current_client->sockfd, buffer, 1024, 0);
                if (valread == 0) {
                    close(current_client->sockfd);
                    yellow();
                    printf("%s with socket %d is disconnected\n", current_client->pseudo, current_client->sockfd);
                    reset();
                    memset(current_client->pseudo, '\0', 256);
                    current_client->sockfd = -1;
                    current_client->pseudoSet = 0;
                } else {

                    /* Set Name for New Client */
                    if (current_client->pseudoSet == 0) {
                        if (checkIfPseudoExistInLC(buffer) == -1 || checkIfPseudoExistInRegisterFile(buffer) == -1) {
                            memset(buffer, '\0', 1024);
                            snprintf(buffer, 1024, "Sorry, pseudo is already use, please enter a new pseudo :");
                            send(current_client->sockfd, buffer, 1024, 0);
                        } else {
                            strncpy(current_client->pseudo, buffer, 256);
                            current_client->pseudoSet = 1;
                            yellow();
                            printf("set pseudo %s for socket %d \n", current_client->pseudo, current_client->sockfd);
                            reset();
                            memset(buffer, '\0', 1024);
                            snprintf(buffer, 1024, "you are now connected as %s", current_client->pseudo);
                            send(current_client->sockfd, buffer, 1024, 0);
                        }

                        /* All Commands */
                    } else if (buffer[0] == '/') {
                        char *tbuf = strtok(buffer, " ");
                        char *tbufSend = tbuf;

                        /* Nickname Command */
                        if (tbuf != NULL && !strcmp(tbuf, "/nickname")) {
                            char nickname[32] = {0};
                            char password[256] = {0};
                            tbuf = strtok(NULL, " ");
                            if (tbuf != NULL && tbuf[0] != '\0') {
                                memset(nickname, '\0', 32);
                                strncpy(nickname, tbuf, 32);
                            }
                            if (nickname[0] == '\0') {
                                snprintf(buffer, sizeof(buffer),
                                         "You dont have enter pseudo, please retry /nickname new_pseudo :");
                                if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                    stop("send");
                                }
                            } else {
                                tbuf = strtok(NULL, "\n");
                                if (tbuf != NULL && tbuf[0] != '\0') {
                                    strncpy(password, tbuf, sizeof(password) - 1);
                                }
                                if (password[0] == '\0') {
                                    if (checkIfPseudoExistInLC(nickname) == -1 ||
                                        checkIfPseudoExistInRegisterFile(nickname) == -1) {
                                        memset(buffer, '\0', 1024);
                                        snprintf(buffer, sizeof(buffer),
                                                 "Sorry, pseudo is already use, please retry /nickname new_pseudo :");
                                        if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                            stop("send");
                                        }
                                    } else {
                                        strncpy(current_client->pseudo, nickname, 32);
                                        memset(buffer, '\0', 1024);
                                        yellow();
                                        printf("set new pseudo %s for socket %d \n", current_client->pseudo,
                                               current_client->sockfd);
                                        reset();
                                        snprintf(buffer, sizeof(buffer), "your new pseudo is %s",
                                                 current_client->pseudo);
                                        if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                            stop("send");
                                        }
                                    }
                                } else {
                                    if (checkIfPseudoExistInRegisterFile(nickname) == -1) {
                                        char *checkedPseudo = calloc(sizeof(char), 32);
                                        checkedPseudo = checkerRegisterFile(nickname, password);
                                        if (checkedPseudo != NULL) {
                                            disconnectClient(checkedPseudo);
                                            strncpy(current_client->pseudo, checkedPseudo, 32);
                                            memset(buffer, '\0', 1024);
                                            yellow();
                                            printf("set new pseudo %s for socket %d \n", current_client->pseudo,
                                                   current_client->sockfd);
                                            reset();
                                            snprintf(buffer, sizeof(buffer), "your new pseudo is %s",
                                                     current_client->pseudo);
                                            if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                                stop("send");
                                            }
                                        } else {
                                            memset(buffer, '\0', 1024);
                                            snprintf(buffer, sizeof(buffer),
                                                     "Sorry, error on set registered pseudo please retry /nickname pseudo password :");
                                            if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                                stop("send");
                                            }
                                        }
                                    } else {
                                        memset(buffer, '\0', 1024);
                                        snprintf(buffer, sizeof(buffer),
                                                 "Sorry, pseudo is not registered, please retry /nickname pseudo password :");
                                        if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                            stop("send");
                                        }
                                    }

                                }

                            }
                        }

                        /* Date Command */
                        if (tbuf != NULL && !strcmp(tbuf, "/date")) {
                            time_t timestamp = time(NULL);
                            char time_string[32];
                            struct tm *timeinfo = localtime(&timestamp);
                            strftime(time_string, sizeof(time_string), "%X", timeinfo);
                            snprintf(buffer, 1024, "Time on the server : %s", time_string);
                            if (send(current_client->sockfd, buffer, strlen(buffer), 0) < 0) {
                                stop("send");
                            }
                        }

                        /* EXIT Command */
                        if (tbuf != NULL && !strcmp(tbuf, "/exit")) {
                            snprintf(buffer, sizeof(buffer), "You are now disconnected !");
                            if (send(current_client->sockfd, buffer, strlen(buffer), 0) < 0) {
                                stop("send");
                            }
                            yellow();
                            printf("%s with socket %d is disconnected\n", current_client->pseudo,
                                   current_client->sockfd);
                            reset();
                            close(current_client->sockfd);
                            current_client->sockfd = -1;
                        }
                        /* Register Command */
                        if (tbuf != NULL && !strcmp(tbuf, "/register")) {
                            char registerPseudo[32] = {0};
                            char registerPassword[256] = {0};
                            char errorMessage[256] = "You don't have to enter a ";
                            tbuf = strtok(NULL, " ");
                            if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                strncpy(registerPseudo, tbuf, sizeof(registerPseudo) - 1);
                            } else {
                                strcat(errorMessage, "pseudo in your command, please retry");
                                if (send(current_client->sockfd, errorMessage, sizeof(errorMessage), 0) < 0) {
                                    stop("send");
                                }
                            }
                            if (strcmp(registerPseudo, "") != 0) {
                                tbuf = strtok(NULL, "\n");
                                if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                    strncpy(registerPassword, tbuf, sizeof(registerPassword) - 1);
                                } else {
                                    strcpy(errorMessage,
                                           "You don't have to enter a password in your command, please retry");
                                    if (send(current_client->sockfd, errorMessage, sizeof(errorMessage), 0) < 0) {
                                        stop("send");
                                    }
                                }
                                if (strcmp(registerPassword, "") != 0) {
                                    FILE *fileWrite = fopen("login.dat", "ab");
                                    if (fileWrite == NULL) {
                                        printf("Error opening file\n");
                                    } else {
                                        fwrite(registerPseudo, sizeof(char), sizeof(registerPseudo), fileWrite);
                                        fwrite(registerPassword, sizeof(char), sizeof(registerPassword), fileWrite);
                                        snprintf(buffer, sizeof(buffer), "Pseudo registered");
                                        if (send(current_client->sockfd, buffer, sizeof(buffer), 0) < 0) {
                                            stop("send");
                                        }
                                        fclose(fileWrite);
                                    }
                                }
                            }
                        }

                        /* Mp Command */
                        if (tbuf != NULL && !strcmp(tbuf, "/mp")) {
                            char privateDestName[32] = {0};
                            char privateDestMessage[256] = {0};
                            char errorMessage[256] = {0};
                            tbuf = strtok(NULL, " ");
                            if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                strncpy(privateDestName, tbuf, 32);
                            }
                            if (strcmp(privateDestName, "") == 0) {
                                snprintf(errorMessage, 256, "You dont have enter a name in your command, please retry");
                                if (send(current_client->sockfd, errorMessage, 256, 0) < 0) {
                                    stop("send");
                                }
                            } else {
                                tbuf = strtok(NULL, "\n");
                                if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                    strncpy(privateDestMessage, tbuf, 256);
                                }
                                memset(errorMessage, '\0', 256);
                                if (strcmp(privateDestMessage, "") == 0) {
                                    snprintf(errorMessage, 256,
                                             "You dont have enter message in your command, please retry");
                                    if (send(current_client->sockfd, errorMessage, 256, 0) < 0) {
                                        stop("send");
                                    }
                                } else {
                                    if (checkIfPseudoExistInLC(privateDestName) == -1) {
                                        memset(buffer, '\0', 1024);
                                        snprintf(buffer, 1024, "private message from %s : %s", current_client->pseudo,
                                                 privateDestMessage);
                                        sendPrivateMessage(buffer, privateDestName);
                                    } else {
                                        snprintf(buffer, 1024, "Pseudo not found, please retry !");
                                        if (send(current_client->sockfd, buffer, 1024, 0) < 0) {
                                            stop("send");
                                        }
                                    }
                                }
                            }
                        }
                        /* Alert Command  */
                        if (tbuf != NULL && !strcmp(tbuf, "/alerte")) {
                            char pseudoOrMessage[256] = {0};
                            tbuf = strtok(NULL, " ");
                            if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                strncpy(pseudoOrMessage, tbuf, 256);
                                if (checkIfPseudoExistInLC(pseudoOrMessage) == -1) {
                                    tbuf = strtok(NULL, "\n");
                                    if (tbuf != NULL && strcmp(tbuf, "") != 0) {
                                        snprintf(tempbuff, sizeof(buffer), "/alerte private message from %s : %s",
                                                 current_client->pseudo, tbuf);
                                        sendPrivateMessage(tempbuff, pseudoOrMessage);
                                    } else {
                                        snprintf(buffer, sizeof(buffer), "need to specify message, please retry");
                                        send(current_client->sockfd, buffer, sizeof(buffer), 0);
                                    }
                                } else {
                                    tbuf = strtok(NULL, "\n");
                                    if (tbuf != NULL && tbuf[0] != '\0') {
                                        snprintf(buffer, sizeof(buffer), "/alerte %s %s", pseudoOrMessage, tbuf);
                                    } else {
                                        snprintf(buffer, sizeof(buffer), "/alerte %s", pseudoOrMessage);
                                    }
                                    SendToAllClients(buffer, current_client->sockfd);
                                }
                            } else {
                                snprintf(buffer, sizeof(buffer), "need to specify, user or message, please retry");
                                send(current_client->sockfd, buffer, sizeof(buffer), 0);
                            }
                        }

                        /* Send Command  */
                        if (tbufSend != NULL && !strcmp(tbufSend, "/send")) {
                            char pseudo[32] = {0};
                            char fileContainer[256] = {0};
                            char pathFile[32] = {0};
                            tbufSend = strtok(NULL, " ");
                            if (tbufSend != NULL && strcmp(tbufSend, "") != 0) {
                                strncpy(pseudo, tbufSend, 32);
                                if (checkIfPseudoExistInLC(pseudo) == -1) {
                                    tbufSend = strtok(NULL, " ");
                                    if (tbufSend != NULL && strcmp(tbufSend, "") != 0) {
                                        strncpy(pathFile, tbufSend, 32);
                                        tbufSend = strtok(NULL,"\n"); //delim
                                        if(tbufSend != NULL && strcmp(tbuf,"") != 0){
                                            strncpy(fileContainer,tbufSend,256);
                                        }
                                        memset(tempbuff,'\0',1024+256+32);
                                        snprintf(tempbuff, sizeof(tempbuff), "/send %s %s", pathFile,fileContainer);
                                        sendPrivateMessage(tempbuff, pseudo);
                                    }
                                } else {
                                    snprintf(buffer, 1024, "Pseudo not found, please retry !");
                                    send(current_client->sockfd, buffer, sizeof(buffer), 0);
                                }
                            }
                        }


                    } else {
                        /* SendAll */
                        memset(tempbuff, 0, sizeof(tempbuff));
                        snprintf(tempbuff, sizeof(tempbuff), "%s : %s", current_client->pseudo, buffer);
                        SendToAllClients(tempbuff, current_client->sockfd);
                    }
                }
            }

            current_node = (node_t *) current_node->next;
        }

    }

    return 0;

}



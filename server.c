/*
** server.c -- a stream socket server demo
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT "9999"
#define BACKLOG 10

struct addrinfo hints, *server_info, *p;
struct sockaddr_storage their_addr;
char buffer[1024];
int sockfd;
int yes = 1;
char s[INET6_ADDRSTRLEN];
int rv;
pthread_t client_thread;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *handler_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);

    while (1) {
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("server: client %s disconnected\n", s);
            } else {
                perror("recv");
            }
            close(client_fd);
            pthread_exit(NULL);
        }

        buffer[bytes_received] = '\0';
        printf("Client %s: %s\n", s, buffer);

        if (strcmp(buffer, "exit") == 0) {
            printf("server: client %s requested disconnection\n", s);
            close(client_fd);
            pthread_exit(NULL);
        }

        if (send(client_fd, buffer, bytes_received, 0) == -1) {
            perror("send");
        }
    }
}

int main(void) {
    // Set up the struct hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = server_info; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(server_info);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections on port %s...\n", PORT);

    while (1) {
        socklen_t sin_size = sizeof their_addr;
        int *new_fd = malloc(sizeof(int));
        if (new_fd == NULL) {
            perror("malloc");
            exit(1);
        }

        *new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (*new_fd == -1) {
            perror("accept");
            free(new_fd);
            continue;
        }

        if (pthread_create(&client_thread, NULL, handler_client, new_fd) != 0) {
            perror("pthread_create");
            close(*new_fd);
            free(new_fd);
            continue;
        }

        pthread_detach(client_thread); // Detach the thread to clean up automatically
    }

    return 0;
}

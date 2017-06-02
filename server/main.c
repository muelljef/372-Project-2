#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netdb.h>

#define BSIZE 4096

int startUp(int argc, char *argv[]);
void error(const char *msg);
int contactClientDataSocket(uint16_t portno);
void handleRequest(int connectionSocketFd);

int main(int argc, char *argv[]) {
    int sockfd, connectionSocketFd;

    sockfd = startUp(argc, argv);
    //SERVER LOOP
    //infinite loop waiting for clients
    while(1)
    {
        //getting new clients
        connectionSocketFd = accept(sockfd, NULL, NULL);
        if (connectionSocketFd < 0) {
            // TODO: what to do here? is this correct?
            perror("accept");
            continue;
        }
        // TODO: add host to message if possible, maybe move this to
        printf("Connection with client\n");

        // TODO: handle the response
        handleRequest(connectionSocketFd);
        close(connectionSocketFd);
        break;
    }

    close(sockfd);
    return 0;
}

void handleRequest(int connectionSocketFd)
{
    char buffer[BSIZE];
    uint16_t portno;
    int dataSockFd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    memset(buffer, '\0', BSIZE);
    read(connectionSocketFd, buffer, 2);

    // Sending files
//    if (strncmp(buffer, "-g", 2) == 0) {
//        write(connectionSocketFd, "1", 1);
//        // TODO: implement action
//        memset(buffer, '\0', BSIZE);
//        read(connectionSocketFd, buffer, 2);
//        return;
//    }

    // Listing files
    if (strncmp(buffer, "-l", 2) == 0) {
        // indicate successful command
        write(connectionSocketFd, "1", 1);

        // read the port number to connect on
        memset(buffer, '\0', BSIZE);
        read(connectionSocketFd, buffer, BSIZE - 1);
        portno = (uint16_t) atoi(buffer);
        if (portno == 0) {
            fprintf(stderr, "ERROR, invalid port number, %u\n", portno);
            exit(1);
        }
        printf("client data port number: %u\n", portno);
        write(connectionSocketFd, "1", 1);

        // Send message to client, this could be files
        dataSockFd = contactClientDataSocket(portno);
        write(dataSockFd, "Blah, blah, blah", strlen("Blah, blah, blah"));

        // close the connection
        close(dataSockFd);
        return;
    }

    write(connectionSocketFd, "0", 1);
    return;
}

int startUp(int argc, char *argv[])
{
    uint16_t portno;
    int sockfd;
    struct sockaddr_in serv_addr;

    //Checking enough arguments given
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    } else if (argc > 2) {
        fprintf(stderr,"ERROR, too many arguments\n");
        exit(1);
    }

    // Convert and check that the port number given is in the valid range
    portno = (uint16_t) atoi(argv[1]);
    if (portno == 0) {
        fprintf(stderr, "ERROR, invalid port number, %u\n", portno);
        exit(1);
    }

    //Creating the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    //Setting the server address to accept any clients
    memset((char *) &serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    //Binding the socket
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }

    //Listening on the socket
    listen(sockfd, 1);
    printf("Server open on %u\n", portno);

    return sockfd;
}

int contactClientDataSocket(uint16_t portno)
{
    int dataSockFd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    // Create the socket
    dataSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSockFd < 0) {
        fprintf(stderr, "Error opening socket");
        exit(1);
    }

    // Get server details
    // TODO: have host send this info and use it
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    //Setting server details in struct serv_addr
    memset((char *) &serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    //Connect to the socket
    if (connect(dataSockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Could not connect to port %u\n", portno);
        exit(1);
    }

    return dataSockFd;
}

//From TLPI
//entry: message to display with perror
//exit: will display perror message then exit with status 1
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

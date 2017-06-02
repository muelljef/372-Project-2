#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BSIZE 4096

int startUp(int argc, char *argv[]);
void error(const char *msg);
int contactClientDataSocket(char *hostname, uint16_t portno);
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
    char buffer[BSIZE], hostname[BSIZE];
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

        // read the host name
        memset(hostname, '\0', BSIZE);
        read(connectionSocketFd, hostname, BSIZE - 1);
        if (strlen(hostname) < 1) {
            fprintf(stderr, "ERROR, no host name given, %s\n", hostname);
            write(connectionSocketFd, "0", 1);
            return;
        }
        printf("client host name: %s\n", hostname);
        // indicate successfully retrieved hostname
        write(connectionSocketFd, "1", 1);

        // read the port number to connect on
        memset(buffer, '\0', BSIZE);
        read(connectionSocketFd, buffer, BSIZE - 1);
        portno = (uint16_t) atoi(buffer);
        if (portno == 0) {
            fprintf(stderr, "ERROR, invalid port number, %u\n", portno);
            write(connectionSocketFd, "0", 1);
            return;
        }
        printf("client data port number: %u\n", portno);
        // indicate successful port number
        write(connectionSocketFd, "1", 1);

        // Send message to client, this could be files
        dataSockFd = contactClientDataSocket(hostname, portno);
        if (dataSockFd < 0) {
            fprintf(stderr, "ERROR, could not connect to client data socket, %u\n", portno);
            write(connectionSocketFd, "0", 1);
            return;
        }

        // Write files to socket
        // source for code
        // https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
        // TODO: make sure cannot do buffer overflow
        memset(buffer, '\0', BSIZE);
        DIR *directory;
        struct dirent *entry;
        directory = opendir("./");
        if (directory != NULL) {
            while (entry = readdir(directory)) {
                strcat(buffer, entry->d_name);
                strcat(buffer, "\n");
            }
            closedir(directory);
        }

        write(dataSockFd, buffer, strlen(buffer));

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

int contactClientDataSocket(char *hostname, uint16_t portno)
{
    int dataSockFd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    // Create the socket
    dataSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSockFd < 0) {
        fprintf(stderr, "Error opening socket");
        return -1;
    }

    // Get server details
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        return -1;
    }

    //Setting server details in struct serv_addr
    memset((char *) &serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    //Connect to the socket
    if (connect(dataSockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Could not connect to port %u\n", portno);
        return -1;
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

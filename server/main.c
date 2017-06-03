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
#define CHUNK_SIZE 512

int startUp(int argc, char *argv[]);
void error(const char *msg);
int contactClientDataSocket(char *hostname, uint16_t portno);
void handleRequest(int connectionSocketFd);
void writeSocketError(int connectionSocketFd, char *msg);

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
            perror("accept");
            continue;
        }

        handleRequest(connectionSocketFd);
        close(connectionSocketFd);
        // TODO: add SIGINT handler above to close the sockfd
//        close(sockfd);
//        return 0;
    }
}

void handleRequest(int connectionSocketFd)
{
    char buffer[BSIZE], clientHostname[BSIZE], filename[BSIZE];
    uint16_t portno;
    int dataSockFd;
    char fileChunk[CHUNK_SIZE];
    size_t numBytes = 0;
    ssize_t sentBytes = 0;
    int offset = 0;

    // read the host name
    memset(clientHostname, '\0', BSIZE);
    read(connectionSocketFd, clientHostname, BSIZE - 1);
    if (strlen(clientHostname) < 1) {
        fprintf(stderr, "ERROR, no host name given, %s\n", clientHostname);
        write(connectionSocketFd, "NO HOST NAME GIVEN", strlen("NO HOST NAME GIVEN"));
        return;
    }
    printf("Connection from %s\n", clientHostname);
    // indicate successfully retrieved hostname
    write(connectionSocketFd, "1", 1);

    // read the port number to connect on
    memset(buffer, '\0', BSIZE);
    read(connectionSocketFd, buffer, BSIZE - 1);
    portno = (uint16_t) atoi(buffer);
    if (portno == 0) {
        fprintf(stderr, "ERROR, invalid port number, %u\n", portno);
        write(connectionSocketFd, "INVALID PORT NUMBER", strlen("INVALID PORT NUMBER"));
        return;
    }
    // indicate successful port number
    write(connectionSocketFd, "1", 1);

    // read the command
    memset(buffer, '\0', BSIZE);
    read(connectionSocketFd, buffer, 2);
    // Sending files
    if (strncmp(buffer, "-g", 2) == 0) {
        // indicate the command is recognized
        write(connectionSocketFd, "1", 1);

        // read the filename
        memset(filename, '\0', BSIZE);
        read(connectionSocketFd, filename, BSIZE - 1);
        if (strlen(filename) < 1) {
            fprintf(stderr, "ERROR, no file name given, %s\n", filename);
            write(connectionSocketFd, "0", 1);
            return;
        }
        // indicate successfully retrieved filename
        printf("File \"%s\" requested on port %u\n", filename, portno);

        FILE *sendfile;
        sendfile = fopen(filename, "r");
        // referenced linux man pages for fopen()
        // http://man7.org/linux/man-pages/man3/fopen.3.html
        if (sendfile == NULL) {
            fprintf(stderr, "File not found. Sending error message to %s:%u\n", clientHostname, portno);
//            write(connectionSocketFd, "FILE NOT FOUND", strlen("FILE NOT FOUND"));
            writeSocketError(connectionSocketFd, "FILE NOT FOUND");
            return;
        } else {
            write(connectionSocketFd, "1", 1);
        }

        // Connect to the client and send
        dataSockFd = contactClientDataSocket(clientHostname, portno);
        if (dataSockFd < 0) {
            fprintf(stderr, "ERROR, could not connect to client data socket, %u\n", portno);
            write(connectionSocketFd, "0", 1);
            return;
        }

        // referenced the following for sending files
        // https://stackoverflow.com/questions/5594042/c-send-file-to-socket
        // send the file
        printf("Sending \"%s\" to %s:%u\n", filename, clientHostname, portno);
        while ((numBytes = fread(fileChunk, sizeof(char), CHUNK_SIZE, sendfile)) > 0) {
            offset = 0;
            while((sentBytes = send(dataSockFd, fileChunk + offset, numBytes, 0)) > 0) {
                offset += sentBytes;
                numBytes -= sentBytes;
            }
            memset(fileChunk, '\0', BSIZE);
        }

        // close the connection
        close(dataSockFd);
        return;
    }

    // Listing files
    if (strncmp(buffer, "-l", 2) == 0) {
        printf("List directory requested on port %u\n", portno);
        // indicate successful command
        write(connectionSocketFd, "1", 1);

        // Send message to client, this could be files
        dataSockFd = contactClientDataSocket(clientHostname, portno);
        if (dataSockFd < 0) {
            fprintf(stderr, "ERROR, could not connect to client data socket, %u\n", portno);
            write(connectionSocketFd, "0", 1);
            return;
        }

        // Write directory filenames to socket
        // source for reading directory from the following link
        // https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
        printf("Sending directory contents to %s:%u\n", clientHostname, portno);
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

    write(connectionSocketFd, "INVALID COMMAND", strlen("INVALID COMMAND"));
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
    sleep(1);
    if (connect(dataSockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Client Data Socket");
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

void writeSocketError(int connectionSocketFd, char *msg)
{
    write(connectionSocketFd, msg, strlen(msg));
}

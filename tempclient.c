#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern int h_errno; 

struct hostent *gethostbyname(const char *name);

int main(int argc, char **argv) {
    char *hostName = argv[1];
    char *nickname = argv[2];
    char c;
    char buffer[255] = "";


    struct sockaddr_in serverAddress;
    struct hostent *serverInfo = gethostbyname(hostName);
    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2727);

    bcopy((char *) serverInfo->h_addr, 
        (char *)&serverAddress.sin_addr.s_addr, serverInfo->h_length);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress));

    write(sockfd, nickname, strlen(nickname));

    int i = 0;
    while(read(sockfd, &c, 1)){
        //printf("%c",c);
        buffer[i++] = c;
    }

    printf("%.*s\n", i-1, buffer);

    close(sockfd);
    printf("\n");

    return 0;
}
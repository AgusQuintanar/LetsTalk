#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

//Compile and link with -pthread

typedef struct td {
	int hid;
	char *nick;
} ThreadData;

void * listenClient(void *arg) {
	ThreadData *td = (ThreadData *)arg;
	printf("Se ha conectado: %s. ID: %d\n", td->nick,td->hid);
	free(arg);
	pthread_exit(NULL);
}

int main() {
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2727);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bind(sockfd, (const struct sockaddr *)&serverAddress, sizeof(serverAddress));
    listen(sockfd,5);

    int currentId = 1;

    while(1){
        struct sockaddr_in clientAddress;
        int clientSize = sizeof(clientAddress);
        int clientSocket = accept(sockfd, (struct sockaddr *)&clientAddress, (unsigned int *)&clientSize);

        printf("se conecto \n");


        char buffer[255] = "";

        char c;
        int i = 0;

        while(read(clientSocket,&c,1)){
           //printf("%c",c);
           buffer[i++] = c;
           //write(clientSocket,&c,1);
        }
        buffer[i] = '\0';

        printf("antes \n");
        strcat(buffer, " conectado");
        printf("desoues \n");        
        printf("%s", buffer);
        // if (strcmp(buffer, "bye") == 0) {

        // }
        
        char *message = "holi";


        pthread_t newThread;
	
        ThreadData *td = (ThreadData *)calloc(1,sizeof(ThreadData));
        td->hid = currentId++;
        td->nick = "Agus";
        pthread_create(&newThread, NULL, listenClient, (void *)td);


        printf("Cliente conectado: %d \n", td->hid);
        write(clientSocket,message,strlen(message));
        //close(clientSocket);
    }
}
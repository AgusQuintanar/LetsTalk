#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 250

// Variables globales
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[20];

/* Sobreescribir salida estándar */
void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

/* Quitar saltos de línea de un arreglo de caracteres*/
void str_trim_lf (char* arr, int length) {
    for (int i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

/* Atrapar Ctrl+C y salir*/
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

/* Función de thread para enviar */
void send_msg_handler() {
    char message[LENGTH] = {};
	char buffer[LENGTH + 20] = {}; // Inicializar el buffer con el tamaño del nombre + mensaje 

    while(1) {
  	    str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "bye") == 0) { // Si el mensaje es bye, se termina el thread
			break;
        } 
        else {
            sprintf(buffer, "%s> %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }
        
		bzero(message, LENGTH); // Vaciar mensaje 
        bzero(buffer, LENGTH + 20); // Vaciar buffer
    }
  catch_ctrl_c_and_exit(2);
}

/* Función de thread para recibir */
void recv_msg_handler() {
	char message[LENGTH] = {};
    while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            printf("%s", message);
            str_overwrite_stdout();
        } 
        else if (receive == 0) { // Si no se recibe nada, no se imprime
			break;
        } 
		memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Uso: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1"; // IP de host
	int port = atoi(argv[1]); // Puerto

	signal(SIGINT, catch_ctrl_c_and_exit); // Asignar función de salida a Ctrl+C

	printf("Ingresa tu nombre: ");
    fgets(name, 20, stdin);
    str_trim_lf(name, strlen(name)); // Quitar salto de línea a nombre

    /* Validar longitu del nombre */
    if (strlen(name) > 20 || strlen(name) < 2){
		printf("Nombre tiene que tener entre 2 20 caracteres.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Configurar socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    /* Conectar al servidor */
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
		printf("[Cliente]: ERROR conexión\n");
		return EXIT_FAILURE;
	}

	/* Enviar nombre a servidor*/
	send(sockfd, name, 20, 0);

	printf("* Bienvenido al chat, %s *\n",name);

    /* Crear hilo para enviar mensaje */
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("[Cliente]: ERROR hilo\n");
        return EXIT_FAILURE;
	}

    /* Crear hilo para recibir mensaje */
    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("[Cliente]: ERROR hilo\n");
		return EXIT_FAILURE;
	}

    while (1){
		if(flag){
			printf("\nBye\n");
			break;
        }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
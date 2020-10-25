// El siguiente codigo se realizo con la guia del siguiente tutorial: https://www.youtube.com/watch?v=fNerEo6Lstw&feature=youtu.be

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

#define BUFFER_SZ 250

// Variables globales
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char nombre[20];

// Quitar saltos de línea de un arreglo de caracteres
void quitar_salto_linea(char* arr, int length) {
    for (int i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

/* Atrapar Ctrl+C y salir*/
void salir_ctrl_c(int sig) {
    flag = 1;
}

/* Función de thread para enviar */
void enviar_mensaje() {
    char mensaje[BUFFER_SZ] = {};
	char buffer[BUFFER_SZ + 20] = {}; // Inicializar el buffer con el tamaño del nombre + mensaje 

    while(1) {
  	    fflush(stdout);
        fgets(mensaje, BUFFER_SZ, stdin);
        quitar_salto_linea(mensaje, BUFFER_SZ);

        if (strcmp(mensaje, "bye") == 0) { // Si el mensaje es bye, se termina el thread
			break;
        } 
        else {
            sprintf(buffer, "%s: %s\n", nombre, mensaje);
            send(sockfd, buffer, strlen(buffer), 0);
        }
        
		bzero(mensaje, BUFFER_SZ); // Vaciar mensaje 
        bzero(buffer, BUFFER_SZ + 20); // Vaciar buffer
    }
  salir_ctrl_c(2);
}

/* Función de thread para recibir */
void recibir_mensaje() {
	char mensaje[BUFFER_SZ] = {};
    while (1) {
		int receive = recv(sockfd, mensaje, BUFFER_SZ, 0);
        if (receive > 0) {
            if (strcmp(mensaje, "Bye desde el server.") == 0) { // Si el mensaje es bye, se termina el thread
                flag = 2;
                break;
            }
            else if (strcmp(mensaje, "Servidor lleno.") == 0) { // Si el mensaje es bye, se termina el thread
                flag = 3;
                break;
            }
            printf("> %s", mensaje);
            fflush(stdout);
        } 
        else if (receive == 0) { // Si no se recibe nada, no se imprime
			break;
        } 
		memset(mensaje, 0, sizeof(mensaje));
    }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Uso: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1"; // IP de host
	int port = atoi(argv[1]); // Puerto

	signal(SIGINT, salir_ctrl_c); // Asignar función de salida a Ctrl+C

	printf("Ingresa tu nombre: ");
    fgets(nombre, 20, stdin);
    quitar_salto_linea(nombre, strlen(nombre)); // Quitar salto de línea a nombre

    /* Validar longitu del nombre */
    if (strlen(nombre) > 20 || strlen(nombre) < 2){
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
		printf("[Cliente]: ERROR conexión.\n");
		return EXIT_FAILURE;
    }

	/* Enviar nombre a servidor*/
	send(sockfd, nombre, 20, 0);

    /* Crear hilo para enviar mensaje */
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *) enviar_mensaje, NULL) != 0){
		printf("[Cliente]: ERROR hilo.\n");
        return EXIT_FAILURE;
	}

    /* Crear hilo para recibir mensaje */
    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *) recibir_mensaje, NULL) != 0){
		printf("[Cliente]: ERROR hilo.\n");
		return EXIT_FAILURE;
	}

    while (1){
		if(flag == 1){
			printf("\nSesión finalizada.\n");
			break;
        }
        else if (flag == 2) {
			printf("\nBye desde el server.\n");
            break;
        }
        else if (flag == 3) {
			printf("\nServidor lleno. Vuelva más tarde.\n");
            break;
        }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
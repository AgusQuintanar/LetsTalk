#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 5 // Número máximo de clientes
#define BUFFER_SZ 250 // Tamaño del buffer

volatile sig_atomic_t flag = 0;
static _Atomic unsigned int cli_count = 0; // Contador de clientes activos (el tipo _Atomic previene data races)
static int uid = 10;

/* Estructura de cliente */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[20];
} client_t;

client_t *clients[MAX_CLIENTS]; // Lista enlazada de clientes

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Sobreescribir salida estándar */
void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

/* Quitar saltos de línea de un arreglo de caracteres*/
void str_trim_lf (char* arr, int length) {
    // printf("%d", length);
    for (int i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

/* Añadir cliente nuevo a la lista de activos */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex); // Proteger la lista temporalmente

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex); // Desbloquear la lista
}

/* Quitar clientes del arreglo de activos */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Quitar clientes del arreglo de activos */
void queue_flush(){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			clients[i] = NULL;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void print_client_addr(struct sockaddr_in addr){
    printf(
        "%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24
    );
}

/* Enviar mensaje a todos los clientes excepto al remitente */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for (int i=0; i<MAX_CLIENTS; ++i){
		if (clients[i]){
			if (clients[i]->uid != uid){
				if (write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("[SERVER]: ERROR al mandar mensajes");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void catch_ctrl_c_and_exit(int sig) {
    send_message("Bye desde el server",-1);
    flag = 1;
    // queue_flush();
    kill(0, SIGKILL);
}

/* Manejar la comunicación con el cliente */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ]; // Buffer que recibirá un nombre
	char name[20];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Nombre del cliente 
	if (recv(cli->sockfd, name, 20, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 20-1){
		printf("Nombre faltante.\n");
		leave_flag = 1;
	} 
    else {
		strcpy(cli->name, name);
        // str_trim_lf(name, strlen(name));
		sprintf(buff_out, "%s conectado\n", name);
		printf("> %s", buff_out);
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ); // Vaciar el buffer

    while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){ // Mandar un mensaje a los clientes
			if(strlen(buff_out) > 0){
				send_message(buff_out, cli->uid);

				str_trim_lf(buff_out, strlen(buff_out));
				printf("> %s \n", buff_out);
			}
		} 
        else if (receive == 0 || strcmp(buff_out, "bye") == 0){ // Verificar si el mensaje es bye
			sprintf(buff_out, "%s ha abandonado el chat.\n", cli->name);
			printf("> %s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} 
        else {
			printf("[SERVER]: ERROR -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

    /* Eliminar cliente de lista, terminar thread y limpiar */
	close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

	return NULL;
}

    
int main(int argc, char **argv){
    
    /* Verificar de los parámetros de entrada */
	if(argc != 2){
		printf("Uso: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1"; // IP de host
	int port = atoi(argv[1]); // Puerto

    int option = 1;
	int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Configurar socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip); // Asociar IP a socket
    serv_addr.sin_port = htons(port); // Asociar puerto a socket

    /* Atrapar señal para terminar proceso */
	//signal(SIGPIPE, catch_ctrl_c_and_exit);
    signal(SIGINT, catch_ctrl_c_and_exit);


    /* Verificar entrada del socket */
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0){
		perror("[SERVER]: ERROR en setsockopt");
        return EXIT_FAILURE;
	}

    /* Verificar Bind */
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[SERVER]: ERROR en binding");
        return EXIT_FAILURE;
    }

    /* Verificar Listen */
    if (listen(listenfd, 10) < 0) {
        perror("[SERVER]: ERROR al escuchar");
        return EXIT_FAILURE;
	}

    printf("* Servidor inicializado *\n");

    /* Inicio del chatroom */
    while(1) { 
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen); // Aceptar la conexión

		/* Verificar número máximo de clientes */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("[SERVER]: Límite de clientes alcanzado. Rechazado: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

        /* Configuración de cliente */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Agregar cliente al arreglo y hacer fork al hilo*/
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduccion de uso de CPU */
		sleep(1);

		if(flag){
			break;
        }
    }

    return EXIT_SUCCESS; 
}
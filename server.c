// El siguiente codigo se realizo con la guia del siguiente tutorial: https://www.youtube.com/watch?v=fNerEo6Lstw&feature=youtu.be

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

#define NUM_CLIENTES_MAX 5 // Número máximo de clientes
#define BUFFER_SZ 250 // Tamaño del buffer

volatile sig_atomic_t flag = 0;
static _Atomic unsigned int num_clientes = 0; // Contador de clientes activos (el tipo _Atomic previene data races)
static int id = 10;

/* Estructura de cliente */
typedef struct{
	struct sockaddr_in direccion_ip;
	int sockfd;
	int id;
	char nombre[20];
} cliente;

cliente *clientes[NUM_CLIENTES_MAX]; // Lista enlazada de clientes

pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Sobreescribir salida estándar */
void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

/* Quitar saltos de línea de un arreglo de caracteres*/
void quitar_salto_linea(char* arr, int length) {
    for (int i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

/* Añadir cliente nuevo a la lista de activos */
void agregar_cliente(cliente *cl){
	pthread_mutex_lock(&clientes_mutex); // Proteger la lista temporalmente

	for(int i=0; i < NUM_CLIENTES_MAX; ++i){
		if(!clientes[i]){
			clientes[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clientes_mutex); // Desbloquear la lista
}

/* Quitar clientes del arreglo de activos */
void eliminar_cliente(int id){
	pthread_mutex_lock(&clientes_mutex);

	for(int i=0; i < NUM_CLIENTES_MAX; ++i){
		if(clientes[i]){
			if(clientes[i]->id == id){
				clientes[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clientes_mutex);
}

void imprimir_ip(struct sockaddr_in addr){
    printf(
        "%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24
    );
}

/* Enviar mensaje a todos los clientes excepto al remitente */
void mandar_mensaje(char *s, int id){
	pthread_mutex_lock(&clientes_mutex);

	for (int i=0; i<NUM_CLIENTES_MAX; ++i){
		if (clientes[i]){
			if (clientes[i]->id != id){
				if (write(clientes[i]->sockfd, s, strlen(s)) < 0){
					perror("[SERVER]: ERROR al mandar mensajes.");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clientes_mutex);
}

void salir_ctrl_c(int sig) {
    mandar_mensaje("Bye desde el server.",-1);
    flag = 1;
    kill(0, SIGKILL);
}

/* Manejar la comunicación con el cliente */
void *handle_client(void *arg){
	char buffer[BUFFER_SZ]; // Buffer que recibirá un nombre
	char nombre[20];
	int flag_terminar = 0;

	num_clientes++;
	cliente *cli = (cliente *)arg;

	// Nombre del cliente 
	if (recv(cli->sockfd, nombre, 20, 0) <= 0 || strlen(nombre) <  2 || strlen(nombre) >= 20-1){
		printf("Nombre faltante.\n");
		flag_terminar = 1;
	} 
    else {
		strcpy(cli->nombre, nombre);
		sprintf(buffer, "%s se ha conectado.\n", nombre);
		printf("> %s", buffer);
        char msgBienvenida[50];

        sprintf(msgBienvenida, "Bievenido al server, %s.\n", nombre);

        write(cli->sockfd, msgBienvenida, strlen(msgBienvenida));
		mandar_mensaje(buffer, cli->id);
	}


	bzero(buffer, BUFFER_SZ); // Vaciar el buffer

    while(1){
		if (flag_terminar) {
			break;
		}

		int recibido = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
		if (recibido > 0){ // Mandar un mensaje a los clientes
			if(strlen(buffer) > 0){
				mandar_mensaje(buffer, cli->id);

				quitar_salto_linea(buffer, strlen(buffer));
				printf("> %s \n", buffer);
			}
		} 
        else if (recibido == 0 || strcmp(buffer, "bye") == 0){ // Verificar si el mensaje es bye
			sprintf(buffer, "%s ha abandonado el chat.\n", cli->nombre);
			printf("> %s", buffer);
			mandar_mensaje(buffer, cli->id);
			flag_terminar = 1;
		} 
        else {
			printf("[SERVER]: ERROR -1\n");
			flag_terminar = 1;
		}

		bzero(buffer, BUFFER_SZ);
	}

    /* Eliminar cliente de lista, terminar thread y limpiar */
	close(cli->sockfd);
    eliminar_cliente(cli->id);
    free(cli);
    num_clientes--;
    pthread_detach(pthread_self());

	return NULL;
}

    
int main(int argc, char **argv){
    
    /* Verificar de los parámetros de entrada */
	if(argc != 2){
		printf("Uso: %s <PUERTO>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1"; // IP de host
	int puerto = atoi(argv[1]); // Puerto

    int opcion = 1;
	int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Configurar socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip); // Asociar IP a socket
    serv_addr.sin_port = htons(puerto); // Asociar puerto a socket

    /* Atrapar señal para terminar proceso */
    signal(SIGINT, salir_ctrl_c);


    /* Verificar entrada del socket */
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opcion, sizeof(opcion)) < 0){
		perror("[SERVER]: ERROR en setsockopt.");
        return EXIT_FAILURE;
	}

    /* Verificar Bind */
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[SERVER]: ERROR en binding.");
        return EXIT_FAILURE;
    }

    /* Verificar Listen */
    if (listen(listenfd, 10) < 0) {
        perror("[SERVER]: ERROR al escuchar.");
        return EXIT_FAILURE;
	}

    printf("* Servidor inicializado *\n");

    /* Inicio del chatroom */
    while(1) { 
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen); // Aceptar la conexión

		/* Verificar número máximo de clientes */
		if((num_clientes + 1) > NUM_CLIENTES_MAX){
            write(connfd, "Servidor lleno.", strlen("Servidor lleno."));
			printf("[SERVER]: Límite de clientes alcanzado. Rechazado: ");
			imprimir_ip(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

        /* Configuración de cliente */
		cliente *cli = (cliente *)malloc(sizeof(cliente));
		cli->direccion_ip = cli_addr;
		cli->sockfd = connfd;
		cli->id = id++;

		/* Agregar cliente a la lista y crear hilo*/
		agregar_cliente(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduccion de uso de CPU */
		sleep(1);

		if(flag){
			break;
        }
    }

    return EXIT_SUCCESS; 
}
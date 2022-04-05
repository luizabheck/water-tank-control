#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 2000

int sendMsgToClient(int socket_desc, char *server_message, struct sockaddr *client_addr);
int receiveMsgFromClient(int socket_desc, char *client_message, struct sockaddr *client_addr);
void messageHandler(char *client_message, char *server_message);
void *serverThreadFunction(void *arg);
void *plantThreadFunction(void *arg);

float water_level = 0.4;

int main(void)
{
    pthread_t server_thread, plant_thread;

    pthread_create(&server_thread, NULL, serverThreadFunction, NULL);
    pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);

    pthread_join(server_thread, NULL);
    pthread_join(plant_thread, NULL);

    return 0;
}

void *plantThreadFunction(void *arg)
{
    while (1)
    {
        printf("alguma coisa\n");
        // do something
        usleep(1000000);
    }
}

void *serverThreadFunction(void *arg)
{
    struct sockaddr_in server_addr, client_addr;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    int socket_desc;

    // Clean buffers:
    // Seta todos os bytes para 0
    memset(server_message, 0, sizeof(server_message));
    memset(client_message, 0, sizeof(client_message));

    // Create UDP socket:
    // AF_INET for IPv4
    // SOCK_DGRAM for UDP
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Qdo dá erro
    if (socket_desc < 0)
    {
        printf("Error while creating socket\n");
        return (void *)(-1);
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BUFFER_SIZE);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind to the set port and IP:
    if (bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Couldn't bind to the port\n");
        return (void *)(-1);
    }
    printf("Done with binding\n");

    printf("Listening for incoming messages...\n\n");

    while (1)
    {
        receiveMsgFromClient(socket_desc, client_message, (struct sockaddr *)&client_addr);

        messageHandler(client_message, server_message);

        sendMsgToClient(socket_desc, server_message, (struct sockaddr *)&client_addr);
    }

    // Close the socket:
    close(socket_desc);
}

void messageHandler(char *client_message, char *server_message)
{
    char *cmd, value_str[5];
    int value;

    if (strstr(client_message, "#") == NULL) // Checks if has not received any argument
    {
        cmd = strtok(client_message, "!"); // Breaks the string to eliminate the '!'
    }
    else // If has arguments
    {
        cmd = strtok(client_message, "#");    // Gets the command related part of message
        strcpy(value_str, strtok(NULL, "!")); // Gets the value part of message
        value = atoi(value_str);              // Converts the received value (string) to integer
    }

    // Respond to client:
    if (strcmp(client_message, "CommTest") == 0)
    {
        strcpy(server_message, "Comm#OK!");
    }
    else if (strcmp(client_message, "Start") == 0)
    {
        strcpy(server_message, "Start#OK!");
        // TODO (Re-)Iniciar o simulador da Planta
    }
    else if (strcmp(client_message, "GetLevel") == 0)
    {
        // TODO Retornar nível atual
        strcpy(server_message, "Level#");
        // strcat(current_level,"!");
        // strcpy(server_message,strcat(server_message,current_level));
    }
    else if (strcmp(client_message, "OpenValve") == 0)
    {
        strcpy(server_message, "Open#");
        strcat(value_str, "!");
        strcpy(server_message, strcat(server_message, value_str));
    }
    else if (strcmp(client_message, "CloseValve") == 0)
    {
        strcpy(server_message, "Close#");
        strcat(value_str, "!");
        strcpy(server_message, strcat(server_message, value_str));
    }
    else if (strcmp(client_message, "SetMax") == 0)
    {
        strcpy(server_message, "Max#");
        strcat(value_str, "!");
        strcpy(server_message, strcat(server_message, value_str));
    }
    else
    {
        strcpy(server_message, "Err!");
    }
}

int receiveMsgFromClient(int socket_desc, char *client_message, struct sockaddr *client_addr)
{
    // sizeof(*server_addr) quero o tamanho do que o ponteiro esta apontando
    int client_struct_length = sizeof(*client_addr);

    // Receive client's message:
    // MSG_WAITALL (since Linux 2.2)
    // This flag requests that the operation block until the full request is satisfied.
    // However, the call may still return less data than requested if a signal is caught,
    // an error or disconnect occurs, or the next data to be received is of a different
    // type than that returned.

    if (recvfrom(socket_desc, client_message, BUFFER_SIZE, 0,
                 client_addr, &client_struct_length) < 0)
    {
        printf("Couldn't receive\n");
        return -1;
    }

    // printf("Received message from IP: %s and port: %i\n",
    //        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    printf("Msg from client: %s\n", client_message);
    return 0;
}

int sendMsgToClient(int socket_desc, char *server_message, struct sockaddr *client_addr)
{
    int client_struct_length = sizeof(*client_addr);

    if (sendto(socket_desc, server_message, strlen(server_message), 0,
               client_addr, client_struct_length) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    }
    return 0;
}

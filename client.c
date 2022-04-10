#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr);
int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr);
void *controlThreadFunction(void *arg);
float executeCommand(char *command, char *expected_response);
int socketConfig();

// TODO usar o tamanho total, multiplicar pelos bytes
#define BUFFER_SIZE 2000

// Global variables
int socket_desc;
struct sockaddr_in server_addr;

int main(void)
{
    pthread_t control_thread;

    socketConfig();

    pthread_create(&control_thread, NULL, controlThreadFunction, NULL);

    pthread_join(control_thread, NULL);

    // Close the socket:
    close(socket_desc);

    return 0;
}

int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr)
{
    int server_struct_length = sizeof(*server_addr);

    if (sendto(socket_desc, client_message, strlen(client_message), 0,
               server_addr, server_struct_length) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    } 
    return 0;
}

int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr)
{
    // sizeof(*server_addr) quero o tamanho do que o ponteiro esta apontando
    int server_struct_length = sizeof(*server_addr);

    // Receive the server's response:
    if (recvfrom(socket_desc, server_message, BUFFER_SIZE, 0,
                 server_addr, &server_struct_length) < 0)
    {
        printf("Error while receiving server's msg\n");
        return -1;
    }
    printf("Server's response: %s\n", server_message);
    return 0;
}

int socketConfig()
{
    // Create socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_desc < 0)
    {
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BUFFER_SIZE);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    return 0;
}

float executeCommand(char *command, char *expected_response)
{

    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    char value_str[BUFFER_SIZE];
    char *response_cmd;
    float value = 0;

    // Clean buffers:
    memset(server_message, 0, sizeof(server_message));
    memset(client_message, 0, sizeof(client_message));

    strcpy(client_message, command);

    // Send the message to server:
    sendMsgToServer(socket_desc, client_message, (struct sockaddr *)&server_addr);

    // Wait for server response:
    receiveMsgFromServer(socket_desc, server_message, (struct sockaddr *)&server_addr);

    if (strstr(server_message, "#") == NULL) // Checks if has not received any argument
    {
        response_cmd = strtok(server_message, "!");
    }
    else // If has arguments
    {
        response_cmd = strtok(server_message, "#"); // Gets the command related part of message
        strcpy(value_str, strtok(NULL, "!"));       // Gets the value part of message
        value = atof(value_str);                    // Converts the received value (string) to integer
    }

    if (strcmp(response_cmd, expected_response) != 0)
    {
        return -1;
    }

    printf("VALUE: %f", value);

    return value;
}

void *controlThreadFunction(void *arg)
{
    float result = 0;

    // Initial commands
    result = executeCommand("CommTest!", "Comm");
    if (result == -1)
    {
        printf("CommTest failed!");
        return (void *)(-1);
    }

    result = executeCommand("SetMax#100!", "Max");
    if (result == -1)
    {
        printf("SetMax failed!");
        return (void *)(-1);
    }

    result = executeCommand("Start!", "Start");
    if (result == -1)
    {
        printf("Start failed!");
        return (void *)(-1);
    }
}

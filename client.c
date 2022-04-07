#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr);
int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr);

// TODO usar o tamanho total, multiplicar pelos bytes
#define BUFFER_SIZE 2000

int main(void)
{
    int socket_desc;
    struct sockaddr_in server_addr;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    int server_struct_length = sizeof(server_addr);

    // Clean buffers:
    memset(server_message, 0, sizeof(server_message));
    memset(client_message, 0, sizeof(client_message));

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

    // Echo:
    printf("Enter message: ");
    gets(client_message);

    // Send the message to server:
    sendMsgToServer(socket_desc, client_message, (struct sockaddr *)&server_addr);

    // Wait for server response:
    receiveMsgFromServer(socket_desc, server_message, (struct sockaddr *)&server_addr);

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

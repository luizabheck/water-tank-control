#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define max(a,b) ((a) > (b) ? (a) : (b))

int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr);
int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr);
void *controlThreadFunction(void *arg);
float executeCommand(char *command, char *expected_response);
int socketConfig();
int control();
float clamp(float value, float min, float max);

// TODO usar o tamanho total, multiplicar pelos bytes
#define BUFFER_SIZE 100

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

    memset(server_message, 0, BUFFER_SIZE * sizeof(char));

    // Receive the server's response:
    if (recvfrom(socket_desc, server_message, BUFFER_SIZE, 0,
                 server_addr, &server_struct_length) < 0)
    {
        printf("Error while receiving server's msg\n");
        return -1;
    }
    // printf("Server's response: %s\n", server_message);
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
    server_addr.sin_port = htons(7000);
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

    return value;
}

void *controlThreadFunction(void *arg)
{
    float result = 0;

    // Initial commands
    result = executeCommand("CommTest!", "Comm");
    if (result == -1)
    {
        printf("CommTest failed!\n");
        return (void *)(-1);
    }

    result = executeCommand("SetMax#100!", "Max");
    if (result == -1)
    {
        printf("SetMax failed!\n");
        return (void *)(-1);
    }

    result = executeCommand("Start!", "Start");
    if (result == -1)
    {
        printf("Start failed!\n");
        return (void *)(-1);
    }

    // Valve Control
    control();
}

float clamp(float value, float min, float max)
{
    if (value >= max)
    {
        return max;
    }
    else if (value <= min)
    {
        return min;
    }

    return value;
}

int control()
{

    float error;
    float previous_error = 0;
    float u;
    float delta_u = 0;
    float P = 0, I = 0, D = 0;
    float previous_D = 0, previous_I = 0;
    float ki = 0.8, kp = 800, kd = 1;
    char command[BUFFER_SIZE];
    float ref = 0.8;
    float dT_s = 0.04;
    float level = 0;
    int valve_position = 50;
    struct timespec start_time, end_time;
    long elapsed_time = 0;
    long variavel_do_danilo = 0;

    float result = 0;

    printf("Control started");

    while (1)
    {

        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

        level = executeCommand("GetLevel!", "Level");
        if (level == -1)
        {
            printf("GetLevel failed!\n");
            return -1;
        }

        error = ref - (level / 100.0);

        I = previous_I + ki * (error + previous_error) * dT_s;

        D = previous_D + kd * (error - previous_error) / dT_s;

        P = kp * error;

        // printf("P: %3.2f\t\tI: %3.2f\t\tD: %3.2f\n", P, I, D);

        u = P + I + D;

        // Control saturation
        u = clamp(u, -95, 95);

        delta_u = u;

        // Saturation
        // delta_u = clamp(delta_u, -10, 10);

        if (delta_u != 0)
        {
            if (delta_u > 0)
            {
                if ((valve_position + (int)delta_u) < 100)
                {
                    sprintf(command, "OpenValve#%d!", (int)round(delta_u));
                    result = executeCommand(command, "Open");
                    if (result == -1)
                    {
                        printf("OpenValve failed!\n");
                        return -1;
                    }
                    valve_position = valve_position + (int)round(delta_u);
                }
                else if (valve_position != 100)
                {
                    delta_u = 100 - valve_position;
                    sprintf(command, "OpenValve#%d!", (int)round(delta_u));
                    result = executeCommand(command, "Open");
                    if (result == -1)
                    {
                        printf("OpenValve failed!\n");
                        return -1;
                    }
                    valve_position = 100;
                }
            }
            else if (delta_u < 0)
            {
                if ((valve_position + (int)delta_u) > 0)
                {
                    sprintf(command, "CloseValve#%d!", -(int)round(delta_u));
                    result = executeCommand(command, "Close");
                if (result == -1)
                    {
                        printf("CloseValve failed!\n");
                        return -1;
                    }
                    valve_position = valve_position + (int)round(delta_u);
                }
                else if (valve_position != 0)
                {
                    delta_u = valve_position;
                    sprintf(command, "CloseValve#%d!", (int)round(delta_u));
                    result = executeCommand(command, "Close");
                if (result == -1)
                    {
                        printf("CloseValve failed!\n");
                        return -1;
                    }
                    valve_position = 0;
                }
            }

            valve_position = clamp(valve_position, 0, 100);

            previous_error = error;
            previous_I = I;
            previous_D = D;

            clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
            elapsed_time = (end_time.tv_nsec - start_time.tv_nsec)/1000;
            variavel_do_danilo = ((dT_s * 1000000)-elapsed_time);

            usleep(max(variavel_do_danilo,0));
        }
    }

    
}
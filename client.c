#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <SDL/SDL.h>

// Macro ///////////////////////////////////
#define max(a, b) ((a) > (b) ? (a) : (b))

// Constants ///////////////////////////////
#define BUFFER_SIZE 100
#define KI 40
#define KD 10
#define KP 900
#define REFERENCE 0.8
#define dT_MS 0.05

// Plot ///////////////////////////////////
#define SCREEN_W 640 
#define SCREEN_H 480
#define BPP 32
typedef Uint32 pixel_t;
////////////////////////////////////////////

// Global variables ///////////////////////
int socket_desc;
struct sockaddr_in server_addr;
float level = 0;
long passed_time_ms = 0;
int valve_position = 50;

// Plot functions /////////////////////////
typedef struct
{
    SDL_Surface *canvas;
    int height;  // canvas height
    int width;   // canvas width
    int Xoffset; // X off set, in canvas pixels
    int Yoffset; // Y off set, in canvas pixels
    int Xext;    // X extra width
    int Yext;    // Y extra height
    double x_max;
    double y_max;
    double Xstep; // half a distance between X pixels in 'x_max' scale

    pixel_t *zpixel;

} canvas_t;

typedef struct
{
    canvas_t *canvas;
    double current_time;
    double current_level;
    pixel_t level_color;
    double in_current;
    pixel_t in_color;
    double out_current;
    pixel_t out_color;

} dataholder_t;
//////////////////////////////////////////

// Functions declaration //////////////////
int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr);
int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr);
void *controlThreadFunction(void *arg);
float executeCommand(char *command, char *expected_response);
int socketConfig();
int control();
float clamp(float value, float min, float max);
void *plotThreadFunction(void *arg);

// Plot functions /////////////////////////
void c_hlinedraw(canvas_t *canvas, int x_step, int y, pixel_t color);
void c_pixeldraw(canvas_t *canvas, int x, int y, pixel_t color);
void c_vlinedraw(canvas_t *canvas, int x, int y_step, pixel_t color);
void c_linedraw(canvas_t *canvas, double x0, double y0, double x1, double y1, pixel_t color);
canvas_t *c_open(int width, int height, double x_max, double y_max);
dataholder_t *d_init(int width, int height, double x_max, double y_max, double current_level, double in_current, double out_current);
void d_setColors(dataholder_t *data, pixel_t level_color, pixel_t in_color, pixel_t out_color);
void d_draw(dataholder_t *data, double time, double level, double in_angle, double out_angle);
void quitevent();
//////////////////////////////////////////

int main(int argc, char *argv[])
{
    pthread_t control_thread, plot_thread;

    if (argc != 3)
    {
        fprintf(stderr, "USAGE: %s <server_ip> <port> \n", argv[0]);
        exit(1);
    }

    socketConfig(argv[1], atoi(argv[2]));

    pthread_create(&plot_thread, NULL, plotThreadFunction, NULL);
    pthread_create(&control_thread, NULL, controlThreadFunction, NULL);

    pthread_join(plot_thread, NULL);
    pthread_join(control_thread, NULL);

    // Close the socket:
    close(socket_desc);

    return 0;
}

// Plot functions /////////////////////////

inline void c_pixeldraw(canvas_t *canvas, int x, int y, pixel_t color)
{
    *(((pixel_t *)canvas->canvas->pixels) + ((-y + canvas->Yoffset) * canvas->canvas->w + x + canvas->Xoffset)) = color;
}

inline void c_hlinedraw(canvas_t *canvas, int x_step, int y, pixel_t color)
{
    int offset = (-y + canvas->Yoffset) * canvas->canvas->w;
    int x;

    for (x = 0; x < canvas->width + canvas->Xoffset; x += x_step)
    {
        *(((pixel_t *)canvas->canvas->pixels) + (offset + x)) = color;
    }
}

inline void c_vlinedraw(canvas_t *canvas, int x, int y_step, pixel_t color)
{
    int offset = x + canvas->Xoffset;
    int y;
    int Ystep = y_step * canvas->canvas->w;

    for (y = 0; y < canvas->height + canvas->Yext; y += y_step)
    {
        *(((pixel_t *)canvas->canvas->pixels) + (offset + y * canvas->canvas->w)) = color;
    }
}

inline void c_linedraw(canvas_t *canvas, double x0, double y0, double x1, double y1, pixel_t color)
{
    double x;

    for (x = x0; x <= x1; x += canvas->Xstep)
    {
        c_pixeldraw(canvas, (int)(x * canvas->width / canvas->x_max + 0.5), (int)((double)canvas->height / canvas->y_max * (y1 * (x1 - x) + y1 * (x - x0)) / (x1 - x0) + 0.5), color);
    }
}

canvas_t *c_open(int width, int height, double x_max, double y_max)
{
    int x, y;
    canvas_t *canvas;
    canvas = malloc(sizeof(canvas_t));

    canvas->Xoffset = 10;
    canvas->Yoffset = height;

    canvas->Xext = 10;
    canvas->Yext = 10;

    canvas->height = height;
    canvas->width = width;
    canvas->x_max = x_max;
    canvas->y_max = y_max;

    canvas->Xstep = x_max / (double)width / 2;

    // canvas->zpixel = (pixel_t *)canvas->canvas->pixels +(height-1)*canvas->canvas->w;

    SDL_Init(SDL_INIT_VIDEO); // SDL init
    canvas->canvas = SDL_SetVideoMode(canvas->width + canvas->Xext, canvas->height + canvas->Yext, BPP, SDL_SWSURFACE);

    c_hlinedraw(canvas, 1, 0, (pixel_t)SDL_MapRGB(canvas->canvas->format, 255, 255, 255));
    for (y = 10; y < y_max; y += 10)
    {
        c_hlinedraw(canvas, 3, y * height / y_max, (pixel_t)SDL_MapRGB(canvas->canvas->format, 220, 220, 220));
    }
    c_vlinedraw(canvas, 0, 1, (pixel_t)SDL_MapRGB(canvas->canvas->format, 255, 255, 255));
    for (x = 10; x < x_max; x += 10)
    {
        c_vlinedraw(canvas, x * width / x_max, 3, (pixel_t)SDL_MapRGB(canvas->canvas->format, 220, 220, 220));
    }

    return canvas;
}

dataholder_t *d_init(int width, int height, double x_max, double y_max, double current_level, double in_current, double out_current)
{
    dataholder_t *data = malloc(sizeof(dataholder_t));

    data->canvas = c_open(width, height, x_max, y_max);
    data->current_time = 0;
    data->current_level = current_level;
    data->level_color = (pixel_t)SDL_MapRGB(data->canvas->canvas->format, 255, 180, 0); // Orange
    data->in_current = in_current;
    data->in_color = (pixel_t)SDL_MapRGB(data->canvas->canvas->format, 180, 255, 0); // Green
    data->out_current = out_current;
    data->out_color = (pixel_t)SDL_MapRGB(data->canvas->canvas->format, 0, 180, 255); // Blue

    return data;
}

void d_setColors(dataholder_t *data, pixel_t level_color, pixel_t in_color, pixel_t out_color)
{
    data->level_color = level_color;
    data->in_color = in_color;
    data->out_color = out_color;
}

void d_draw(dataholder_t *data, double time, double level, double in_angle, double out_angle)
{
    c_linedraw(data->canvas, data->current_time, data->current_level, time, level, data->level_color);
    c_linedraw(data->canvas, data->current_time, data->in_current, time, in_angle, data->in_color);
    c_linedraw(data->canvas, data->current_time, data->out_current, time, out_angle, data->out_color);
    data->current_time = time;
    data->current_level = level;
    data->in_current = in_angle;
    data->out_current = out_angle;

    SDL_Flip(data->canvas->canvas);
}

void quitevent()

{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        // SDL_QUIT happens when you close the graph window
        if (event.type == SDL_QUIT)
        {
            // close files, etc...
            close(socket_desc);
            SDL_Quit();
            exit(1); // this will terminate all threads !
        }
    }
}

//////////////////////////////////////////

// Sends message to server, -1 if error
int sendMsgToServer(int socket_desc, char *client_message, struct sockaddr *server_addr)
{
    int server_struct_length = sizeof(*server_addr);

    if (sendto(socket_desc, client_message, strlen(client_message), 0,
               server_addr, server_struct_length) < 0)
    {
        printf("Unable to send message\n");
        return -1;
    }
    // printf("Sending: %s\n", client_message);
    return 0;
}

// Receives message from server, -1 if error
int receiveMsgFromServer(int socket_desc, char *server_message, struct sockaddr *server_addr)
{
    // sizeof(*server_addr) quero o tamanho do que o ponteiro esta apontando
    int server_struct_length = sizeof(*server_addr);

    memset(server_message, 0, BUFFER_SIZE * sizeof(char));

    // Receive the server's response:
    if (recvfrom(socket_desc, server_message, BUFFER_SIZE, 0,
                 server_addr, &server_struct_length) < 0)
    {
        // printf("Error while receiving server's msg\n");
        return -1;
    }
    // printf("Server's response: %s\n", server_message);
    return 0;
}

// Sets socket configs and timeout, -1 if error
int socketConfig(char ip[15], int port)
{
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 50 * 1000,
    };

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
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    return 0;
}

// Executes a command and returns the received value (if it has any), 0 if it has no values and -1 if error
float executeCommand(char *command, char *expected_response)
{
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    char value_str[BUFFER_SIZE];
    char *response_cmd;
    float value = 0;
    int response = 0;

    // Clean buffers:
    memset(server_message, 0, sizeof(server_message));
    memset(client_message, 0, sizeof(client_message));

    strcpy(client_message, command);

    // Send the message to server:
    sendMsgToServer(socket_desc, client_message, (struct sockaddr *)&server_addr);

    // Wait for server response:
    response = receiveMsgFromServer(socket_desc, server_message, (struct sockaddr *)&server_addr);

    if (response == -1)
    {
        return -1;
    }

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

// Sends initial commands and start the control
void *controlThreadFunction(void *arg)
{
    float result = 0;
    int socket_flag = 0;

    // Initial commands

    while (socket_flag == 0)
    {
        result = executeCommand("CommTest!", "Comm");
        if (result == -1)
        {
            printf("CommTest failed! Trying again...\n");
        }
        else
        {
            socket_flag = 1;
        }
    }

    while (socket_flag == 1)
    {
        result = executeCommand("Start!", "Start");
        if (result == -1)
        {
            printf("Start failed! Trying again...\n");
        }
        else
        {
            socket_flag = 2;
        }
    }

    while (socket_flag == 2)
    {
        result = executeCommand("SetMax#100!", "Max");
        if (result == -1)
        {
            printf("SetMax failed! Trying again...\n");
        }
        else
        {
            socket_flag = 0;
        }
    }

    // Valve Control
    control();
}

// Controls the graph
void *plotThreadFunction(void *arg)
{
    dataholder_t *data;

    data = d_init(640, 480, 200, 110, level, valve_position, 0);

    while (1)
    {
        d_draw(data, passed_time_ms / 1000, level, valve_position, 0);

        quitevent();
        usleep(50000);
    }
}

// Saturation
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

// Controls the opening and closing of the valve
int control()
{
    float error = 0, previous_error = 0;
    float u = 0, delta_u = 0;
    float P = 0, I = 0, D = 0;
    float previous_D = 0, previous_I = 0;
    float kp = KP, ki = KI, kd = KD;
    char command[BUFFER_SIZE];
    float ref = REFERENCE;
    float dT_s = dT_MS;
    struct timespec start_time, end_time;
    long elapsed_time_us = 0, delta_time_us = 0;
    float result = 0;
    int socket_flag = 0;

    printf("Control started\n");

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

        // Sends the command Get Level until it gets the right response
        while (socket_flag == 0)
        {
            level = executeCommand("GetLevel!", "Level");
            if (level == -1)
            {
                printf("GetLevel failed! Trying again...\n");
            }
            else
            {
                socket_flag = 1;
            }
        }

        socket_flag = 0;

        // Controller
        error = ref - (level / 100.0);

        P = kp * error;

        I = previous_I + ki * (error + previous_error) * dT_s;

        D = previous_D + kd * (error - previous_error) / dT_s;

        u = P + I + D;

        // Control saturation
        u = clamp(u, 5, 95);

        // Calculates how much the valve should open/close
        delta_u = u - valve_position;

        // Open/Close saturation
        delta_u = clamp(delta_u, -2, 2);

        if (delta_u != 0)
        {
            if (delta_u > 0)
            {
                // Valve position can't be bigger than 100
                if ((valve_position + (int)delta_u) < 100)
                {
                    sprintf(command, "OpenValve#%d!", (int)round(delta_u));
                    while (socket_flag == 0)
                    {
                        result = executeCommand(command, "Open");
                        if (result == -1)
                        {
                            printf("OpenValve failed! Trying again...\n");
                        }
                        else
                        {
                            socket_flag = 1;
                        }
                    }
                    valve_position = valve_position + (int)round(delta_u);
                    socket_flag = 0;
                }
                else if (valve_position != 100)
                {
                    delta_u = 100 - valve_position;
                    sprintf(command, "OpenValve#%d!", (int)round(delta_u));
                    while (socket_flag == 0)
                    {
                        result = executeCommand(command, "Open");
                        if (result == -1)
                        {
                            printf("OpenValve failed! Trying again...\n");
                        }
                        else
                        {
                            socket_flag = 1;
                        }
                    }
                    valve_position = 100;
                    socket_flag = 0;
                }
            }
            else if (delta_u < 0)
            {
                // Valve position can't be smaller than 0
                if ((valve_position + (int)delta_u) > 0)
                {
                    sprintf(command, "CloseValve#%d!", -(int)round(delta_u));
                    while (socket_flag == 0)
                    {
                        result = executeCommand(command, "Close");
                        if (result == -1)
                        {
                            printf("CloseValve failed! Trying again...\n");
                        }
                        else
                        {
                            socket_flag = 1;
                        }
                    }
                    valve_position = valve_position + (int)round(delta_u);
                    socket_flag = 0;
                }
                else if (valve_position != 0)
                {
                    delta_u = valve_position;
                    sprintf(command, "CloseValve#%d!", (int)round(delta_u));
                    while (socket_flag == 0)
                    {
                        result = executeCommand(command, "Close");
                        if (result == -1)
                        {
                            printf("CloseValve failed! Trying again...\n");
                        }
                        else
                        {
                            socket_flag = 1;
                        }
                    }
                    valve_position = 0;
                    socket_flag = 0;
                }
            }

            valve_position = clamp(valve_position, 0, 100);

            previous_error = error;
            previous_I = I;
            previous_D = D;

            clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

            // Makes sure that the elapsed time won't be added to the frequency of the thread
            elapsed_time_us = (end_time.tv_nsec - start_time.tv_nsec) / 1000;
            delta_time_us = ((dT_s * 1000000) - elapsed_time_us);
        }

        passed_time_ms += dT_s * 1000;
        usleep(max(delta_time_us, 0));
    }
}
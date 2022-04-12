#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <SDL/SDL.h>
#include <time.h>

#define BUFFER_SIZE 100
#define MAX_FLUX_INITIAL 100
#define ANGLE_INITIAL 50
#define LEVEL_INITIAL 0.4

#define max(a,b) ((a) > (b) ? (a) : (b))

// Graph
#define SCREEN_W 640 // tamanho da janela que sera criada
#define SCREEN_H 640
//#define BPP 8
// typedef Uint8 pixel_t;
//#define BPP 16
// typedef Uint16 pixel_t;
#define BPP 32
typedef Uint32 pixel_t;

typedef struct
{                     /*Struct to store values concerning to the plant being simulated*/
    double max_flux;  /*Max out flux; Can be changed via SetMax command*/
    double in_angle;  /*Input Valve angle*/
    double out_angle; /*Output Valve angle*/
    double level;     /*Current plant level*/
    double passed_time;
} plant_t;

// Graph////////////////////////////
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
//////////////////////////////////////

// Global variables
char cmd[BUFFER_SIZE];
int value;
int socket_desc;

int sendMsgToClient(int socket_desc, char *server_message, struct sockaddr *client_addr);
int receiveMsgFromClient(int socket_desc, char *client_message, struct sockaddr *client_addr);
void handleMessage(char *client_message, char *server_message);
void *serverThreadFunction(void *arg);
void *plantThreadFunction(void *arg);
void *plotThreadFunction(void *arg);
float clamp(float value, float max, float min);
double outAngle(double time);

void c_hlinedraw(canvas_t *canvas, int x_step, int y, pixel_t color);
void c_pixeldraw(canvas_t *canvas, int x, int y, pixel_t color);
void c_vlinedraw(canvas_t *canvas, int x, int y_step, pixel_t color);
void c_linedraw(canvas_t *canvas, double x0, double y0, double x1, double y1, pixel_t color);
canvas_t *c_open(int width, int height, double x_max, double y_max);
dataholder_t *d_init(int width, int height, double x_max, double y_max, double current_level, double in_current, double out_current);
void d_setColors(dataholder_t *data, pixel_t level_color, pixel_t in_color, pixel_t out_color);
void d_draw(dataholder_t *data, double time, double level, double in_angle, double out_angle);
void quitevent();
long texec(struct timespec antes);

plant_t plant = {
    .max_flux = MAX_FLUX_INITIAL,
    .in_angle = ANGLE_INITIAL,
    .out_angle = 0,
    .level = LEVEL_INITIAL,
    .passed_time = 0,
};

int main(void)
{
    pthread_t server_thread, plant_thread, plot_thread;

    pthread_create(&plot_thread, NULL, plotThreadFunction, NULL);
    pthread_create(&server_thread, NULL, serverThreadFunction, NULL);
    pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);

    pthread_join(plot_thread, NULL);
    pthread_join(server_thread, NULL);
    pthread_join(plant_thread, NULL);

    return 0;
}

// Graph////////////////////////////////////////////
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

double outAngle(double time)
{
    if (time <= 0)
    {
        return 50; /*Starting value*/
    }
    if (time < 20000)
    {
        return (50 + time / 400); /*Ramp up*/
    }
    if (time < 30000)
    {
        return 100;
    }
    if (time < 50000)
    {
        return (100 - (time - 30000) / 250); /*Ramp down*/
    }
    if (time < 70000)
    {
        return (20 + (time - 50000) / 1000);
    }
    if (time < 100000)
    {
        return (40 + 20 * cos((time - 70000) * 2 * M_PI / 10000));
    }
    return 100;
}

void *plotThreadFunction(void *arg)
{
    dataholder_t *data;

    data = d_init(640, 480, 200, 110, plant.level, plant.in_angle, plant.out_angle);

    while (1)
    {
        d_draw(data, plant.passed_time/1000, plant.level * 100, plant.in_angle, plant.out_angle);

        quitevent();
        usleep(50000);
    }
}

void *plantThreadFunction(void *arg)
{
    float delta = 0, influx = 0, outflux = 0;
    int dT_ms = 10;
    struct timespec start_time, end_time;
    long elapsed_time = 0;
    long variavel_do_danilo = 0;

    printf("Plant simulation started\n");

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

        if (cmd != NULL)
        {
            // printf("cmd: %p, %c\n", cmd, *cmd);
            if (strcmp(cmd, "OpenValve") == 0) /*If are equal*/
            {
                delta += value;
            }
            if (strcmp(cmd, "CloseValve") == 0)
            {
                delta -= value;
            }
            if (strcmp(cmd, "SetMax") == 0)
            {
                plant.max_flux = value;
            }
        }

        // printf("cmd: %s\n", cmd);
        // printf("value: %d\n", value);
        //  printf("\ndelta: %f\n", delta);

        // printf("\nPlant delta: %.2f\n", delta);

        if (delta > 0)
        {
            if (delta < 0.01 * dT_ms)
            {
                plant.in_angle = clamp(plant.in_angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                plant.in_angle = clamp(plant.in_angle + 0.01 * dT_ms, 0, 100);
                delta -= 0.01 * dT_ms;
            }
            // printf("Plant delta2: %.2f\n", delta);
        }
        else if (delta < 0)
        {
            if (delta > -0.01 * dT_ms)
            {
                plant.in_angle = clamp(plant.in_angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                plant.in_angle = clamp(plant.in_angle - 0.01 * dT_ms, 0, 100);
                delta += 0.01 * dT_ms;
            }
        }
        // printf("Plant in_angle: %.2f\n", plant.in_angle);

        influx = 1 * sin(M_PI / 2 * plant.in_angle / 100);
        plant.out_angle = outAngle(plant.passed_time);
        outflux = (plant.max_flux / 100) * (plant.level / 1.25 + 0.2) * sin(M_PI / 2 * (plant.out_angle) / 100);
        plant.level = clamp(plant.level + 0.00002 * dT_ms * (influx - outflux), 0, 1);

        plant.passed_time += dT_ms;

        // printf("\npassed_time: %f\n", plant.passed_time);

        clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
        elapsed_time = (end_time.tv_nsec - start_time.tv_nsec)/1000;
        variavel_do_danilo = ((dT_ms * 1000)-elapsed_time);

        usleep(max(variavel_do_danilo,0));

    }
}

void *serverThreadFunction(void *arg)
{
    struct sockaddr_in server_addr, client_addr;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];

    // Clean buffers:
    // Set all bytes to 0
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
    server_addr.sin_port = htons(7000);
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

        handleMessage(client_message, server_message);

        sendMsgToClient(socket_desc, server_message, (struct sockaddr *)&client_addr);
    }

    // Close the socket:
    close(socket_desc);
}

void handleMessage(char *client_message, char *server_message)
{
    char value_str[10];
    char *cmd_pointer;
    // char client_message[BUFFER_SIZE];

    if (strstr(client_message, "#") == NULL) // Checks if has not received any argument
    {
        cmd_pointer = strtok(client_message, "!"); // Breaks the string to eliminate the '!'
    }
    else // If has arguments
    {
        cmd_pointer = strtok(client_message, "#"); // Gets the command related part of message
        strcpy(value_str, strtok(NULL, "!"));      // Gets the value part of message
        value = atoi(value_str);                   // Converts the received value (string) to integer
    }

    strcpy(cmd, cmd_pointer);

    // printf("cmd1: %s\n", cmd);

    // Respond to client:
    if (strcmp(client_message, "CommTest") == 0)
    {
        strcpy(server_message, "Comm#OK!");
    }
    else if (strcmp(client_message, "Start") == 0)
    {
        strcpy(server_message, "Start#OK!");
        // TODO (Re-)Iniciar o simulador da planta
    }
    else if (strcmp(client_message, "GetLevel") == 0)
    {
        strcpy(server_message, "Level#");
        // printf("\nPLANT LEVEL: %f, %d", plant.level, (int)(plant.level * 100));
        sprintf(value_str, "%d", (int)round(plant.level * 100));
        strcat(value_str, "!");
        strcpy(server_message, strcat(server_message, value_str));
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

    memset(client_message, 0, BUFFER_SIZE * sizeof(char));

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

    // printf("Msg from client: %s\n", client_message);
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

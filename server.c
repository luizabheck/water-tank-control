#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <SDL/SDL.h>

#define BUFFER_SIZE 2000
#define MAX_FLUX_INITIAL 100
#define ANGLE_INITIAL 50
#define LEVEL_INITIAL 0.4

//Graph
#define SCREEN_W 640 //tamanho da janela que sera criada
#define SCREEN_H 640
//#define BPP 8
//typedef Uint8 pixel_t;
//#define BPP 16
//typedef Uint16 pixel_t;
#define BPP 32
typedef Uint32 pixel_t;

typedef struct
{                     /*Struct to store values concerning to the plant being simulated*/
    double max_flux;  /*Max out flux; Can be changed via SetMax command*/
    double angle;     /*Input Valve angle*/
    double out_angle; /*Output Valve angle*/
    double level;     /*Current plant level*/
    double passed_time;
} plant_t;

//Graph////////////////////////////
typedef struct {
  SDL_Surface *canvas;
  int Height; // canvas height
  int Width;  // canvas width
  int Xoffset; // X off set, in canvas pixels
  int Yoffset; // Y off set, in canvas pixels
  int Xext; // X extra width
  int Yext; // Y extra height
  double Xmax;
  double Ymax;
  double Xstep; // half a distance between X pixels in 'Xmax' scale

  pixel_t *zpixel;

} canvas_t;

typedef struct {
  canvas_t *canvas;
  double   Tcurrent;
  double   Lcurrent;
  pixel_t Lcolor;
  double   INcurrent;
  pixel_t INcolor;
  double   OUTcurrent;
  pixel_t OUTcolor;

} dataholder_t;
//////////////////////////////////////

// Global variables
char *cmd;
int value;

int sendMsgToClient(int socket_desc, char *server_message, struct sockaddr *client_addr);
int receiveMsgFromClient(int socket_desc, char *client_message, struct sockaddr *client_addr);
void messageHandler(char *client_message, char *server_message);
void *serverThreadFunction(void *arg);
void *plantThreadFunction(void *arg);
float clamp(float value, float max, float min);
double outAngle(double tempo);
dataholder_t *datainit(int Width, int Height, double Xmax, double Ymax, double Lcurrent, double INcurrent, double OUTcurrent);
void c_hlinedraw(canvas_t *canvas, int xstep, int y, pixel_t color);
void c_pixeldraw(canvas_t *canvas, int x, int y, pixel_t color);
void c_vlinedraw(canvas_t *canvas, int x, int ystep, pixel_t color);
void c_linedraw(canvas_t *canvas, double x0, double y0, double x1, double y1, pixel_t color);
canvas_t *c_open(int Width, int Height, double Xmax, double Ymax);
void setdatacolors(dataholder_t *data, pixel_t Lcolor, pixel_t INcolor, pixel_t OUTcolor);
void quitevent();


plant_t plant = {
    .max_flux = MAX_FLUX_INITIAL,
    .angle = ANGLE_INITIAL,
    .out_angle = 0,
    .level = LEVEL_INITIAL,
    .passed_time = 0,
};


// int main(void)
// {
//     pthread_t server_thread, plant_thread;

//     pthread_create(&server_thread, NULL, serverThreadFunction, NULL);
//     pthread_create(&plant_thread, NULL, plantThreadFunction, NULL);

//     pthread_join(server_thread, NULL);
//     pthread_join(plant_thread, NULL);

//     return 0;
// }

int main( int argc, const char* argv[] ) {
  dataholder_t *data;
  double t=0;

  data = datainit(640,480,55,110,45,0,0);

  for (t=0;t<50;t+=0.1) {
    datadraw(data,t,(double)(50+20*cos(t/5)),(double)(70+10*sin(t/10)),(double)(20+5*cos(t/2.5)));
  }

  while(1) {
    quitevent();
  }
}


//Graph////////////////////////////////////////////
inline void c_pixeldraw(canvas_t *canvas, int x, int y, pixel_t color)
{
  *( ((pixel_t*)canvas->canvas->pixels) + ((-y+canvas->Yoffset) * canvas->canvas->w + x+ canvas->Xoffset)) = color;
}

inline void c_hlinedraw(canvas_t *canvas, int xstep, int y, pixel_t color)
{
  int offset =  (-y+canvas->Yoffset) * canvas->canvas->w;
  int x;

  for (x = 0; x< canvas->Width+canvas->Xoffset ; x+=xstep) {
        *( ((pixel_t*)canvas->canvas->pixels) + (offset + x)) = color;
  }
}

inline void c_vlinedraw(canvas_t *canvas, int x, int ystep, pixel_t color)
{
  int offset = x+canvas->Xoffset;
  int y;
  int Ystep = ystep*canvas->canvas->w;

  for (y = 0; y< canvas->Height+canvas->Yext ; y+=ystep) {
    *( ((pixel_t*)canvas->canvas->pixels) + (offset + y*canvas->canvas->w)) = color;
  }
}

inline void c_linedraw(canvas_t *canvas, double x0, double y0, double x1, double y1, pixel_t color) {
  double x;

  for (x=x0; x<=x1; x+=canvas->Xstep) {
    c_pixeldraw(canvas, (int)(x*canvas->Width/canvas->Xmax+0.5), (int)((double)canvas->Height/canvas->Ymax*(y1*(x1-x)+y1*(x-x0))/(x1-x0)+0.5),color);
  }
}

canvas_t *c_open(int Width, int Height, double Xmax, double Ymax)
{
  int x,y;
  canvas_t *canvas;
  canvas = malloc(sizeof(canvas_t));

  canvas->Xoffset = 10;
  canvas->Yoffset = Height;

  canvas->Xext = 10;
  canvas->Yext = 10;

  canvas->Height = Height;
  canvas->Width  = Width; 
  canvas->Xmax   = Xmax;
  canvas->Ymax   = Ymax;

  canvas->Xstep  = Xmax/(double)Width/2;

  //  canvas->zpixel = (pixel_t *)canvas->canvas->pixels +(Height-1)*canvas->canvas->w;

  SDL_Init(SDL_INIT_VIDEO); //SDL init
  canvas->canvas = SDL_SetVideoMode(canvas->Width+canvas->Xext, canvas->Height+canvas->Yext, BPP, SDL_SWSURFACE); 

  c_hlinedraw(canvas, 1, 0, (pixel_t) SDL_MapRGB(canvas->canvas->format,  255, 255,  255));
  for (y=10;y<Ymax;y+=10) {
    c_hlinedraw(canvas, 3, y*Height/Ymax , (pixel_t) SDL_MapRGB(canvas->canvas->format,  220, 220,  220));
  }
  c_vlinedraw(canvas, 0, 1, (pixel_t) SDL_MapRGB(canvas->canvas->format,  255, 255,  255));
  for (x=10;x<Xmax;x+=10) {
    c_vlinedraw(canvas, x*Width/Xmax, 3, (pixel_t) SDL_MapRGB(canvas->canvas->format,  220, 220,  220));
  }

  return canvas;
}

dataholder_t *datainit(int Width, int Height, double Xmax, double Ymax, double Lcurrent, double INcurrent, double OUTcurrent) {
  dataholder_t *data = malloc(sizeof(dataholder_t));


  data->canvas=c_open(Width, Height, Xmax, Ymax);
  data->Tcurrent=0;
  data->Lcurrent=Lcurrent;
  data->Lcolor= (pixel_t) SDL_MapRGB(data->canvas->canvas->format,  255, 180,  0);
  data->INcurrent=INcurrent;
  data->INcolor=(pixel_t) SDL_MapRGB(data->canvas->canvas->format,  180, 255,  0);
  data->OUTcurrent=OUTcurrent;
  data->OUTcolor=(pixel_t) SDL_MapRGB(data->canvas->canvas->format,  0, 180,  255);


  return data;
}

void setdatacolors(dataholder_t *data, pixel_t Lcolor, pixel_t INcolor, pixel_t OUTcolor) {
  data->Lcolor=Lcolor;
  data->INcolor=INcolor;
  data->OUTcolor=OUTcolor;
}

void datadraw(dataholder_t *data, double time, double level, double inangle, double outangle) {
  c_linedraw(data->canvas,data->Tcurrent,data->Lcurrent,time,level,data->Lcolor);
  c_linedraw(data->canvas,data->Tcurrent,data->INcurrent,time,inangle,data->INcolor);
  c_linedraw(data->canvas,data->Tcurrent,data->OUTcurrent,time,outangle,data->OUTcolor);
  data->Tcurrent = time;
  data->Lcurrent = level;
  data->INcurrent = inangle;
  data->OUTcurrent = outangle;

  SDL_Flip(data->canvas->canvas);
}

void quitevent() {
  SDL_Event event;

  while(SDL_PollEvent(&event)) { 
    if(event.type == SDL_QUIT) { 
      // close files, etc...

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

double outAngle(double tempo)
{
    if (tempo <= 0)
    {
        return 50; /*Starting value*/
    }
    if (tempo < 20000)
    {
        return (50 + tempo / 400); /*Ramp up*/
    }
    if (tempo < 30000)
    {
        return 100;
    }
    if (tempo < 50000)
    {
        return (100 - (tempo - 30000) / 250); /*Ramp down*/
    }
    if (tempo < 70000)
    {
        return (20 + (tempo - 50000) / 1000);
    }
    if (tempo < 100000)
    {
        return (40 + 20 * cos((tempo - 70000) * 2 * M_PI / 10000));
    }
    return 100;
}

void *plantThreadFunction(void *arg)
{
    float delta = 0, influx = 0, outflux = 0;
    int dT_ms = 10;

    while (1)
    {
        if (cmd != NULL)
        {
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

        printf("\nPlant delta1: %.2f\n", delta);

        if (delta > 0)
        {
            if (delta < 0.01 * dT_ms)
            {
                plant.angle = clamp(plant.angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                plant.angle = clamp(plant.angle + 0.01 * dT_ms, 0, 100);
                delta -= 0.01 * dT_ms;
            }
            printf("Plant delta2: %.2f\n", delta);
        }
        else if (delta < 0)
        {
            if (delta > -0.01 * dT_ms)
            {
                plant.angle = clamp(plant.angle + delta, 0, 100);
                delta = 0;
            }
            else
            {
                plant.angle = clamp(plant.angle - 0.01 * dT_ms, 0, 100);
                delta += 0.01 * dT_ms;
            }
        }
        printf("Plant angle: %.2f\n", plant.angle);

        influx = 1 * sin(M_PI / 2 * plant.angle / 100);
        plant.out_angle = outAngle(plant.passed_time);
        outflux = (plant.max_flux / 100) * (plant.level / 1.25 + 0.2) * sin(M_PI / 2 * (plant.out_angle) / 100);
        plant.level = clamp(plant.level + 0.00002 * dT_ms * (influx - outflux), 0, 1);

        printf("Plant level: %.2f\n", plant.level);
        usleep(1000000);
    }
}

void *serverThreadFunction(void *arg)
{
    struct sockaddr_in server_addr, client_addr;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    int socket_desc;

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
    char value_str[10];

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
        // TODO (Re-)Iniciar o simulador da planta
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

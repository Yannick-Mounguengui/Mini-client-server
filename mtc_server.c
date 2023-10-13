#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "utils.h"
#include "protocol.h"
#include <pthread.h>


#define MAXNTHREAD 4
pthread_t tid[ MAXNTHREAD ];
int init_sd(int myport);
void do_service(int sd);

static int verbose = 0;

struct client {
        char pseudo[PSEUDO_SIZE];
        int socket;
};

struct client all_clients[MAXNTHREAD];

#define PRINT(...)                         \
    do {                                   \
        if (verbose)                       \
            fprintf(stderr, __VA_ARGS__);  \
    } while (0)

    void * body ( void * arg)
     {
       struct sockaddr_in c_add ;
    socklen_t addrlen ;
    int base_sd = (int ) arg;
    int sd;

    while (1) {
    sd = accept ( base_sd , CAST_ADDR (& c_add ), & addrlen );

    do_service (sd);
    close (sd);
    }
    pthread_detach(pthread_self());
  }

     int main (int argc , char * argv [])
      {
      int base_sd ;
      int myport ;

      if ( argc < 2) USR_ERR (" usage : server <port >");
      myport = atoi ( argv [1]) ;

      base_sd = init_sd ( myport );

      for ( int i=0 ; i< MAXNTHREAD ; i++) // pre -cré ation des threads
      pthread_create (& tid [i], 0, body , ( void *) base_sd );

      while (1) ;
      }


int init_sd(int myport)
{
    struct sockaddr_in my_addr;
    int sd;

    bzero(&my_addr, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(myport);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    PRINT("Preparing socket\n");
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) SYS_ERR("Error in creating socket");

    if (bind(sd, CAST_ADDR(&my_addr), sizeof(my_addr)) < 0)
        SYS_ERR("Bind failed!");

    if (listen(sd, 5) < 0)
        SYS_ERR("listen failed!");

    PRINT("Server listening on port %d\n", myport);

    return sd;
}

void do_service(int sd)
{
    int i, l=1;
    char buffer[MSG_SIZE];
    struct message msg;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    // first read the pseudo
    int r = read(sd, buffer, PSEUDO_SIZE);
    if (r < 0) SYS_ERR("Error in reading from the socket");

    buffer[r] = 0;
    char mypseudo[PSEUDO_SIZE];
    strcpy(mypseudo, buffer);
    PRINT("Received pseudo %s\n", mypseudo);
    pthread_mutex_lock(&mutex);
    for (i = 0; i < MAXNTHREAD; i++) {
        if (all_clients[i].socket == 0) {
            strcpy(all_clients[i].pseudo, mypseudo);
            all_clients[i].socket = sd;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    do {
        l = read(sd, &msg, sizeof(msg));
        if (l == 0) break;
        strcpy(buffer, msg.msg);
        PRINT("Server: received %s\n", buffer);

        i = 0;
        while (buffer[i] != 0) {
            buffer[i] = toupper(buffer[i]);
            i++;
        }
        PRINT("Server: sending %s\n", buffer);

        strcpy(msg.pseudo, mypseudo);
        strcpy(msg.msg, buffer);

        pthread_mutex_lock(&mutex);
        for (i = 0; i < MAXNTHREAD; i++) {
            if (all_clients[i].socket != 0) {
                write(all_clients[i].socket, &msg, sizeof(msg));
            }
        }
        pthread_mutex_unlock(&mutex);
    } while (l!=0);

    pthread_mutex_lock(&mutex);
    for (i = 0; i < MAXNTHREAD; i++) {
        if (all_clients[i].socket == sd) {
            all_clients[i].socket = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

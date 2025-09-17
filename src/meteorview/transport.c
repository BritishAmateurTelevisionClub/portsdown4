#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include "transport.h"
#include "meteorview.h"
#include "font/font.h"
#include "graphics.h"

static int sockfd = 0;
bool connection_made;
extern bool app_exit;
extern char serverip[20];     // Read in from config file in main


void transport_close()
{
  int err = 0;

  err = close(sockfd);
  if (err == 0)
  {
    printf("Port closed\n");
  }
  else
  {
    printf("Attempted to close port but failed\n");
  }
}


int transport_init()
{
  int err = 0;
  struct sockaddr_in serv_addr;

  // Create socket
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("\n Error : Could not create socket \n");
    return 1;
  } 
  else  // Socket created
  {
    printf("Socket created\n");
    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(7680); 

    if(inet_pton(AF_INET, serverip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton (server address conversion) error occured\n");
        printf("Server address %s failed\n", serverip);
        return 1;
    } 
    else   // Server address valid, so connect to it
    {
      printf("Server address valid format\n");
      if ((err=connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
      {
        printf("Error : Connect Failed %i %i ", err ,errno);
        printf("connect: %s\n", strerror(errno));
        return 1;
      } 
      else            // Connected
      {
        // make it non blocking
        printf("Changing socket to non-blocking\n");
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

        connection_made = true;
        printf("Connection to server established\n");
        return 0;
      }
    }
  }
}


int transport_send(unsigned char *data)
{
  int err = 0;
  int error_count = 0;

  do
  {
    err = write(sockfd, data, TCP_DATA_PRE + FFT_BUFFER_SIZE * sizeof(short) * 2);
        
    if (err < 0)
    {
      printf("Error : Write failed %i %i ",err,errno);
      printf(" : %s\n", strerror(errno));
      usleep(10000);
      error_count++;
    }
    else
    {
      error_count = 0;
    }

    if ((error_count > 10) && (app_exit == false))  // errors and not already exiting
    {
      printf("Transport_send error count > 10, returning\n");
      return 1;
    }
  }
  while ((err < 0) && (errno == 11));

  if (err < 0)
  {
    usleep(1000000);
    printf("Reconnecting \n");
    transport_init();
  }
  return 0;
}


#include <stdio.h>
#include <string.h>

#include "socket.h"
#include "queue.h"

int msgIn;
int msgOut;
msg_t msgQueue[MSGQUEUESIZE];

int pushMsg( char* msg, int retries )
{
  int bEmpty;
  
  if((( msgIn + 1 ) % MSGQUEUESIZE ) == msgOut ) {
    printf( "msg queue is full\n" );
    return -1;
  }
  if( msgIn == msgOut ) bEmpty=1;
  
  strcpy( msgQueue[msgIn].msg, msg );
  msgQueue[msgIn].retries = retries;
  msgIn++;
  
  if( msgIn >= MSGQUEUESIZE ) msgIn = 0;

  return bEmpty;
}

int getMsg( char* msg, int* retries )
{
  if( msgIn != msgOut )
  {
    strcpy(msg,msgQueue[msgOut].msg );
    if( retries ) *retries = msgQueue[msgOut].retries;
    msgOut++;
    if( msgOut >= MSGQUEUESIZE ) msgOut = 0;
    return 0;
  } 
  return -1;
}


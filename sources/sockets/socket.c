/*
 * sockect.c
 *   Listener and talker socket
*/

#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/netdevice.h>

#include "socket.h"
#include "queue.h"

#define INTERCOM_PORT    50451
#define INTERCOM_GROUP   "225.0.0.50"

#define HEADER           "MSGCOM1.0:"
#define EVENT_CMD        "EVNT:"
#define HELLO_CMD        "HELO:"
#define SYNC_CMD         "SYNC:"
#define RESET_CMD	 "RSET:"

#define ROLLOVER	 100
int idx=0;

int currentComState = 0;
int bRun;

int normalRetries = 2;

char myIP[IPSTRSIZE];

socket_diag_t diag;

extern int bTestMode;
extern int bFakeErr;

pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t notiferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notify = PTHREAD_COND_INITIALIZER;


stateChangeCallback callback = NULL;


void threadSafePush(char* msg, int retries)
{
  if( bFakeErr ) {
     // send a random # of retries with potentially 1 more than expected
     retries = 1 + rand( ) % ( normalRetries + 1 );
  }

  pthread_mutex_lock(&mutex);
  if(pushMsg( msg,retries )==1) pthread_cond_broadcast(&condition);
  pthread_mutex_unlock(&mutex);
}


char* getMyIP( )
{
  return myIP;
}


station_diag_t* updateDiagData( char* IP, char* data, int cnt )
{
  int i;
  station_diag_t *pOther = NULL;

  diag.frmCnt++;

  for(i=0; i<MAX_OTHERS; i++)
  {
    if(strcmp( diag.other[i].IP,IP ) == 0)
    {
      pOther = &diag.other[i]; 
      break;
    }
  }

  // Got a new IP and storage not full
  if( pOther == NULL && diag.otherFullErr == 0 )
  {
    printf( "New other station IP: %s\n", IP );
    for( i=0; i<MAX_OTHERS;i++ )
    {
      if( diag.other[i].IP[0] == 0 )
      {
        pOther = &diag.other[i];
        strcpy( pOther->IP, IP );
        break;
      }
      else printf( "Index %d has IP:'%s'\n", i, diag.other[i].IP );
    }
  }

  if( pOther == NULL )
  {
    printf( "  ERROR: Not enough space to store new other station info.\n" );
    diag.otherFullErr = 1;
    return NULL;
  }

  if( pOther->lastRxSize == cnt && memcmp( pOther->lastRxData, data, cnt ) == 0 )
  {
    pOther->repeatCnt++;
  }
  else
  {
    // If not the first packet and not a repeat...
    if( pOther->rxCnt )
    {
      if( pOther->repeatCnt < normalRetries )
      {
        pOther->repeatRxMissingErrCnt++;
      } 
      else if( pOther->repeatCnt > normalRetries )
      {
        pOther->repeatRxExtraErrCnt++;
      }
    }
    pOther->repeatCnt = 0;
    memcpy( pOther->lastRxData, data, cnt );
    pOther->lastRxSize = cnt;
  }

  clock_gettime(CLOCK_MONOTONIC, &pOther->lastRxTime);  
  pOther->rxCnt++;

  return pOther;
}

void signalReset( )
{
  char msg[MSGBUFSIZE];
  sprintf( msg, "%s%s", HEADER, RESET_CMD );
  threadSafePush( msg, normalRetries );
}

void signalNewState( int newState )
{
  char msg[MSGBUFSIZE];
  currentComState = newState;
  if( idx>=ROLLOVER ) signalReset( );  
  sprintf( msg, "%s%s%d,%x", HEADER, EVENT_CMD, ++idx, currentComState );
  threadSafePush( msg, normalRetries );
}

void signalHello( )
{
  char msg[MSGBUFSIZE];
  sprintf( msg, "%s%s%s", HEADER, HELLO_CMD, myIP );
  threadSafePush( msg, normalRetries );
}

void signalSync( )
{
  char msg[MSGBUFSIZE];
  sprintf( msg, "%s%s%d,%x", HEADER, SYNC_CMD, idx, currentComState );
  threadSafePush( msg, 0 );
}

int processEvent( char* pt )
{
  int ret = -1;
  int tmp;

  // Get the index of the packet
  if( sscanf( pt, "%d", &tmp ) == 1 ) {
    // If the index of the packet is greater than our own current index 
    // this means this packet is either our own or a repeat.
    if( tmp > idx ) {
      pt = strchr( pt, ',' );
      if( pt ) {
        idx = tmp;
        pt = pt + 1;
        if( sscanf( pt, "%x", &tmp ) == 1 ) {
          // printf( "New state %x received (%d)\n", tmp, idx );
          currentComState = tmp;
          pthread_cond_broadcast(&notify);
          ret = 0;
        }
        else 
        {
          // printf( "Missing state value.\n"); 
        }
      }
      else 
      {
        // printf( "Missing coma separator.\n");
      }             
    }
    else
    { 
      ret = 0;
    } // it's a repeat.
  }
  else
  {
    // printf( "Missing index\n"); 
  }

  return ret;
}

int processHello( char* pt )
{
  int ret=-1;
  if(strcmp(pt,myIP)==0) {
    // printf( "Ignoring my own hello.\n" );
  }
  else {
    signalSync( );
    ret=0;
  }
  return ret;
}

void* listenerThread(void *arg)
{
  struct sockaddr_in addr;
  int fd, nbytes;
  unsigned int addrlen;
  struct ip_mreq mreq;
  char msgbuf[MSGBUFSIZE];
  u_int yes=1;

  /* create what looks like an ordinary UDP socket */
  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    return NULL;
  }

  /* allow multiple sockets to use the same PORT number */
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
    perror("Reusing ADDR failed");
    return NULL;
  }

  /* set up destination address */
  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
  addr.sin_port=htons(INTERCOM_PORT);
     
  /* bind to receive address */
  if (bind(fd,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
    perror("bind");
    return NULL;
  }
     
  /* use setsockopt() to request that the kernel join a multicast group */
  mreq.imr_multiaddr.s_addr=inet_addr(INTERCOM_GROUP);
  mreq.imr_interface.s_addr=htonl(INADDR_ANY);
  if (setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
    perror("setsockopt");
    return NULL;
  }

  /* now just enter the read loop */
  while (bRun) {
    char *pt;
    char srcIP[IPSTRSIZE];
    station_diag_t *pOther;

    addrlen=sizeof(addr);
    if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0 ) {
      perror("recvfrom");
      return NULL;
    }

    // Zero terminate the string
    msgbuf[nbytes]=0;

    inet_ntop(AF_INET, &addr.sin_addr, srcIP, sizeof(srcIP));

    if(strcmp( srcIP,myIP)==0) {
      // Ignore our own transmission
      continue;
    }

    pOther = updateDiagData( srcIP, msgbuf, nbytes );

    // Look for version 1.0 header in the message
    // Format as follow: HEADER:CMD
    //   HELO:<IP>
    //   SYNC:<idx>,<state>
    //   EVNT:<idx>,<state>
    pt = msgbuf;
    if(strncmp(pt,HEADER,strlen(HEADER))==0) {
      pt = pt + strlen(HEADER);

      if( strncmp(pt,EVENT_CMD,strlen(EVENT_CMD))==0) {
        pt = pt + strlen(EVENT_CMD);
        if( processEvent( pt ) < 0 )
        {
          // printf( "Malformed CMD: '%s'\n", msgbuf );
          diag.errCnt++;
          if( pOther ) pOther->mlfrmdRxErrCnt++;
        }
      }
      else if( strncmp(pt,HELLO_CMD,strlen(HELLO_CMD))==0) {
        pt = pt + strlen(HELLO_CMD);
        if( processHello( pt ) < 0 )
        {
          // printf( "Malformed HELLO: '%s'\n", msgbuf );
          diag.errCnt++;
          if( pOther ) pOther->mlfrmdRxErrCnt++;
        }
      }
      else if( strncmp(pt,SYNC_CMD,strlen(SYNC_CMD))==0) {
        pt = pt + strlen(SYNC_CMD);
        if( processEvent( pt ) < 0 )
        {
          // printf( "Malformed SYNC: '%s'\n", msgbuf );
          diag.errCnt++;
          if( pOther ) pOther->mlfrmdRxErrCnt++;
        }
      }
      else if( strncmp(pt,RESET_CMD,strlen(RESET_CMD))==0) {
        pt = pt + strlen(RESET_CMD);
        idx = 0;
      }
      else
      {
        diag.errCnt++;
        if( pOther ) pOther->mlfrmdRxErrCnt++;
        // printf( "Unknown command: '%s'\n", msgbuf );
      }
    }
    else
    { 
      diag.errCnt++;
      if( pOther ) pOther->unxpctdRxErrCnt++;
      // printf( "Unknown packet: '%s'\n", msgbuf );
    }
  }
  return NULL;
}

void* notifierThread( void* arg )
{
  pthread_mutex_lock(&notiferMutex);
  while (bRun) {
    pthread_cond_wait( &notify, &notiferMutex );
    if( callback ) callback( currentComState );
  }
  pthread_mutex_unlock(&notiferMutex);
  return NULL;
}

void* talkerThread(void* arg)
{
  int fd,i,retries,sleep;
  struct sockaddr_in addr;
  char message[MSGBUFSIZE];
  int broadcastEnable=1;

  /* create what looks like an ordinary UDP socket */
  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    return NULL;
  }

  if( setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0 ) {
    perror("setsockopt");
    return NULL;
  }

  /* set up destination address */
  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=inet_addr(INTERCOM_GROUP);
  addr.sin_port=htons(INTERCOM_PORT);

  pthread_mutex_lock(&mutex);
   
  /* now just sendto() our destination! */
  while (bRun) 
  {  
    while( getMsg(message,&retries)==0 ) 
    {
      // printf( "<<< '%s' [%d]\n", message,retries );

      for(i=0;i<(retries+1);i++) 
      {
        if( i>0 && bFakeErr )
        {
          // Put garbage in the retry message
          message[rand( ) % strlen(message)] = 'X';
        }

        if (sendto(fd,message,strlen(message),0,(struct sockaddr *) &addr, sizeof(addr)) < 0) {
          perror("sendto");
          diag.txErrCnt++;
          diag.errCnt++;
        }
        else
        {
          diag.frmCnt++;
          diag.txCnt++;
        }

        if(i<(retries+1))
        {
          // Sleep between 5 and 10ms between retries
          sleep = 5000 + ( rand() % 5000 );
          // printf( "sleep %d ms\n", sleep/1000 );
          if(usleep( sleep ) < 0) 
          {
            perror("usleep");
            return NULL;
          }
        }
      }
    }
    // printf( "Talker thread waiting for next message to send...\n" );
    pthread_cond_wait( &condition, &mutex );
  }
  pthread_mutex_unlock(&mutex);
  return NULL;
}


void resetNetworkHealth( )
{
  diag.frmCnt = 0;
  diag.errCnt = 0;
}


char* getNetworkHealth( )
{
  float errRate;

  if( diag.frmCnt )
  {
    errRate = (float)diag.errCnt / (float)diag.frmCnt;

    if( errRate < 0.01 )
      return "NET OK";
    else if( errRate < 0.05 )
      return "NET WRN";
    else
      return "NET ERR";
  }
  return "NET ???";  
}

int getNetworkStatus( )
{
  int s,ret=-1;
  struct ifconf ifconf;
  struct ifreq ifr[50];
  int ifs;
  int i;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket");
    return -1;
  }

  ifconf.ifc_buf = (char *) ifr;
  ifconf.ifc_len = sizeof ifr;

  if (ioctl(s, SIOCGIFCONF, &ifconf) == -1) {
    perror("ioctl");
    return -1;
  }

  ifs = ifconf.ifc_len / sizeof(ifr[0]);
  printf("This device has %d networking interfaces\n", ifs);
  for (i = 0; i < ifs; i++) {
    struct sockaddr_in *s_in = (struct sockaddr_in *) &ifr[i].ifr_addr;

    if( strcmp(ifr[i].ifr_name,"eth0")==0 ) {
      if (!inet_ntop(AF_INET, &s_in->sin_addr, myIP, sizeof(myIP))) {
        perror("inet_ntop");
        break;
      }
      printf("This device's IP address on %s is %s.\n", ifr[i].ifr_name, myIP );
      ret=0;
    }
  }
  close(s);
  return ret;
}

void picassoPrintStatus( );

void* monitorThread( void* param )
{
  pthread_t listener,talker,notifier;

  while( getNetworkStatus( ) < 0 ) {
    printf("Network not up yet. Coming back in 5...\n");
    sleep( 5 );
  }

  bRun = 1;
  pthread_create( &notifier, NULL, notifierThread, NULL );
  pthread_create( &listener, NULL, listenerThread, NULL );
  pthread_create( &talker, NULL, talkerThread, NULL );
  
  signalHello( );

  while( bRun )
  {
    int i;
    struct timespec ts;

    sleep( 10 );

    clock_gettime(CLOCK_MONOTONIC, &ts);

    picassoPrintStatus( );

    printf( "\nNetwork Diagnostic info - txCnt:%d txErr:%d (Diag:%d/%d) \n", 
      diag.txCnt, diag.txErrCnt, diag.frmCnt, diag.errCnt );

    for( i=0; i<MAX_OTHERS; i++ )
    {
      if( diag.other[i].IP[0] )
      {
        int timeSinceLastMsg = ts.tv_sec - diag.other[i].lastRxTime.tv_sec;

        printf( "  IP:%s rxCnt:%d Err:%d,%d,%d,%d (%d).\n",
          diag.other[i].IP,
          diag.other[i].rxCnt,
          diag.other[i].repeatRxMissingErrCnt,
          diag.other[i].repeatRxExtraErrCnt,
          diag.other[i].unxpctdRxErrCnt,
          diag.other[i].mlfrmdRxErrCnt,
          timeSinceLastMsg );
      }
    }
  }

  return NULL;
}

int initSocketCom( stateChangeCallback fct )
{
  pthread_t monitor;

  printf( "\n\n[Network initialization]\n" );
  // Randomizes the generator
  srand(time(0));
 
  // Reset the diagnostics data
  memset( &diag, 0x00, sizeof( diag ));

  callback = fct;

  pthread_create( &monitor, NULL, monitorThread, NULL );

  return 0;
}


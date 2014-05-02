
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

#include <stdio.h>

#include "serial.h"

#define MAXSRPDATA 100

#define SERIAL_DEBUG 0

// File open (W/R) to the AMA0 serial port
int serialFd = -1;

int picassoAck = 0;
int picassoNak = 0;
int picassoRsp = 0;
char picassoData[ MAXSRPDATA ];

int bRun = 1;

pthread_mutex_t serialMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t serialNotify = PTHREAD_COND_INITIALIZER;

int expectedAnswerLen = 0;

picasso_diag_t LCDDiag;

void resetPicassoHealth( )
{
  LCDDiag.cmdCnt = 0;
  LCDDiag.errCnt = 0;
}

char* getPicassoHealth( )
{
  float rate = 0.0;

  if( LCDDiag.cmdCnt <= 0 ) return "LCD ???";

  rate = (float)LCDDiag.errCnt / (float)LCDDiag.cmdCnt;

  if( rate < 0.01 )
    return "LCD OK";
  else if( rate < 0.05 )
    return "LCD WRN";
  else
    return "LCD ERR";
}


void picassoPrintStatus( )
{
  printf( "\nLCD Diagnostic info - txCmdCnd:%d (%dkB) TxErr:%d RxErr:%d,%d,%d,%d (Diag:%d/%d)\n",
    LCDDiag.txCmdCnt,
    LCDDiag.txByteCnt / 1024,
    LCDDiag.txErr,
    LCDDiag.NakErrCnt,
    LCDDiag.TOutErrCnt,
    LCDDiag.MissErrCnt,
    LCDDiag.ExtraErrCnt,
    LCDDiag.cmdCnt,
    LCDDiag.errCnt );
}


int serialWaitForAnswer( struct timespec *ts )
{
  int ret;

  pthread_mutex_lock(&serialMutex);
  ret = pthread_cond_timedwait( &serialNotify, &serialMutex, ts );
  pthread_mutex_unlock(&serialMutex);

  if( ret != ETIMEDOUT && ret != 0 ) printf( "Unexpected pthread_cond_timedwait ret:%d.\n",ret );

  #if( SERIAL_DEBUG )
  printf( "Got ack:%d nak:%d data:%d (exp:%d)\n", picassoAck, picassoNak, picassoRsp, expectedAnswerLen );
  #endif

  return ret;
}


int serialPutCmd( char* buffer, int size, int answer )
{
  int ret;
  struct timespec ts;

  picassoAck = 0;
  picassoNak = 0;
  picassoRsp = 0;
  expectedAnswerLen = answer;

  if( clock_gettime(CLOCK_REALTIME, &ts) < 0 ) 
  {
    fprintf (stderr, "serialPutCmd - clock_gettime( ) failed: %s\n", strerror(errno));
    return -1;
  }
  ts.tv_sec = ts.tv_sec + 1;

  if( buffer && size )
  {
    if( write( serialFd, buffer, size ) != size )
    {
      fprintf (stderr, "serialPutCmd - write( ) failed: %s\n", strerror(errno));
      LCDDiag.txErr++;
      LCDDiag.errCnt++;
      return -1;
    }
  }

  LCDDiag.cmdCnt++;
  LCDDiag.txCmdCnt++;
  LCDDiag.txByteCnt += size;
  
  // If the expected answer size is -1 this means we don't
  // want to wait for the answer (ACK or NAK)
  if( answer < 0 ) return 0;

  ret = serialWaitForAnswer( &ts );

  if(( ret == ETIMEDOUT ) || ( ret == 0 && picassoNak ))
  {
    char str[ 100 ];
    char tmp[ 8 ];
    int i;
    str[0] = 0;
    for( i=0; i<size; i++ ) { sprintf( tmp, "%02X ", buffer[ i ] ); strcat ( str, tmp ); }
    printf( "Error on command:%s[%d] (%s).\n", str, size, picassoNak ? "NAK" : "TIMEOUT" );
    printf( "  Ack:%d Nak:%d DataRcv:%d (Exp:%d)\n", picassoAck, picassoNak, picassoRsp, expectedAnswerLen );
  }

  if( picassoAck )
  {
    if( answer == 0 )
    {
      return 0;
    }
    else
    {
      if( picassoRsp == answer )
      {
        return 0;
      }
      else if( picassoRsp < answer )
      {
        LCDDiag.errCnt++;
        LCDDiag.MissErrCnt++;
        return -3;
      }
      else if( picassoRsp > answer )
      {
        // This is an anomaly. We got more bytes that we expected for 2 commands (see BUGBUG tags)
        // Treat this as a success for upper layers and ignore this for now (commented line below)
        // LCDDiag.errCnt++;
        LCDDiag.ExtraErrCnt++;
        return 0;
      }
    }
  }
  else if( picassoNak )
  {
    LCDDiag.errCnt++;
    LCDDiag.NakErrCnt++;
    return -4;
  }

  LCDDiag.errCnt++;
  LCDDiag.TOutErrCnt++;
  return -2;
}

void serialGetData( char *buffer, int size )
{
  memcpy( buffer, picassoData, size );
}

void* serialListener( void* param )
{
  // Wait for serial port to be open
  while (serialFd == -1 && bRun ) sleep(1) ;
 
  while( bRun )
  {
    int ret;
    char cmd;

    if(( ret = read (serialFd, &cmd, 1)) == 1 )
    {
      if (cmd == PICASSO_ACK)
      {
        picassoAck = 1;
        if( expectedAnswerLen == 0 )
        {
          if( pthread_cond_signal(&serialNotify) < 0 ) 
          {
            fprintf (stderr, "serialListener - pthread_cond_boadcast( ) failed: %s\n", strerror(errno));
          }
          #if( SERIAL_DEBUG )
          else printf( "Simple ACK.\n" );
          #endif
        }
      }
      else if (cmd == PICASSO_NAK)
      {
        picassoNak = 1;
        if( pthread_cond_signal(&serialNotify) < 0 )
        {
          fprintf (stderr, "serialListener - pthread_cond_boadcast( ) failed: %s\n", strerror(errno));
        }
        #if( SERIAL_DEBUG )
        else printf( "NAK.\n" ); 
        #endif
      }
      else if( picassoRsp < MAXSRPDATA )
      {
        picassoData[ picassoRsp++ ] = cmd;
        if( picassoRsp == expectedAnswerLen )
        {
          if( pthread_cond_signal(&serialNotify) < 0 ) 
          {
            fprintf (stderr, "serialListener - pthread_cond_boadcast( ) failed: %s\n", strerror(errno));
          }
          #if( SERIAL_DEBUG )
          else printf( "Data[%d] ACK.\n", picassoRsp );
          #endif
        }
      }
    }
    else
    {
      printf( "serialListener Unexpected return (%d) from read( ).\n", ret );
    } 
  }

  return (void *)NULL ;
}

void serialFlush( )
{
  tcflush ( serialFd, TCIOFLUSH) ;
  printf( "Serial flushed\n" );
}

int serialConfigure( int baud )
{
  int status;
  struct termios options ;
  speed_t myBaud ;

  switch (baud)
  {
    case     50:	myBaud =     B50 ; break ;
    case     75:	myBaud =     B75 ; break ;
    case    110:	myBaud =    B110 ; break ;
    case    134:	myBaud =    B134 ; break ;
    case    150:	myBaud =    B150 ; break ;
    case    200:	myBaud =    B200 ; break ;
    case    300:	myBaud =    B300 ; break ;
    case    600:	myBaud =    B600 ; break ;
    case   1200:	myBaud =   B1200 ; break ;
    case   1800:	myBaud =   B1800 ; break ;
    case   2400:	myBaud =   B2400 ; break ;
    case   9600:	myBaud =   B9600 ; break ;
    case  19200:	myBaud =  B19200 ; break ;
    case  38400:	myBaud =  B38400 ; break ;
    case  57600:	myBaud =  B57600 ; break ;
    case 115200:	myBaud = B115200 ; break ;
    case 230400:	myBaud = B230400 ; break ;

    default:
      return -2 ;
  }

// Get and modify current options:
  if( tcgetattr (serialFd, &options) == -1 )
  {
    fprintf (stderr, "serialConfigure - tcgetattr( ) failed: %s\n", strerror(errno));
    return -1;
  }

  cfmakeraw   (&options) ;
  cfsetispeed (&options, myBaud) ;
  cfsetospeed (&options, myBaud) ;

  options.c_cflag |= (CLOCAL | CREAD) ;
  options.c_cflag &= ~PARENB ;
  options.c_cflag &= ~CSTOPB ;
  options.c_cflag &= ~CSIZE ;
  options.c_cflag |= CS8 ;
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;
  options.c_oflag &= ~OPOST ;

  options.c_cc [VMIN]  =   0 ;
  options.c_cc [VTIME] = 100 ;	// Ten seconds (100 deciseconds)

  if( tcsetattr (serialFd, TCSANOW | TCSAFLUSH, &options) == -1 )
  {
    fprintf (stderr, "serialConfigure - tcsetattr( ) failed: %s\n", strerror (errno));
    return -1;
  }

  if( ioctl (serialFd, TIOCMGET, &status) == -1 )
  {
    fprintf (stderr, "serialConfigure - ioctl(TIOCMGET) failed: %s\n", strerror (errno));
    return -1;
  }

  status |= TIOCM_DTR ;
  status |= TIOCM_RTS ;

  if( ioctl (serialFd, TIOCMSET, &status) == -1 )
  {
    fprintf (stderr, "serialConfigure - ioctl(TIOCMSET) failed: %s\n", strerror (errno));
    return -1;
  }

  usleep (10000) ;	// 10mS
  printf( "Serial port(%d) now at %d baud.\n", serialFd, baud );

  return 0;
}

int serialOpen(char *device)
{
 
  if( serialFd != -1 )
  {
    serialClose( );
  }
  else
  {
    memset( &LCDDiag, 0x00, sizeof( LCDDiag ));
  }

  if ((serialFd = open (device, O_RDWR | O_NOCTTY | O_NDELAY /*| O_NONBLOCK*/ )) == -1)
  {
    fprintf (stderr, "serialOpen - open(%s) failed: %s\n", device, strerror (errno));
    return -1;
  }

  if( fcntl (serialFd, F_SETFL, O_RDWR) == -1 )
  {
    fprintf (stderr, "serialOpen - fcntl failed: %s\n", strerror (errno));
    return -1;
  }

  printf( "Device '%s' is open %d.\n", device, serialFd );
  return serialFd;
}

void serialStartListener( )
{
  pthread_t serialListenerThread;
  bRun = 1;
  if( pthread_create (&serialListenerThread, NULL, serialListener, NULL) < 0 ) {
    fprintf (stderr, "serialStartListener - pthread_create failed: %s\n", strerror (errno));
  }
  usleep( 50000 );
}

void serialClose( )
{
  bRun = 0;
  close( serialFd );
}


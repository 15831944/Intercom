
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "../inc/socket.h"
#include "syncTimer.h"

#define MAX_TIMERS   10
// #define CLOCK_ID     CLOCK_REALTIME
#define CLOCK_ID     CLOCK_MONOTONIC
	

syncTimer_t timers[MAX_TIMERS];

long deltaInMs( struct timespec a, struct timespec b )
{
  long delta;
  delta = ( a.tv_sec - b.tv_sec ) * 1000;
  delta = delta + ( a.tv_nsec - b.tv_nsec ) / ONE_MS; 
  return delta;
} 

void addMsToTime( struct timespec *ts, int msec )
{
  int sec = msec / 1000;
  int nsec = ( msec % 1000 ) * ONE_MS;

  ts->tv_sec += sec;
  ts->tv_nsec += nsec;
  while( ts->tv_nsec >= ONE_SEC )
  {
    ts->tv_sec++;
    ts->tv_nsec -= ONE_SEC;
  }
}

void syncTimerInit( )
{
  int i;
  struct timespec tspec;

  if( clock_getres(CLOCK_ID, &tspec ) < 0 )
  {
     fprintf (stderr, "addNsToCurrentTime - clock_gettime( ) failed: %s\n", strerror(errno));
  }
  else
  {
    printf( "Clock Res : tsspec sec : %ld  nsec : %ld\n", tspec.tv_sec, tspec.tv_nsec );
  }

  for( i=0; i<MAX_TIMERS; i++ )
  {
    timers[i].active=0;
  }
}

void syncTimerKillTimer( int id )
{
  if( id >=0 && id < MAX_TIMERS )
  {
    timers[id].active = 0;
  }
}


void dumpTimers( )
{
  int i;
  for( i=0; i<MAX_TIMERS;i++ )
  {
    printf( " [%d] %d, %d\n", i, timers[i].active, timers[i].period );
  }
}

int syncTimerSetTimer( int period, void(*callback)(void*), void* arg, int singleShot )
{
  int i;

  for( i=0; i<MAX_TIMERS;i++ )
  {
    if( timers[i].active == 0 )
    {
      timers[i].count = 0;
      timers[i].active = 1;
      timers[i].period = period;
      timers[i].singleShot = singleShot;

      if( clock_gettime(CLOCK_ID, &timers[i].elapsed) < 0 ) {
        fprintf (stderr, "addNsToCurrentTime - clock_gettime( ) failed: %s\n", strerror(errno));
        return -2;
      }
      addMsToTime( &timers[i].elapsed, period );
      timers[i].callback = callback;
      timers[i].arg = arg;
      timers[i].remaining = period - 1;

      return i;
    }
  }

  return -1;
}

int getTimerRemainingTime( int id )
{
  if( id >=0 && id < MAX_TIMERS )
  {
    if( timers[id].active )
    {
      return timers[id].remaining;
    }
  }
  return -1;
}

void syncTimerProcess( )
{
  int i;
  static struct timespec lastTs = {0,0};
  struct timespec ts;

  if( clock_gettime(CLOCK_ID, &ts) < 0 ) {
    fprintf (stderr, "syncTimerProcess - clock_gettime( ) failed: %s\n", strerror(errno));
    return;
  }

  // Some sanity check on time...
  if( ts.tv_sec == lastTs.tv_sec && ts.tv_nsec == lastTs.tv_nsec ) return;
  if( ts.tv_sec <= lastTs.tv_sec && ts.tv_nsec < lastTs.tv_nsec ) { printf( "Back in time!!!\n" ); }
  if( ts.tv_sec > lastTs.tv_sec + 10 && lastTs.tv_sec != 0 ) { printf( "Jump in the future.!!!\n" ); }

  
  // if( ts.tv_sec != lastTs.tv_sec ) printf ("Time is (%ld.%ld)\n", ts.tv_sec,ts.tv_nsec/1000000 );
  
  lastTs = ts;

  for( i=0; i<MAX_TIMERS; i++ )
  {
    if( timers[i].active )
    {
      timers[i].remaining = deltaInMs( timers[i].elapsed, ts );

      /*
      if(( ts.tv_sec > timers[i].elapsed.tv_sec ) || 
         ( ts.tv_sec == timers[i].elapsed.tv_sec && ts.tv_nsec >= timers[i].elapsed.tv_nsec ))
      */
      if( timers[i].remaining <= 0 )
      {
        timers[i].count++;
        //printf( "Timer[%d] elapsed (%d - %ld.%ld)\n", i,timers[i].count,ts.tv_sec,ts.tv_nsec/1000000 );
        if( !timers[i].singleShot )
        { 
          timers[i].elapsed = ts;
          addMsToTime( &timers[i].elapsed, timers[i].period );          
        }
        else
        {
          timers[i].active = 0;
        }
        timers[i].callback( timers[i].arg );
      }
    }
  }
}


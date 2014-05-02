
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "../inc/picasso.h"
#include "../inc/socket.h"
#include "intercom.h"
#include "clock.h"
#include "button.h"
#include "syncTimer.h"
#include "audio.h"

// Uncommenting this will switch to a mode where there is
// an explicit START / STOP button instead of tapping on the
// timer duration button to do the same.
//#define STARTSTOP  1

// There must be a H file to PI defined but I couldn't find it
// so it was easier this way...
#define PI    3.14159265359
#define NOON  (PI+PI/2)
#define SEC   (PI/30)
#define HOUR  (PI/6)

#define FIVE_MINUTES	300000
#define THREE_MINUTES   180000
#define ONE_MINUTE       60000
#define THIRTY_SECONDS   30000

// Defines the location and size of the clock
#define CX ( SCREEN_HEIGHT / 2 ) + 16
#define CY ( SCREEN_HEIGHT / 2 )
#define R  ((SCREEN_HEIGHT / 2 ) - 1)

// This is how time between each reminder. Add an entry to add
// a reminder to the alarm timer.
int reminderTime[] = { 20000, 10000 };

#define REMINDER_COUNT (sizeof(reminderTime)/sizeof(int))
int reminders = -1;

int timerId = -1;
int reminderTimerId = -1;
int timerDuration = THREE_MINUTES;

button_t *durationButton = NULL;
button_t *increaseButton = NULL;
button_t *decreaseButton = NULL;

// Get these from intercom.c  Ugly but well....
extern state_t currentState;
void enterMatrixMode( );

void(*pClockStyleUpdate)( struct tm cNow, struct tm cPrev ) = NULL;
void(*pClockStyleDraw)( ) = NULL;

// In case we want to change the background color of the clock
#define BKGND            BLACK

void drawTheButtons( )
{
  int i=0;
  button_t* pBut;
  while(( pBut=getButton(i++)) != NULL ) drawButton( pBut );
}


void timerReminder( void* arg )
{
  if( reminders > 0 )
  {
    reminders--;
    // printf( "Reminder (%d).\n", reminders );
    playSound( 1 );
    reminderTimerId = syncTimerSetTimer( reminderTime[reminders-1], timerReminder, NULL, 1 );
  }  
}


void timerTimer( void* arg )
{
  // printf( "Timer fired\n" );
  playSound( 1 );
  timerId = -1;

  reminders = REMINDER_COUNT;
  reminderTimerId = syncTimerSetTimer( reminderTime[reminders-1], timerReminder, NULL, 1 );

  if( currentState == stateClock )
  {
    initClock( );
    drawTheButtons( );
  }
}


void actionGoMatrix( void* arg )
{
  durationButton = NULL;
  increaseButton = NULL;
  decreaseButton = NULL;
  enterMatrixMode( );
}


int ackTimerEnd( )
{
  int ACKed = 0;

  if( durationButton->timer != -1 )
  {
    syncTimerKillTimer( durationButton->timer );
    durationButton->timer = -1;
    durationButton->lit = 0;
    ACKed = 1;
  }

  if( reminderTimerId != -1 )
  {
    syncTimerKillTimer( reminderTimerId );
    reminderTimerId = -1;
    ACKed = 1;
  }
  reminders = -1;  
  
  return ACKed;
}


void actionAcknowledgeTimer( void* arg )
{
  int ACKed = ackTimerEnd( );

  sinkTheButton( arg );

#ifndef STARTSTOP
  // This is the case where the timer button triples as the start/stop/ack button 
  if( timerId == -1 && !ACKed )
  {
    if( !ACKed )
    {
      timerId = syncTimerSetTimer( timerDuration, timerTimer, NULL, 1 );
      //printf( "Timer started for %d sec. Id:%d\n", timerDuration / 1000, timerId );
    }
  }
  else
  {
    syncTimerKillTimer( timerId );
    timerId = -1;
  }
#endif

  refreshClock( );
}


void actionIncreaseTimer( void* arg )
{
  if( timerId == -1 )
  {
    sinkTheButton( arg );
    if( timerDuration < FIVE_MINUTES ) timerDuration += THIRTY_SECONDS;
    else timerDuration += ONE_MINUTE;
    
    ackTimerEnd( );
    refreshClock( );
  }
}


void actionDecreaseTimer( void* arg )
{
  if( timerId == -1 )
  {
    sinkTheButton( arg );
    if( timerDuration <= FIVE_MINUTES )
    {
      timerDuration -= THIRTY_SECONDS;
      if( timerDuration < THIRTY_SECONDS ) timerDuration = THIRTY_SECONDS;
    }
    else timerDuration -= ONE_MINUTE;
   
    ackTimerEnd( );
    refreshClock( );
  }
}


#ifdef STARTSTOP
void actionStartTimer( void* arg )
{
  sinkTheButton( arg );

  if( timerId == -1 )
  {
    ackTimerEnd( );
    timerId = syncTimerSetTimer( timerDuration, timerTimer, NULL, 1 );
    printf( "Timer started for %d sec. Id:%d\n", timerDuration / 1000, timerId );
  }
  else
  {
    syncTimerKillTimer( timerId );
    timerId = -1;
  }

  initClock( );
  //printf( "Drawing from actionStartTimer : %s\n", durationButton->txt );
  drawTheButtons( );
}
#endif


char* durationInSecToText( int t )
{
  static char buf[80];
  if( t < 60 )
  {
    sprintf( buf, "%d sec", t );
  }
  else
  {
    if(( t % 60 ) == 0 )
    {
      sprintf( buf, "%d min", t / 60 );
    }
    else
    {
      sprintf( buf, "%dmin %ds", t/60, t%60 );
    }
  }
  return buf;
}


char* getUpTime( )
{
  static char upTimeStr[20];
  static struct timespec start = {-1,-1};
  struct timespec ts;
  long secUpTime;

  if( start.tv_sec == -1 && start.tv_nsec == -1 )
  {
    if( clock_gettime(CLOCK_MONOTONIC, &start) < 0 )
    {
      fprintf (stderr, "getUpTime - clock_gettime( ) failed: %s\n", strerror(errno));
      return "ERR";
    }
  }
  
  if( clock_gettime(CLOCK_MONOTONIC, &ts) < 0 )
  {
    fprintf (stderr, "getUpTime - clock_gettime( ) failed: %s\n", strerror(errno));
    return stateNoChange;
  }

  secUpTime = ts.tv_sec - start.tv_sec;

  if( secUpTime < 60 )
    sprintf( upTimeStr, "(up %dsec)", (int)secUpTime );
  else if( secUpTime < 3600 )
    sprintf( upTimeStr, "(up %dmin)", (int)(secUpTime / 60));
  else if( secUpTime < 86400 )
    sprintf( upTimeStr, "(up %dhr)", (int)(secUpTime / 3600));
  else
    sprintf( upTimeStr, "(up %dd)", (int)(secUpTime / 86400));

  return upTimeStr;
}

char* getCurrentDate( )
{
  time_t now;
  struct tm cNow;
  static char timeStr[100];

  time( &now );
  localtime_r( &now, &cNow );

  sprintf( timeStr, "%02d/%02d/%04d",
    cNow.tm_mon + 1,
    cNow.tm_mday,
    cNow.tm_year + 1900);

  return timeStr;
}

char* getCurrentTime( )
{
  time_t now;
  struct tm cNow;
  static char timeStr[100];

  time( &now );
  localtime_r( &now, &cNow );

  sprintf( timeStr, "%02d:%02d:%02d %s",
    cNow.tm_hour,
    cNow.tm_min,
    cNow.tm_sec,
    getUpTime( ));

  return timeStr;
}

void drawFatNeedle( float a, int l, int color, int width )
{
  int x,y;
  int dx,dy,pdx,pdy;
  int x1,x2,x3,x4;
  int y1,y2,y3,y4;

  dx = l*cosf(a);
  dy = l*sinf(a);

  x = CX + dx;
  y = CY + dy;

  pdx = -dy * width / l;
  pdy = dx * width / l;

  x1 = x + pdx;
  y1 = y + pdy;
  x2 = CX + pdx;
  y2 = CY + pdy;
  x3 = CX - pdx;
  y3 = CY - pdy;
  x4 = x - pdx;
  y4 = y - pdy;

  picassoSetColor( color );
  picassoDrawTriangle( x1,y1,x2,y2,x3,y3, 1 );
  picassoDrawTriangle( x1,y1,x4,y4,x3,y3, 1 );
}


void drawPointyNeedle( float a, int l, int color, int wide )
{
  int x1,y1;

  x1 = CX + l*cosf(a);
  y1 = CY + l*sinf(a);
  picassoSetColor( color );

  if( wide )
  {
    int x2,y2,x3,y3;
    x2 = CX + 10*cosf(a-PI/2);
    y2 = CY + 10*sinf(a-PI/2);
    x3 = CX + 10*cosf(a+PI/2);
    y3 = CY + 10*sinf(a+PI/2);
    picassoDrawTriangle( x1,y1,x2,y2,x3,y3,1 );
  }
  else
  {
    picassoDrawLine( CX,CY,x1,y1 );
    picassoDrawCircle( x1,y1,5,1 );
  }
}

void Style70sDraw( )
{
  int i;

  picassoSetColor( WHITE );
  picassoDrawCircle( CX,CY,R,0 );
  picassoDrawCircle( CX,CY,R-1,0 );

  for(i=0;i<12;i++)
  {
    int x1,y1;
    int x2,y2;
    float a = i*(PI/6);

    x1 = CX + R*cosf(a);
    y1 = CY + R*sinf(a);
    x2 = CX + (R-10)*cosf(a);
    y2 = CY + (R-10)*sinf(a);
    picassoDrawLine( x1,y1,x2,y2 );
  }
}

void Style70sUpdate( struct tm cNow, struct tm cPrev )
{
    if( cNow.tm_min != cPrev.tm_min )
    {
      drawPointyNeedle( NOON + HOUR * cPrev.tm_hour + (SEC / 12 * cPrev.tm_min), R - 55, BKGND, 1 );  
      drawPointyNeedle( NOON + SEC * cPrev.tm_min, R - 15, BKGND, 1 );
    }
 
    drawPointyNeedle( NOON + SEC * cPrev.tm_sec, R - 20, BKGND, 0 );
    // Erase and redraw the seconds immediately so that 
    drawPointyNeedle( NOON + SEC * cNow.tm_sec, R - 20, RED, 0 );
    drawPointyNeedle( NOON + SEC * cNow.tm_min, R - 15, WHITE, 1 );
    drawPointyNeedle( NOON + HOUR * cNow.tm_hour + (SEC / 12 * cNow.tm_min), R - 55, WHITE, 1 );
    // Draw the seconds again so that it appears above the minute and hour needle
    drawPointyNeedle( NOON + SEC * cNow.tm_sec, R - 20, RED, 0 );
}


void StyleIkeaDrawNumber( int n )
{
  // This formula retuns the 1/6th of Pi for a given clock digit (1..12)
  // 1>10, 2>11, 3>0, 4>1, etc...
  int i = (n+9)%12;
  int x,y;
  float a = i*(PI/6);
  char str[3];

  picassoSetTextColor( BLACK );
  picassoSetTextBackground( WHITE );
  picassoSetTextSize( 3 );

  sprintf( str, "%d",n );

  x = CX + (R-16)*cosf(a) - picassoGetLetterWidth( ) * ((n>=10)?2:1) - 2;
  y = CY + (R-16)*sinf(a) - picassoGetLetterHeight( ) - 2;

  if( n == 10 ) x = x + 4;
  if( n == 11 ) x = x + 3;
  if( n == 12 ) x = x - 4;
 
  picassoPutText( x, y, str );
}

void StyleIkeaDraw( )
{
  int i;

  picassoSetColor( WHITE );
  picassoDrawCircle( CX,CY,R,1 );

  for(i=1;i<=12;i++) StyleIkeaDrawNumber( i );
}

int clockDigitUnder( int s )
{
  // This formula returns the digit (1..12) which is the closest to the seconds(0..59)
  // Works also for minutes.
  // 0>12, 1>12, 2>12, 3>1, 4>1, etc... 
  return (( s + 57 ) % 60 ) / 5 + 1;
}

void StyleIkeaUpdate( struct tm cNow, struct tm cPrev )
{
  int undrMin,undrSec;

  undrSec = clockDigitUnder( cPrev.tm_sec );
  // printf( "Digit under seconds needle at %d is %d.\n", cPrev.tm_sec, undrSec );

  if( cNow.tm_min != cPrev.tm_min )
  {
    drawFatNeedle( NOON + HOUR * cPrev.tm_hour + (SEC / 12 * cPrev.tm_min), R - 60, WHITE, 5 );  
    drawFatNeedle( NOON + SEC * cPrev.tm_min, R - 30, WHITE, 5 );
    undrMin = clockDigitUnder( cPrev.tm_min );
  }
  else undrMin = undrSec;
 
  drawPointyNeedle( NOON + SEC * cPrev.tm_sec, R - 30, WHITE, 0 );

  StyleIkeaDrawNumber( undrSec );
  if( undrSec != undrMin ) StyleIkeaDrawNumber( undrMin );
 
  picassoSetColor( BLACK );
  picassoDrawCircle( CX,CY,16,1 );

  drawPointyNeedle( NOON + SEC * cNow.tm_sec, R - 30, RED, 0 );

  drawFatNeedle( NOON + SEC * cNow.tm_min, R - 30, BLACK, 5 );
  drawFatNeedle( NOON + HOUR * cNow.tm_hour + (SEC / 12 * cNow.tm_min), R - 60, BLACK, 5 );
  // Draw the seconds again so that it appears above the minute and hour needle
  drawPointyNeedle( NOON + SEC * cNow.tm_sec, R - 30, RED, 0 );

}


int refreshClock( )
{
  time_t now;
  static time_t prev;
  static struct tm cPrev;
  static int prevDuration = -1;
  static int prevRemaining = -1;
  static int random = -1;
  struct tm cNow;
  char durationBtnTxt[80];
  int timerIdle = ( timerId == -1 );
  int reboot = 0;

  time( &now );

  durationBtnTxt[0] = 0;

  // First, refresh the content of the duration button
  if( timerIdle )
  {
    if( prevDuration != timerDuration )
    {
      strcpy( durationBtnTxt, durationInSecToText( timerDuration / 1000 ));
      prevDuration = timerDuration;
    }
  }
  else
  {
    // This is to get the duration updated at the end of the timer.
    prevDuration = -1;
    int remainging = 1 + getTimerRemainingTime( timerId ) / 1000;
    if( prevRemaining != remainging )
    {
      strcpy( durationBtnTxt, durationInSecToText( remainging ));
      // printf( "Drawing from refresh clock (timerId!=-1) : %s\n", durationButton->txt );
      prevRemaining = remainging;
    }
  }

  // If the content is different than the current button display
  if( durationBtnTxt[0] && strcmp( durationBtnTxt, durationButton->txt ))
  {
    int oldTxtLength = strlen( durationButton->txt );
    int newTxtLength = strlen( durationBtnTxt );

    strcpy( durationButton->txt, durationBtnTxt );

    if( newTxtLength < oldTxtLength )
      // Draw the whole button
      drawButton( durationButton );
    else
      // Just draw the text to avoid flashing when redrawing the background
      drawButtonText( durationButton );
  }

  if( increaseButton->lit != timerIdle )
  {
    increaseButton->lit = timerIdle;
    drawButton( increaseButton );
  }

  if( decreaseButton->lit != timerIdle )
  {
    decreaseButton->lit = timerIdle;
    drawButton( decreaseButton );
  }

  // Then if the current time has changed (by at least 1 sec), redraw clock the needles
  if( memcmp( &now, &prev, sizeof( now )))
  {
    localtime_r( &now, &cNow );

    if( random == -1 ) random = 1 + ( rand( ) % 10 );

    // Between 7pm and 7am, dim the backlight
    if( cNow.tm_hour >= 19 || cNow.tm_hour < 6 ) picassoSetContrast( 1 );

#if 0
    if( cNow.tm_hour == 7 ) 
    {
      int ipLen = strlen(getMyIP( ));
      // 7 is the length of the shortest IP string: "w.x.y.z". Use "z" as the pseudo unique way of
      // rebooting each station every day at a different time.
      if( ipLen >= 7 )
      {
        if( cNow.tm_min == getMyIP( )[ipLen-1] - '0' ) reboot = 1;
      }
      else
      {
        if( cNow.tm_min == random ) reboot = 1;
      } 
    }
    
    // No IP connectivity and clock is at the default time
    if( cNow.tm_hour == 16 && strlen(getMyIP( )) < 7 )
    {
       if( cNow.tm_min == random ) reboot = 1;
    }
#endif

    pClockStyleUpdate( cNow, cPrev );
  
    cPrev = cNow;
    prev = now;
  }
  return reboot;
}


void drawClock( )
{
  picassoClearScreen( );
 
  //printf( "Drawing from drawClock : %s\n", durationButton->txt );
  drawTheButtons( ); 

  pClockStyleDraw( );
}



void initClock( )
{
  int bTimerActif = (timerId != -1);
  int r,c;
  int o = 0;
#ifdef STARTSTOP
  button_t *pBut;
#else
  o = 16;
#endif

  //pClockStyleUpdate = Style70sUpdate;
  //pClockStyleDraw = Style70sDraw;

  pClockStyleUpdate = StyleIkeaUpdate;
  pClockStyleDraw = StyleIkeaDraw;

  destroyAllButtons( );

  buttonCreate( 0,0,SCREEN_HEIGHT,SCREEN_HEIGHT, TRANSPARENT_KEY, "", actionGoMatrix );

  r = 0;c = 4;
  durationButton = getButton( buttonCreate( 5+c*80, 5+r*80+o, 75+(c+1)*80, 75+r*80+o, 
    BLACK, 
    durationInSecToText( bTimerActif ? ( 1 + getTimerRemainingTime( timerId ) / 1000 ) : timerDuration / 1000 ),
    actionAcknowledgeTimer ));
  durationButton->lit = 0;

  if( reminders >= 0 && durationButton->timer == -1 )
  {
    durationButton->timer = syncTimerSetTimer( 750, blinkButtonTimer, durationButton, 0 );
  }

  r = 1;c = 4;
  decreaseButton = getButton( buttonCreate( 5+c*80, 5+r*80+o, 75+c*80, 75+r*80+o, BLUE, "-", actionDecreaseTimer ));
  decreaseButton->lit = !bTimerActif;
  
  r = 1;c = 5;
  increaseButton = getButton( buttonCreate( 5+c*80, 5+r*80+o, 75+c*80, 75+r*80+o, RED, "+", actionIncreaseTimer ));
  increaseButton->lit = !bTimerActif;

#ifdef STARTSTOP
  r = 2;c = 4;
  pBut = getButton( buttonCreate( 5+c*80, 5+r*80+o, 75+(c+1)*80, 75+r*80+o, 
     bTimerActif ? YELLOW : GREEN, 
     bTimerActif ? "STOP" : "START", 
     actionStartTimer ));
  pBut->lit = 1;
#endif
}

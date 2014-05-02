/*
 * intercom.c:
 *
 ***********************************************************************
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>

#include "../inc/socket.h"
#include "../inc/picasso.h"
#include "intercom.h"
#include "syncTimer.h"
#include "button.h"
#include "clock.h"
#include "audio.h"

// Command line flags
int bTestMode = 0;
int bMaster = 0;
int bBoot = 0;
int bFakeErr = 0;


button_t *pAckButton = NULL;
int comReminderTimerId = -1;

// Device button states
int localComState = 0;
int remoteComState = 0;
int newRemoteState = 0;

state_t currentState = stateMatrix;

static struct timespec maskTouch = {0,0};


void refreshMatrix( )
{
  int i=0;
  button_t* pBut;
  char status[256];
  static char prevStatus[256];

  while(( pBut = getButton(i)) != NULL )
  {
    if( i != pAckButton->idx )
    {
      int state = (localComState & ( 1 << i )) | (remoteComState & ( 1 << i ));
      if( pBut->lit != state )
      {
        pBut->lit = state;
        drawButton( pBut );
      }
    }
    i++;
  }

  if(( localComState || remoteComState ) && ( pAckButton->timer == -1 ))
  {
    pAckButton->timer = syncTimerSetTimer( 750, blinkButtonTimer, pAckButton, 0 );
  }
  else if(( !localComState && !remoteComState ) && ( pAckButton->timer != -1 ))
  {
    syncTimerKillTimer( pAckButton->timer );
    pAckButton->timer = -1;
    if( pAckButton->lit )
    {
      pAckButton->lit = 0;
      drawButton( pAckButton );
    }
  }
  
  sprintf( status, "%s - %s - %s - %s - %s", 
    VERSION_STR,
    getCurrentTime( ), 
    getMyIP( ), 
    getNetworkHealth( ),
    getPicassoHealth( ));

  if( strcmp( status, prevStatus ))
  {
    int xPos,yPos;

    xPos = 5;
    yPos = SCREEN_HEIGHT - picassoGetLetterHeight( 'A' ) - 1; 

    // If the new text is shorter, erase the background
    if( strlen( status ) < strlen( prevStatus ))
    {
      picassoSetColor( BLACK );
      picassoDrawRect( xPos, yPos, SCREEN_WIDTH, SCREEN_HEIGHT, 1 );
      // printf( "Erasing the background.\n" );
    }

    picassoSetTextBackground( BLACK );
    picassoSetTextColor( GRAY );
    picassoSetTextSize( 1 );
    picassoPutText( xPos, yPos, status );
    
    strcpy( prevStatus, status );
  }

}




void drawMatrix( )
{
  int i=0;
  button_t* pBut;
  

  picassoClearScreen( );
  picassoSetContrast( 9 );

  while(( pBut=getButton(i++)) != NULL )
  {
    drawButton( pBut );
  }

  refreshMatrix( );
}


void maskTouchFor( int ms )
{
  if( clock_gettime(CLOCK_MONOTONIC, &maskTouch) < 0 )
  {
    fprintf (stderr, "maskTouchFor - clock_gettime( ) failed: %s\n", strerror(errno));
    maskTouch.tv_sec = 0;
    maskTouch.tv_nsec = 0;
    return;
  }
  addMsToTime( &maskTouch, ms );
}


void reminderSoundTimer( void* arg )
{
  playSound( soundNotify );
}


void newStateCallback( int state )
{
  if( state && remoteComState == 0 )
  {
    playSound( soundNotify );
    if( comReminderTimerId != -1 ) syncTimerKillTimer( comReminderTimerId );
    comReminderTimerId = syncTimerSetTimer( 5000, reminderSoundTimer, NULL, 0 );
  }
  else if( state == 0 && comReminderTimerId != -1 )
  {
    syncTimerKillTimer( comReminderTimerId );
    comReminderTimerId = -1;
  }

  newRemoteState = 1;
  remoteComState = state;
  localComState = 0;

  // Disable touch for 3/4 sec to avoid accidentally acknowledging a
  // message which arrives as someone was about to press on the screen
  maskTouchFor( 750 );
}


void actionAcknowledge( void* arg )
{
  button_t* pBut = (button_t*)arg;

  sinkTheButton( pBut );

  localComState = 0;
  remoteComState = 0;
  signalNewState( 0 );

  if( comReminderTimerId != -1 )
  {
    syncTimerKillTimer( comReminderTimerId );
    comReminderTimerId = -1;
  }

  refreshMatrix( );
}

void actionNormalButton( void* arg )
{
  int mask;
  button_t* pBut = (button_t*)arg;

  sinkTheButton( pBut );
  
  mask = (1 << pBut->idx);

  // Pressing on any button implicitely acknowledges the message.
  remoteComState = 0;
  if( comReminderTimerId != -1 )
  {
    syncTimerKillTimer( comReminderTimerId );
    comReminderTimerId = -1;
  }

  if(( mask & localComState ) == 0 )
    localComState = localComState | mask;
  else
    localComState = localComState & ~mask;

  signalNewState( localComState );

  refreshMatrix( );
}

/*
#define ROW 3
#define COL 6

int colorPatern[] = { WHITE,WHITE,WHITE,WHITE,BLUE,WHITE,
                      WHITE,WHITE,WHITE,RED,GREEN,YELLOW,
                      WHITE,WHITE,WHITE,WHITE,WHITE,WHITE };

char* txtPatern[] = { "DR", "AST", "HYG", "RCP", "",   "",
                      "1",  "2",   "3",   "",    "",   "",
                      "1",  "2",   "3",   "4",   "5",  "6" }; 
*/

#define ROW 3
#define COL 5

int colorPatern[] = { WHITE ,WHITE ,WHITE ,GREEN ,WHITE,
                      WHITE ,RED   ,YELLOW,BLUE  ,WHITE,
                      WHITE ,WHITE ,WHITE ,WHITE ,WHITE };

char* txtPatern[] = { "DR", "RCP", "AST",   "",    "",
                        "",    "",    "",   "",  "LAB",
                       "1",   "2",   "3", "FRT", "OFF" }; 

void initMatrix( )
{
  int i=0;
  int xOffset;
  int c,r;

  xOffset = 5 + ( 6 - COL ) * 40;

  destroyAllButtons( );

  for( r=0; r<ROW; r++ )
  {
    for( c=0; c<COL; c++ )
    {
      buttonCreate( xOffset+c*80, 5+r*80, 
                    xOffset+70+c*80, 75+r*80, 
                    colorPatern[i], txtPatern[i], 
                    actionNormalButton );
      i++;
    }
  }

  pAckButton = getButton( COL - 1 ); 
  // Override the action of the acknowledge button
  pAckButton->action = actionAcknowledge;
}


void testModeStateCallback( int state )
{
  remoteComState = state;
}


void testMode( )
{
  initSocketCom( newStateCallback );

  while( 1 )
  { 
    int i,n,s;
    sleep( 15 );

    s = 0;
    for( i=0; i<4; i++ )
    {
      // pick any button but the
      do { n=rand( ) % 18; } while ((n==5) || (s & (1<<n)));
      s = s | (1<<n);
      signalNewState( s );
      usleep( 250000 + ( rand( ) % 250000 ));
    }
    printf( "Reported state %X.\n", s );
  }
}


int newStateTransition( int *touch )
{
  static struct timespec idle = {0,0};
  struct timespec ts;

  if( clock_gettime(CLOCK_MONOTONIC, &ts) < 0 )
  {
    fprintf (stderr, "idleProcess - clock_gettime( ) failed: %s\n", strerror(errno));
    return stateNoChange;
  }


  if( *touch || localComState || remoteComState || idle.tv_sec == 0 )
  {
    idle = ts;
    addMsToTime( &idle, 3000 );
  }
    
  // Mask touch imput for as long as current time is before "mask" time
  if(( maskTouch.tv_sec > ts.tv_sec ) || ( maskTouch.tv_sec == ts.tv_sec && maskTouch.tv_nsec > ts.tv_nsec ))
  {
     *touch = 0;
  }

  if( currentState == stateMatrix && ts.tv_sec >= idle.tv_sec && ts.tv_nsec > idle.tv_nsec )
  {
    return stateClock;
  }

  if( newRemoteState && currentState == stateClock )
  {
    return stateMatrix;
  }

  return stateNoChange;
}


void enterMatrixMode( )
{
  currentState = stateMatrix;
  destroyAllButtons( );
  initMatrix( );
  drawMatrix( );

  // Mask the touch events for 500ms to avoid spurious activations.
  maskTouchFor( 500 ); 
}

void enterClockMode( )
{
  currentState = stateClock;
  destroyAllButtons( );
  initClock( );
  drawClock( );
}

void resetDiagInfo( void* arg )
{
  resetPicassoHealth( );
  resetNetworkHealth( );
}

int main (int argc, char** argv)
{
  int bRun = 0;
  int bReboot = 0;
  int i;

  syncTimerInit( );

  initAudio( );

  for(i=0;i<argc;i++) {
    if(strcmp(argv[i],"-test")==0)     bTestMode = 1;
    if(strcmp(argv[i],"-master")==0)   bMaster = 1;
    if(strcmp(argv[i],"-boot")==0)     bBoot = 1;
    if(strcmp(argv[i],"-err")==0)      bFakeErr = 1;
  }
  
  printf("\n\n");

  if( bTestMode ) testMode( );

  // If any critical initialization in this block fail, bRun will be false
  // and the main loop will not run and the program will exit.
  do
  {
    bRun = 0;

    if( picassoInit( ) < 0 ) { printf( "picassoInit( ) Failed.\n"); break; }

    // One time initializations
    if( picassoSetScreenOrientation( 1 ) < 0 ) break;
    if( picassoEnableTouch( ) < 0 ) break;

    // BUGBUG : Moved this prior to the network because the threads created
    // by initSocketCom were somehow messing with serial communication.
    enterMatrixMode( );
    
    if( initSocketCom( newStateCallback ) < 0 ) break;

    bRun = 1;
  
  } while( 0 );

  // Initialize the random generator seed
  srand(time(NULL));

  // Every five minutes, reset the diag counters
  syncTimerSetTimer( 300000, resetDiagInfo, NULL, 0 );

  // This is our main loop
  while( bRun )
  {
    int x,y,touchEvt;

    // 30ms between touch polling
    usleep( 30000 );

    touchEvt = picassoGetTouch( &x, &y );

    // Change between the main application states
    switch( newStateTransition( &touchEvt ))
    {
    case stateClock :
      enterClockMode( );
      break;

    case stateMatrix :
      enterMatrixMode( );
      break;
    }

    switch( currentState )
    {
    case stateNoChange :
      // This is here merely to make the compiler happy
      break;
    case stateClock :
      bReboot = refreshClock( );
      if( bReboot ) bRun = 0;
      break;
    case stateMatrix :
      refreshMatrix( );
      break;
    case stateTimer :
      break;
    case stateStopWatch :
      break;
    }

    if( touchEvt == 1 )
    {
      button_t *pBut = findButton( x, y );
      if( pBut )
      {
        if( pBut->action ) pBut->action( pBut );
      }
    }

    newRemoteState = 0;
    
    syncTimerProcess( );
  }

  picassoDeinit( );

  if( bReboot ) system( "reboot" );

  return 0 ;
}

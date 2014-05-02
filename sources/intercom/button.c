
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "syncTimer.h"
#include "button.h"
#include "../inc/picasso.h"

#define MAX_BUTTONS	32

int matrixCnt = 0;
button_t matrix[MAX_BUTTONS];


int letterWidth = -1;
int letterHeight = -1;


void releaseButton( void* arg )
{
  button_t* pBut = (button_t*)arg;
  pBut->pressed = 0;
  drawButton( pBut );
}


void sinkTheButton( button_t* pBut )
{
  pBut->pressed = 1;
  drawButton( pBut );
  syncTimerSetTimer( 100, releaseButton, pBut, 1 );
}

void blinkButtonTimer( void* arg )
{
  button_t* pBut = (button_t*)arg;
  pBut->lit = !pBut->lit;
  drawButton( pBut );
}

button_t* getButton( int idx )
{
  if( idx < 0 || idx >= matrixCnt ) return NULL;
  return &matrix[ idx ];
}

button_t* findButton( int x, int y )
{
  int i;
  button_t *pBut;
  for( i=0; i<matrixCnt; i++ )
  {
    pBut = &matrix[ i ];
    if(( pBut->x1 <= x ) && ( pBut->x2 >= x ) && ( pBut->y1 <= y ) && ( pBut->y2 >= y )) return pBut;
  }
  return NULL;
}

void destroyButton( button_t* pBut )
{
  if( pBut->timer != -1 ) syncTimerKillTimer( pBut->timer );
  pBut->timer = -1; 
  pBut->x1 = -1;
  pBut->y1 = -1;
  pBut->x2 = -1;
  pBut->y2 = -1;
  pBut->color = -1;
  pBut->txt[0] = 0;
  pBut->lit = 0;
  pBut->action = NULL;
  pBut->idx = -1;
}

void destroyAllButtons( )
{
  int i;
  for(i=0;i<matrixCnt;i++)
  {
    destroyButton( &matrix[i] );
  }
  matrixCnt = 0;
}

int buttonCreate( int x1, int y1, int x2, int y2, int c, char* txt, actionCallback callback )
{
  button_t* pBut;

  pBut = &matrix[ matrixCnt ];
  pBut->x1 = x1;
  pBut->y1 = y1;
  pBut->x2 = x2;
  pBut->y2 = y2;
  pBut->color = c;
  strcpy( pBut->txt, txt );

  pBut->pressed = 0;
  pBut->lit = 0;
  pBut->action = callback;
  pBut->idx = matrixCnt;
  pBut->timer = -1;

  matrixCnt++;

  return pBut->idx;
}

int darkenColor( int c, int percent )
{
  int r = (GET_RED( c ) * percent) / 100;
  int g = (GET_GREEN( c ) * percent) / 100;
  int b = (GET_BLUE( c ) * percent) / 100;
  return RGB(r,g,b);
}

// SD : Sunken Depth
#define SD 4

int getCenterColor( button_t *pBut )
{
  return darkenColor( pBut->color, pBut->lit ? 100 : 15 );
}

void drawButtonText( button_t *pBut )
{
  int text;
  int l = strlen( pBut->txt );
  int size = ( l > 1 ) ? 2 : 3;
  int sink = pBut->pressed ? SD : 0;
  int background = getCenterColor( pBut );

  int width = l * size * picassoGetLetterWidth( );
  int height = size * picassoGetLetterHeight( );

  int xOff = ( pBut->x2 - pBut->x1 - width ) / 2;
  int yOff = ( pBut->y2 - pBut->y1 - height ) / 2;

  if( pBut->color == BLACK )
    text = pBut->lit ? BLACK : WHITE;
  else
    text = pBut->lit ? darkenColor( pBut->color, 30 ) : BLACK;

  picassoSetTextBackground( background );
  picassoSetTextColor( text );
  picassoSetTextSize( size );

  picassoPutText( pBut->x1+xOff+sink, pBut->y1+yOff+sink, pBut->txt );
}

void drawButton( button_t *pBut )
{
  int frame = darkenColor( pBut->color, pBut->lit ? 80 : 60 );
  int center = getCenterColor( pBut );
  int sink = pBut->pressed ? SD : 0;
  
  if( pBut->color == TRANSPARENT_KEY )
  {
    return;
  }

  if( pBut->color == BLACK ) frame = GRAY;

  picassoSetFont( 0 );  
  picassoSetTextSize( 1 );
  picassoSetColor( BLACK );

  if( sink )
  {
    picassoDrawRect( pBut->x1,pBut->y1,pBut->x2+sink,pBut->y1+sink,1 );
    picassoDrawRect( pBut->x1,pBut->y1+sink,pBut->x1+sink,pBut->y2+sink,1 );
  }
  else
  {
    picassoDrawRect( pBut->x2+1,pBut->y1,pBut->x2+SD,pBut->y2+SD,1 );
    picassoDrawRect( pBut->x1,pBut->y2+1,pBut->x2,pBut->y2+SD,1 );
  }
  
  picassoSetColor( frame );
  picassoDrawRect( pBut->x1+sink,pBut->y1+sink,pBut->x2+sink,pBut->y2+sink,0 );
  picassoSetColor( center );
  picassoDrawRect( pBut->x1+1+sink,pBut->y1+1+sink,pBut->x2-1+sink,pBut->y2-1+sink,1 );

  if( pBut->txt[0] )
  {
    drawButtonText( pBut );
  }
}

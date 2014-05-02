
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "serial.h"
#include "picasso.h"

#define HIBYTE( x )  (( x & 0xFF00 ) >> 8 )
#define LOBYTE( x )  ( x & 0x00FF )

#define MAKEWORD( h,l ) (( h << 8 ) + l )

int currentColor = WHITE;

int picassoSetColor( int c )
{
  int previous = currentColor;
  currentColor = c;
  return previous;
}

int picassoDrawLine( int x1, int y1, int x2, int y2 )
{
  char buffer[ 12 ];
  buffer[ 0 ] = 0xFF;
  buffer[ 1 ] = 0xC8;
  buffer[ 2 ] = HIBYTE( x1 );
  buffer[ 3 ] = LOBYTE( x1 );
  buffer[ 4 ] = HIBYTE( y1 );
  buffer[ 5 ] = LOBYTE( y1 );
  buffer[ 6 ] = HIBYTE( x2 );
  buffer[ 7 ] = LOBYTE( x2 );
  buffer[ 8 ] = HIBYTE( y2 );
  buffer[ 9 ] = LOBYTE( y2 );
  buffer[ 10 ] = HIBYTE( currentColor );
  buffer[ 11 ] = LOBYTE( currentColor );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoDrawCircle( int x, int y, int r, int fill )
{
  char buffer[ 10 ];
  buffer[ 0 ] = 0xFF;
  buffer[ 1 ] = ( fill ? 0xC2 : 0xC3 );
  buffer[ 2 ] = HIBYTE( x );
  buffer[ 3 ] = LOBYTE( x );
  buffer[ 4 ] = HIBYTE( y );
  buffer[ 5 ] = LOBYTE( y );
  buffer[ 6 ] = HIBYTE( r );
  buffer[ 7 ] = LOBYTE( r );
  buffer[ 8 ] = HIBYTE( currentColor );
  buffer[ 9 ] = LOBYTE( currentColor );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoDrawRect( int x1, int y1, int x2, int y2, int fill )
{
  char buffer[ 12 ];
  buffer[ 0 ] = 0xFF;
  buffer[ 1 ] = ( fill ? 0xC4 : 0xC5 );
  buffer[ 2 ] = HIBYTE( x1 );
  buffer[ 3 ] = LOBYTE( x1 );
  buffer[ 4 ] = HIBYTE( y1 );
  buffer[ 5 ] = LOBYTE( y1 );
  buffer[ 6 ] = HIBYTE( x2 );
  buffer[ 7 ] = LOBYTE( x2 );
  buffer[ 8 ] = HIBYTE( y2 );
  buffer[ 9 ] = LOBYTE( y2 );
  buffer[ 10 ] = HIBYTE( currentColor );
  buffer[ 11 ] = LOBYTE( currentColor );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoDrawTriangle( int x1, int y1, int x2, int y2, int x3, int y3, int fill )
{
  char buffer[ 16 ];
  buffer[ 0 ] = 0xFF;
  buffer[ 1 ] = ( fill ? 0xA9 : 0xBF );
  buffer[ 2 ] = HIBYTE( x1 );
  buffer[ 3 ] = LOBYTE( x1 );
  buffer[ 4 ] = HIBYTE( y1 );
  buffer[ 5 ] = LOBYTE( y1 );
  buffer[ 6 ] = HIBYTE( x2 );
  buffer[ 7 ] = LOBYTE( x2 );
  buffer[ 8 ] = HIBYTE( y2 );
  buffer[ 9 ] = LOBYTE( y2 );
  buffer[ 10] = HIBYTE( x3 );
  buffer[ 11] = LOBYTE( x3 );
  buffer[ 12] = HIBYTE( y3 );
  buffer[ 13] = LOBYTE( y3 );
  buffer[ 14 ] = HIBYTE( currentColor );
  buffer[ 15 ] = LOBYTE( currentColor );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoSetOrigin( int x, int y )
{
  char buffer[6] = {0xFF,0xCC,0x00,0x00,0x00,0x00};
  buffer[2] = HIBYTE( x );
  buffer[3] = LOBYTE( x );
  buffer[4] = HIBYTE( y );
  buffer[5] = LOBYTE( y );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoSetTextBackground( unsigned short b )
{
  static unsigned short current = -1;
  char buffer[ 4 ] = {0xFF,0xE6,0x00,0x00};
  if( current == b ) return 0;
  buffer[2] = HIBYTE( b );
  buffer[3] = LOBYTE( b );
  current = b;
  return serialPutCmd( buffer, sizeof(buffer),2);
}


int picassoSetTextColor( unsigned short c )
{
  static unsigned short current = -1;
  char buffer[ 4 ] = {0xFF,0xE7,0x00,0x00};
  if( current == c ) return 0;
  buffer[2] = HIBYTE( c );
  buffer[3] = LOBYTE( c );
  current = c;
  // BUGBUG : See comments in picassoPutText( ) for the same problem...
  // If the LCD gives too many timeout, set the last value to 1
  return serialPutCmd( buffer, sizeof(buffer),1);
}


int picassoSetTextSize( int s )
{
  int ret;
  static int currentSize = -1;
  char buffer[ 4 ] = {0xFF,0xE4,0x00,0x00};
  if( s < 0 || s > 16 ) return -2;
  if( currentSize == s ) return 0;

  buffer[3] = s;
  ret = serialPutCmd( buffer, sizeof(buffer),2);
  if( ret < 0 ) return ret;
  
  buffer[ 1 ] = 0xE3;
  buffer[ 3 ] = s;

  ret = serialPutCmd( buffer, sizeof(buffer),2);
  if( ret < 0 ) return ret;

  currentSize = s;
  return 0;
}


int picassoSetScreenOrientation( int o )
{
  char buffer[ 4 ] = {0xFF,0x9E,0x00,0x00};
  buffer[2] = HIBYTE( o );
  buffer[3] = LOBYTE( o );
  return serialPutCmd( buffer, sizeof(buffer),2);
}

int picassoEnableClipping( )
{
  char buffer[ 4 ] = {0xFF,0xA2,0x00,0x01};
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoSetClipping( int i )
{
  char buffer[ 10 ] = {0xFF,0xB5,0x00,0x00,0x00,0x00};
  buffer[6] = HIBYTE( i ? SCREEN_WIDTH : 1 );
  buffer[7] = LOBYTE( i ? SCREEN_WIDTH : 1 );
  buffer[8] = HIBYTE( i ? SCREEN_HEIGHT : 1 );
  buffer[9] = LOBYTE( i ? SCREEN_HEIGHT : 1 );
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoPutText( int x, int y, char* txt )
{
  int len = strlen( txt );
  char buffer[ 100 ] = {0x00,0x18};

  if( len == 0 ) return 0;
  if( picassoSetOrigin( x,y ) < 0 ) return -1;

  strcpy( buffer+2, txt );

  // BUGBUG : Sometimes the panel fails to send the second byte of the
  // string length it's supposed to to reply. It only sends one.
  // Only waiting for for 1 byte shouldn't cause any problems unless we
  // send another command way too soon after.
  // UPDATE: Problem seem to be gone. If the LCD gives too many timeout,
  // set the last value to 1 instead of 2
  return serialPutCmd( buffer, strlen( txt )+3, 1);
}

int picassoSetFont( int f )
{
  char buffer[ 4 ] = {0xFF,0xE5,0x00,0x00};
  if( f <= 0 && f > 2 ) return -2;
  buffer[3] = f;
  return serialPutCmd( buffer, sizeof(buffer),2);
}


int picassoGetLetterWidth( )
{
  int ret;
  char data[ 2 ];
  char buffer[ 3 ] = {0x00,0x1E,'A'};
  static int letterWidth = -1;

  if( letterWidth == -1 )
  {
    picassoSetTextSize( 1 );
    if(( ret = serialPutCmd( buffer, sizeof(buffer),2)) == 0 )
    {
      serialGetData( data, 2 );
      letterWidth = data[ 1 ];
    }
  }
  return letterWidth;
}


int picassoGetLetterHeight( )
{
  int ret;
  char data[ 2 ];
  char buffer[ 3 ] = {0x00,0x1D,'A'};
  static int letterHeight = -1;

  if( letterHeight == -1 )
  {
    picassoSetTextSize( 1 );
    if(( ret = serialPutCmd( buffer, sizeof(buffer),2)) == 0 )
    {
      serialGetData( data, 2 );
      letterHeight = data[ 1 ];
    }
  }
  return letterHeight;
}

int picassoClearScreen( )
{
  char buffer[ 2 ] = {0xFF,0xCD};
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoEnableTouch( )
{
  char buffer[ 4 ] = {0xFF,0x38,0x00,0x00};
  return serialPutCmd( buffer, sizeof(buffer),0);
}

int picassoSetContrast( int c )
{
  static int current = -1;
  char buffer[ 4 ] = {0xFF,0x9C,0x00,0x00};
  if( c < 0 || c > 9 ) return -2;
  if( c == current ) return 0;
  buffer[ 3 ] = c;
  current = c;
  return serialPutCmd( buffer, sizeof(buffer),2);
}

int picassoGetTouch( int* x, int* y )
{
  int mode = -1;
  char data[ 2 ];
  char buffer[ 4 ] = {0xFF,0x37,0x00,0x00};
  if( serialPutCmd( buffer, sizeof(buffer),2) == 0 )
  {
    serialGetData( data, 2 );
    mode = data[ 1 ];
    if( mode == 1 || mode == 3 )
    {
       buffer[ 3 ] = 0x01;
       if( serialPutCmd( buffer, sizeof(buffer),2) == 0 )
       {
          serialGetData( data, 2 );
          *x = MAKEWORD( data[0], data[1] );
       }
       buffer[ 3 ] = 0x02;
       if( serialPutCmd( buffer, sizeof(buffer),2) == 0 )
       {
          serialGetData( data, 2 );
          *y = MAKEWORD( data[0], data[1] );
       }   
    }
  }
  return mode;
}

int picassoCheckDisplayModel( )
{
  int ret = -1;
  int length;
  char data[ 100 ];
  char buffer[ 2 ] = {0x00,0x1A};

  if(( ret = serialPutCmd( buffer, sizeof(buffer),2)) == 0 )
  {
    serialGetData( data, 2 );
    length = data[ 1 ];

    usleep( 50000 );

    serialGetData( data, length + 2 );

    data[ length + 2 ] = 0;

    printf( "LCD Model:'%s' (%d)\n", data+2, length );

    if( strstr( data+2, "uLCD-43PT" )) return 0;
    else printf( "ERROR: Invalid LCD model string.\n");
  }
  return ret;
}

int picassoSetBaudRate( int baud )
{
  char buffer[ 4 ] = {0x00,0x26,0x00,0x00};
  switch( baud )
  {
  case 9600 : buffer[ 3 ] = 6; break;
  case 19200 : buffer[ 3 ] = 8; break;
  case 57600 : buffer[ 3 ] = 12; break;
  case 115200 : buffer[ 3 ] = 13; break;
  default : return -1;
  }
  
  serialPutCmd( buffer, sizeof(buffer),-1);

  if( serialConfigure( baud ) < 0 )
  {
    printf( "serialConfigure failed\n" );
    return -1;
  }

  // The set baudrate command is acked at the new speed 100ms later
  return serialPutCmd( NULL, 0, 0 );
}

int baudRateTable[] = { 115200, 9600, 19200, 57600 };

void picassoWarmUp( )
{
  int i;
  for( i=0; i<10; i++ )
  {
    serialOpen( "/dev/ttyAMA0" );
    serialConfigure( baudRateTable[ i % 4 ] );
    usleep( 100000 );
  }
}

int picassoInit( )
{
  int retry = 0;
  int baudIndex = 0;
  int ret;

  printf( "[Picasso LCD Init]\n" );

  if( serialOpen( "/dev/ttyAMA0" ) < 0 ) return -1 ;
 
  if( serialConfigure( baudRateTable[ baudIndex++ ] ) < 0 ) printf( "SerialConfigure failed.\n" );

  serialStartListener( );

  do
  {
    char dummy[] = { 0xAA };
    if( retry++ >= 2 )
    {
       if( baudIndex >= sizeof( baudRateTable ) / sizeof( int ))
       {
         printf( "Tried everything to communicate.\n" );
         return -1;
       }

       printf( "WARNING: Failed %d times at %d, trying at %d baud.\n", 
         retry, 
         baudRateTable[ baudIndex-1 ], 
         baudRateTable[ baudIndex ] );

       serialConfigure( baudRateTable[ baudIndex ] );
       retry = 0;
       baudIndex++;
    }

    printf( "Probing with dummy command...\n" );
    ret = serialPutCmd( dummy, sizeof( dummy ), 0 );

  } while( ret == -2 ); // While we get timeout, keep on trying different baud rates

  printf( "Got answer. Switching to 115200 baud.\n" );

  // 100ms
  usleep( 100000 );

  ret = picassoSetBaudRate( 115200 );
  
  if( ret == 0 )
  { 
    ret = picassoCheckDisplayModel( );
  }

  return ret;
}

void picassoDeinit( )
{
  serialClose( );
}


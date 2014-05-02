

#define RGB(r,g,b)      (unsigned short)((( r & 0x1F ) << 11 ) | (( g & 0x3F ) << 5 ) | ( b & 0x1F ))

#define GET_RED( c )    (( c >> 11 ) & 0x1F )
#define GET_GREEN( c )  (( c >> 5 ) & 0x3F )
#define GET_BLUE( c )   ( c & 0x1F )

#define BLACK           RGB( 0,0,0 )
#define GRAY            RGB( 10, 20, 10 )
#define WHITE           RGB( 31, 63, 31 )
#define RED             RGB( 31, 0, 0 )
#define GREEN           RGB( 0, 63, 0 )
#define BLUE            RGB( 0, 0, 31 )
#define YELLOW		RGB( 31, 63, 0 )

#define TRANSPARENT_KEY RGB( 30, 60, 3 )

#define SCREEN_WIDTH	480
#define SCREEN_HEIGHT   272

// Init
void picassoWarmUp( );
int picassoInit( );
int picassoSetBaudRate( int baud );
void picassoDeinit( );

// General
int picassoClearScreen( );
int picassoSetContrast( int c );
int picassoSetScreenOrientation( int o );
int picassoEnableClipping( );
int picassoSetClipping( int i );
char* getPicassoHealth( );
void resetPicassoHealth( );

// Drawing
int picassoSetColor( int c );
int picassoDrawLine( int x1, int y1, int x2, int y2 );
int picassoDrawCircle( int x, int y, int r, int fill );
int picassoDrawRect( int x1, int y1, int x2, int y2, int fill );
int picassoDrawTriangle( int x1, int y1, int x2, int y2, int x3, int y3, int fill );

// Text
int picassoSetTextBackground( unsigned short b );
int picassoSetTextColor( unsigned short c );
int picassoSetTextSize( int s );
int picassoPutText( int x, int y, char* txt );
int picassoSetFont( int f );
int picassoGetLetterWidth( );
int picassoGetLetterHeight( );

// Touch
int picassoEnableTouch( );
int picassoGetTouch( int* x, int* y );





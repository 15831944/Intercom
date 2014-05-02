
int serialOpen( char *device );
int serialConfigure( int baud );
void serialStartListener( );
void serialFlush( );
void serialClose( );

int serialGetchar( void );
void serialPutchar( int data );
int serialPutCmd( char* buffer, int size, int answer );
void serialGetData( char *buffer, int size );

#define PICASSO_ACK	0x06
#define PICASSO_NAK	0x15

typedef struct _picasso_diag_t {
  unsigned int txByteCnt;
  unsigned int txCmdCnt;
  unsigned int txErr;
  unsigned int NakErrCnt;
  unsigned int TOutErrCnt;
  unsigned int MissErrCnt;
  unsigned int ExtraErrCnt;

  unsigned int cmdCnt;
  unsigned int errCnt;

} picasso_diag_t;


#include <time.h>

#define MSGBUFSIZE     256
#define IPSTRSIZE      80
#define MAX_OTHERS     30

typedef void(*stateChangeCallback)(int);

int initSocketCom( stateChangeCallback );
void signalNewState( int newState );
char* getMyIP( );
char* getNetworkHealth( );
void resetNetworkHealth( );

typedef struct _station_diag {
  char IP[IPSTRSIZE];

  char lastRxData[MSGBUFSIZE];
  int lastRxSize;
  struct timespec lastRxTime;
  int repeatCnt;

  unsigned int rxCnt;

  int repeatRxMissingErrCnt;
  int repeatRxExtraErrCnt;
  int unxpctdRxErrCnt;
  int mlfrmdRxErrCnt;
} station_diag_t;


typedef struct _socket_diag {
  unsigned int txCnt;
  int txErrCnt;
  int otherFullErr;

  unsigned int frmCnt;
  unsigned int errCnt;
  
  station_diag_t other[MAX_OTHERS];

} socket_diag_t;

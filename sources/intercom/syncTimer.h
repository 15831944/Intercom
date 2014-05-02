
#define ONE_SEC	     1000000000
#define ONE_MS       1000000

typedef struct _syncTimer_t
{
  int active;
  int period;
  int singleShot;
  struct timespec elapsed;
  void(*callback)(void*);
  void* arg;
  int remaining;
  int count;
} syncTimer_t;

void addMsToTime( struct timespec *ts, int msec );
void syncTimerProcess( );
void syncTimerInit( );
int syncTimerSetTimer( int period, void(*callback)(void*), void* arg, int singleShot );
int getTimerRemainingTime( int id );

void syncTimerKillTimer( int id );
long deltaInMs( struct timespec a, struct timespec b );

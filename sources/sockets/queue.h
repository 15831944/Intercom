
#define MSGQUEUESIZE     8

typedef struct _msg_t
{
  int retries;
  char msg [MSGBUFSIZE];
} msg_t;


int pushMsg( char* msg, int retries );
int getMsg( char* msg, int* retries );

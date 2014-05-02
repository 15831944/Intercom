
typedef void(*actionCallback)(void*);

typedef struct _button_t {
  int x1,y1,x2,y2;
  unsigned short color;
  int lit;
  int pressed;
  int blink;
  int idx;
  int timer;
  char txt[10];
  actionCallback action;
  struct button_t *previous;
} button_t;


void sinkTheButton( button_t* pBut );
void blinkButtonTimer( void* arg );
button_t* findButton( int x, int y );
button_t* getButton( int idx );

void destroyAllButtons( );
int buttonCreate( int x1, int y1, int x2, int y2, int c, char* txt, actionCallback callback );
void drawButtonText( button_t *pBut );
void drawButton( button_t *pBut );

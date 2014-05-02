

typedef enum {
  soundNoSound = 0,
  soundAlarm = 1,
  soundNotify = 2,

} sound_t;

void playSound( sound_t sound );
void initAudio( );



#define VERSION_STR "v1.3.4"

typedef enum {
  stateNoChange = 0,
  stateMatrix = 1,
  stateClock = 2,
  stateTimer = 3,
  stateStopWatch = 4,

} state_t;

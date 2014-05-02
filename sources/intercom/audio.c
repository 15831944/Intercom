
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "audio.h"

pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audioNotify = PTHREAD_COND_INITIALIZER;

sound_t soundId = soundNoSound;

void playSound( sound_t sound )
{
  soundId = sound;
  if( pthread_cond_signal(&audioNotify) < 0 ) 
  {
    fprintf (stderr, "playSound - pthread_cond_signal( ) failed: %s\n", strerror(errno));
  }
}

void* audioPlayback( void* param )
{
  while( 1 )
  {
    pthread_mutex_lock(&audioMutex);
    pthread_cond_wait( &audioNotify, &audioMutex );
    pthread_mutex_unlock(&audioMutex);
    while( soundId != soundNoSound )
    {
      sound_t nowPlaying = soundId;
      soundId = soundNoSound;
      switch( nowPlaying )
      {
      case soundNoSound :
        // Compiler happy
        break;
      case soundAlarm :
        if( system("aplay -q /root/alarm.wav") < 0 ) {
          fprintf (stderr, "audioPlayback - system(aplay'alarm') failed: %s\n", strerror (errno));
        }
        break;
      case soundNotify :
        if( system( "aplay -q /root/notify.wav") < 0 ) {
          fprintf (stderr, "audioPlayback - system(aplay'notify') failed: %s\n", strerror (errno));
        }
        break;
      }
    }
  }
}

void initAudio( )
{
  pthread_t audioThread;

  if( system( "amixer cset numid=3 1" ) < 0 ) {
    fprintf (stderr, "initAudio - system(amixer cset numid=3 1) failed: %s\n", strerror (errno));
  }
  //if( system( "amixer cset numid=3 300" ) < 0 ) {
  if( system( "amixer set PCM 90%%" ) < 0 ) { 
    fprintf (stderr, "initAudio - system(amixer set PCM 80%%) failed: %s\n", strerror (errno));
  }
  
  soundId = 0;
  if( pthread_create (&audioThread, NULL, audioPlayback, NULL) < 0 ) {
    fprintf (stderr, "initAudio - pthread_create failed: %s\n", strerror (errno));
  }
}

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <sid.h> /* The "reSID" header! */

#include "resid.h"

#define RESID_CLOCK_FREQUENCY 985248 /* PAL C64 speed. */

#define RESID_SAMPLE_RATE 44100
#define RESID_SDL_CALLBACK_BUFFER_SIZE 2048
#define RESID_BUFFER_SIZE 4096

#define RESID_LAG_UPPER ((RESID_BUFFER_SIZE / 2) + 200)
#define RESID_LAG_LOWER ((RESID_BUFFER_SIZE / 2) - 200)



static SID resid_instance;

static int16_t resid_buffer[RESID_BUFFER_SIZE];
static int resid_buffer_index_in = 0;
static int resid_buffer_index_out = 0;

static uint32_t resid_buffer_reads = 0;
static uint32_t resid_buffer_writes = 0;

/* Need to add 400 to speed up the emulator at startup
   to allow reSID to catch up to SDL before a program is loaded. */
static int32_t resid_sync_internal = (RESID_CLOCK_FREQUENCY / 100) + 400;



uint8_t resid_read_hook(void *dummy, uint16_t address)
{
  (void)dummy;
  return resid_instance.read(address - 0xD400);
}



void resid_write_hook(void *dummy, uint16_t address, uint8_t value)
{
  (void)dummy;
  resid_instance.write(address - 0xD400, value);
}



int32_t resid_sync(void)
{
  return resid_sync_internal;
}



static inline void resid_sync_control(void)
{
  /* Try to keep a constant lag between the reads and writes.
     This is needed to avoid choppy audio. */
  int32_t lag = resid_buffer_writes - resid_buffer_reads;
  if (lag > RESID_LAG_UPPER) {
    /* reSID writes faster than SDL can read, slow down emulation. */
    resid_sync_internal--;
  } else if (lag < RESID_LAG_LOWER) {
    /* SDL reads faster than reSID can write, speed up emulation. */
    resid_sync_internal++;
  } else {
    /* Around the preferred lag, use optimal speed. */
    resid_sync_internal = RESID_CLOCK_FREQUENCY / 100;
  }
}



static void resid_audio_callback(void *userdata, Uint8 *stream, int len)
{
  (void)userdata;
  int copied = 0;
  int to_copy;

  while (copied < (len / 2)) {
    to_copy = resid_buffer_index_in - resid_buffer_index_out;
    if (to_copy <= 0) {
      to_copy = RESID_BUFFER_SIZE - resid_buffer_index_out; /* Wrap */
    }

    if ((to_copy + copied) > (len / 2)) {
      to_copy = len / 2 - copied;
    }

    memcpy(stream + (copied * 2),
      &resid_buffer[resid_buffer_index_out], to_copy * 2);

    copied += to_copy;
    resid_buffer_index_out += to_copy;

    if (resid_buffer_index_out >= RESID_BUFFER_SIZE) {
      resid_buffer_index_out = 0;
    }
  }

  resid_buffer_reads += (len / 2);
  resid_sync_control();
}



static int resid_audio_init(void)
{
  SDL_AudioSpec desired, obtained;

  desired.freq     = RESID_SAMPLE_RATE;
  desired.format   = AUDIO_S16SYS;
  desired.channels = 1;
  desired.samples  = RESID_SDL_CALLBACK_BUFFER_SIZE;
  desired.userdata = 0;
  desired.callback = resid_audio_callback;

  if (SDL_OpenAudio(&desired, &obtained) != 0) {
    fprintf(stderr, "SDL_OpenAudio() failed: %s\n", SDL_GetError());
    return -1;
  }

  if (obtained.format != AUDIO_S16) {
    fprintf(stderr, "Did not get unsigned 8-bit audio format!\n");
    SDL_CloseAudio();
    return -1;
  }

  SDL_PauseAudio(0);
  return 0;
}



void resid_pause(void)
{
  SDL_PauseAudio(1);
}



void resid_resume(void)
{
  SDL_PauseAudio(0);
}



static void resid_exit(void)
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}



int resid_init(void)
{
  resid_instance.set_chip_model(MOS6581);
  if (resid_instance.set_sampling_parameters(RESID_CLOCK_FREQUENCY,
    SAMPLE_RESAMPLE_FAST, RESID_SAMPLE_RATE) == false) {
    fprintf(stderr, "reSID sampling parameters failed!\n");
    return -1;
  }

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
    fprintf(stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
    return -1;
  }
  atexit(resid_exit);

  return resid_audio_init();
}



void resid_execute(uint32_t cycles, bool warp_mode_active)
{
  cycle_count delta_t = cycles;

  int samples = resid_instance.clock(delta_t,
    &resid_buffer[resid_buffer_index_in],
    RESID_BUFFER_SIZE - resid_buffer_index_in);

  resid_buffer_index_in += samples;
  if (resid_buffer_index_in >= RESID_BUFFER_SIZE) {
    resid_buffer_index_in = 0;
  }
  resid_buffer_writes += samples;

  if (warp_mode_active) {
    /* Reset counters since lag becomes way too high in warp mode. */
    resid_buffer_writes %= RESID_BUFFER_SIZE;
    resid_buffer_reads  %= RESID_BUFFER_SIZE;
  }
}




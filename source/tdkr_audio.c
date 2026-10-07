/* tdkr_audio.c -- android.media.AudioTrack on audout.
 *
 * The engine (Gameloft's "vox" mixer) mixes its sound itself and streams it
 * to Java's AudioTrack: new AudioTrack(stream, rate, channels, format, size,
 * mode), play(), then write(byte[], off, size) in a loop, which blocks while
 * the track's buffer is full -- that is what paces the engine's audio thread.
 * Here each track has a ring buffer; one pump thread (rt_audout_pump_start)
 * mixes the playing tracks into audout's 48 kHz stereo buffers, resampling
 * (nearest frame) from the track's rate. 16-bit PCM only, mono or stereo. MIT.
 */
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "rt_audout.h"
#include "tdkr.h"
#include "util.h"

#define MAX_TRACKS 4
#define RING_BYTES (64 * 1024) /* power of two */

typedef struct Track {
  int used;
  int rate, chans, playing, cur_ok;
  uint32_t step, frac;      /* 16.16 source frames per output frame */
  int16_t cur[2];
  uint32_t rd, wr;          /* byte counters; RING_BYTES is a power of two */
  uint8_t ring[RING_BYTES];
} Track;

static Mutex g_lock;
static CondVar g_space;
static Track *g_tracks[MAX_TRACKS];
static Track g_pool[MAX_TRACKS]; /* never freed: a writer may still hold a released track */
static int g_pump, g_paused;

static int pop_frame(Track *t) {
  uint32_t fb = (uint32_t)t->chans * 2;
  if (t->wr - t->rd < fb)
    return 0;
  int16_t v[2];
  for (int c = 0; c < t->chans; c++) {
    uint8_t lo = t->ring[t->rd++ & (RING_BYTES - 1)];
    uint8_t hi = t->ring[t->rd++ & (RING_BYTES - 1)];
    v[c] = (int16_t)(lo | (hi << 8));
  }
  t->cur[0] = v[0];
  t->cur[1] = t->chans > 1 ? v[1] : v[0];
  return 1;
}

static void fill(int16_t *out, int frames, void *ud) {
  (void)ud;
  memset(out, 0, (size_t)frames * 4);
  if (g_paused)
    return;
  mutexLock(&g_lock);
  for (int i = 0; i < MAX_TRACKS; i++) {
    Track *t = g_tracks[i];
    if (!t || !t->playing)
      continue;
    for (int f = 0; f < frames; f++) {
      if (!t->cur_ok && !(t->cur_ok = pop_frame(t)))
        break; /* underrun: silence until the engine writes more */
      int l = out[2 * f] + t->cur[0], r = out[2 * f + 1] + t->cur[1];
      out[2 * f] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
      out[2 * f + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
      t->frac += t->step;
      while (t->frac >= 65536) {
        t->frac -= 65536;
        t->cur_ok = pop_frame(t);
      }
    }
  }
  condvarWakeAll(&g_space);
  mutexUnlock(&g_lock);
}

void tdkr_audio_pause(int paused) { g_paused = paused; }

/* ----------------------------------------------------------------- Java */
jvalue tdkr_at_init(JObj *self, const jvalue *a, const JMethod *m) {
  int rate = a[1].i > 0 ? a[1].i : 44100, cfg = a[2].i;
  Track *t = NULL;
  mutexLock(&g_lock);
  for (int i = 0; i < MAX_TRACKS && !t; i++)
    if (!g_pool[i].used) {
      t = &g_pool[i];
      memset(t, 0, sizeof *t);
      t->used = 1;
    }
  mutexUnlock(&g_lock);
  if (!t) {
    debugPrintf("[audio] AudioTrack: no free track\n");
    self->p = NULL;
    return jv_none();
  }
  t->rate = rate;
  t->chans = (cfg == 2 || cfg == 4) ? 1 : 2; /* CHANNEL_(OUT_)MONO; the rest stereo */
  if (rt_audout_open() == 0 && !g_pump)
    g_pump = rt_audout_pump_start(fill, NULL, 0x2A, 2) == 0;
  t->step = (uint32_t)(((uint64_t)rate << 16) / rt_audout_rate());
  mutexLock(&g_lock);
  int placed = 0;
  for (int i = 0; i < MAX_TRACKS && !placed; i++)
    if (!g_tracks[i])
      g_tracks[i] = t, placed = 1;
  mutexUnlock(&g_lock);
  if (!placed) {
    t->used = 0;
    t = NULL;
  }
  self->p = t;
  debugPrintf("[audio] AudioTrack %d Hz, %d ch%s\n", rate, t ? t->chans : 0, placed ? "" : " (no free track)");
  return jv_none();
}

jvalue tdkr_at_min_buffer(JObj *self, const jvalue *a, const JMethod *m) {
  int ch = (a[1].i == 2 || a[1].i == 4) ? 1 : 2;
  return jv_i(a[0].i / 5 * ch * 2 & ~3); /* about 200 ms */
}

jvalue tdkr_at_play(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  if (t)
    t->playing = 1;
  return jv_none();
}

jvalue tdkr_at_pause(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  if (t)
    t->playing = 0;
  return jv_none();
}

jvalue tdkr_at_stop(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  if (t) {
    mutexLock(&g_lock);
    t->playing = 0;
    t->rd = t->wr = 0;
    t->cur_ok = 0;
    condvarWakeAll(&g_space);
    mutexUnlock(&g_lock);
  }
  return jv_none();
}

jvalue tdkr_at_release(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  if (!t)
    return jv_none();
  mutexLock(&g_lock);
  t->playing = 0;
  for (int i = 0; i < MAX_TRACKS; i++)
    if (g_tracks[i] == t)
      g_tracks[i] = NULL;
  self->p = NULL;
  condvarWakeAll(&g_space);
  mutexUnlock(&g_lock);
  svcSleepThread(30000000ll); /* a writer that was waiting leaves it first */
  t->used = 0;
  return jv_none();
}

jvalue tdkr_at_write(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  JObj *arr = a[0].l;
  int off = a[1].i, size = a[2].i;
  if (!t || !arr || arr->kind != JK_ARRAY || off < 0 || size <= 0 || off + size > arr->a.len)
    return jv_i(0);
  if (!g_pump) { /* no sound output: take the data, at about the speed it would play */
    svcSleepThread((int64_t)size * 1000000000ll / ((int64_t)t->rate * t->chans * 2));
    return jv_i(size);
  }
  const uint8_t *src = (const uint8_t *)arr->a.data + off;
  int done = 0;
  mutexLock(&g_lock);
  while (done < size) {
    uint32_t space = RING_BYTES - (t->wr - t->rd);
    if (!space) {
      if (!t->playing)
        break; /* a stopped (or released) track does not block */
      condvarWaitTimeout(&g_space, &g_lock, 20000000ull);
      continue;
    }
    uint32_t n = space < (uint32_t)(size - done) ? space : (uint32_t)(size - done);
    /* two memcpy (the ring's end, then its start) instead of a byte loop: the
     * A57's copy moves 16 bytes at a time */
    uint32_t pos = t->wr & (RING_BYTES - 1), first = RING_BYTES - pos;
    if (first > n)
      first = n;
    memcpy(t->ring + pos, src + done, first);
    memcpy(t->ring, src + done + first, n - first);
    t->wr += n;
    done += (int)n;
  }
  mutexUnlock(&g_lock);
  if (done < size)
    svcSleepThread(5000000ll);
  return jv_i(done);
}

jvalue tdkr_at_state(JObj *self, const jvalue *a, const JMethod *m) {
  Track *t = self->p;
  return jv_i(t && t->playing ? 3 : 1); /* PLAYSTATE_PLAYING / _STOPPED */
}

jvalue tdkr_at_native_rate(JObj *self, const jvalue *a, const JMethod *m) { return jv_i((jint)rt_audout_rate()); }


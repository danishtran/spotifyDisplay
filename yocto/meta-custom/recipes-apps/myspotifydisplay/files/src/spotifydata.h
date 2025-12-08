#ifndef SPOTIFYDATA_H
#define SPOTIFYDATA_H

#include <stdint.h>

typedef struct {
  char track[128];        /* track_title */
  char artist[128];       /* joined artist_list */
  char album_url[256];    /* album_cover URL */
  char repeat_state[16];  /* "off", "context", "track" */
  int  shuffle_state;     /* 0 = false, 1 = true */
  int  is_playing;        /* 0 = paused, 1 = playing */
} SpotifyInfo;

/* Returns 0 on success, <0 on error */
int fetch_spotify_state(const char *url, SpotifyInfo *info);

int get_album_art_rgb565(const char *url, uint16_t **out_pixels, int *out_w, int *out_h);

#endif

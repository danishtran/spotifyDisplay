#include "spotifydata.h"
#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Using stb_image.h to load image from: https://github.com/nothings/stb/blob/master/stb_image.h */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct string_buf {
    char *ptr;
    size_t len;
};

static void init_string(struct string_buf *s) {
  s->len = 0;
  s->ptr = (char *)malloc(1);
  if (s->ptr) s->ptr[0] = '\0';
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, void *userdata) {
  struct string_buf *s = (struct string_buf *)userdata;
  size_t total = size * nmemb;

  char *new_ptr = (char *)realloc(s->ptr, s->len + total + 1);
  if (!new_ptr) {
    return 0;
  }
  s->ptr = new_ptr;
  memcpy(s->ptr + s->len, ptr, total);
  s->len += total;
  s->ptr[s->len] = '\0';
  return total;
}


static int http_get_to_string(const char *url, struct string_buf *out) {
  CURL *curl = curl_easy_init();
  if (!curl) return -1;

  init_string(out);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    if (out->ptr) free(out->ptr);
    return -2;
  }

  curl_easy_cleanup(curl);
  return 0;
}

int fetch_spotify_state(const char *url, SpotifyInfo *info) {
  struct string_buf json;
  int rc = http_get_to_string(url, &json);
  if (rc != 0) {
    return rc;
  }

  json_error_t err;
  json_t *root = json_loads(json.ptr, 0, &err);
  free(json.ptr);

  if (!root) {
    fprintf(stderr, "JSON parse error: %s\n", err.text);
    return -3;
  }

  /* 
    Flask JSON:
    album_cover  (string)
    artist_list  (array of strings)
    is_playing   (bool)
    repeat_state (string: "off"/"context"/"track")
    shuffle_state(bool)
    track_title  (string)
  */

  const char *track = NULL;
  const char *album_url = NULL;
  const char *repeat = NULL;
  int is_playing = 0;
  int shuffle = 0;
  const char *artist = NULL;

  json_t *j_track      = json_object_get(root, "track_title");
  json_t *j_album      = json_object_get(root, "album_cover");
  json_t *j_repeat     = json_object_get(root, "repeat_state");
  json_t *j_shuffle    = json_object_get(root, "shuffle_state");
  json_t *j_is_playing = json_object_get(root, "is_playing");
  json_t *j_art_list   = json_object_get(root, "artist_list");

  if (json_is_string(j_track))
    track = json_string_value(j_track);
  if (json_is_string(j_album))
    album_url = json_string_value(j_album);
  if (json_is_string(j_repeat))
    repeat = json_string_value(j_repeat);
  if (json_is_boolean(j_is_playing))
    is_playing = json_boolean_value(j_is_playing);
  if (json_is_boolean(j_shuffle))
    shuffle = json_boolean_value(j_shuffle);

  if (json_is_array(j_art_list) && json_array_size(j_art_list) > 0) {
    json_t *first = json_array_get(j_art_list, 0);
    if (json_is_string(first))
      artist = json_string_value(first);
  }

  snprintf(info->track, sizeof(info->track),
            "%s", track ? track : "No Track");
  snprintf(info->artist, sizeof(info->artist),
            "%s", artist ? artist : "Unknown Artist");
  snprintf(info->album_url, sizeof(info->album_url),
            "%s", album_url ? album_url : "");
  snprintf(info->repeat_state, sizeof(info->repeat_state),
            "%s", repeat ? repeat : "off");
  info->is_playing    = is_playing;
  info->shuffle_state = shuffle;

  json_decref(root);
  return 0;
}

static char      cached_url[256]       = {0};
static uint16_t *cached_pixels         = NULL;
static int       cached_w              = 0;
static int       cached_h              = 0;

static void free_album_cache(void) {
  if (cached_pixels) {
    free(cached_pixels);
    cached_pixels = NULL;
  }
  cached_url[0] = '\0';
  cached_w = cached_h = 0;
}

int get_album_art_rgb565(const char *url, uint16_t **out_pixels, int *out_w, int *out_h) {
  if (!url || !*url) {
    return -1;
  }

  if (cached_pixels && strcmp(url, cached_url) == 0) {
    *out_pixels = cached_pixels;
    *out_w = cached_w;
    *out_h = cached_h;
    return 0;
  }

  free_album_cache();

  struct string_buf img_buf;
  int rc = http_get_to_string(url, &img_buf);
  if (rc != 0) {
      return rc;
  }

  int x, y, n;
  unsigned char *rgb = stbi_load_from_memory(
    (const unsigned char *)img_buf.ptr,
    (int)img_buf.len,
    &x, &y, &n, 3
  );
  free(img_buf.ptr);

  if (!rgb) {
    fprintf(stderr, "stbi_load_from_memory failed\n");
    return -2;
  }

  size_t pixels_count = (size_t)x * (size_t)y;
  uint16_t *rgb565 = (uint16_t *)malloc(pixels_count * sizeof(uint16_t));
  if (!rgb565) {
    stbi_image_free(rgb);
    return -3;
  }

  for (size_t i = 0; i < pixels_count; ++i) {
    unsigned char r = rgb[i * 3 + 0];
    unsigned char g = rgb[i * 3 + 1];
    unsigned char b = rgb[i * 3 + 2];

    uint16_t r5 = (r & 0xF8) >> 3;
    uint16_t g6 = (g & 0xFC) >> 2;
    uint16_t b5 = (b & 0xF8) >> 3;

    rgb565[i] = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
  }

  stbi_image_free(rgb);

  strncpy(cached_url, url, sizeof(cached_url) - 1);
  cached_url[sizeof(cached_url) - 1] = '\0';
  cached_pixels = rgb565;
  cached_w = x;
  cached_h = y;

  *out_pixels = cached_pixels;
  *out_w = cached_w;
  *out_h = cached_h;
  return 0;
}

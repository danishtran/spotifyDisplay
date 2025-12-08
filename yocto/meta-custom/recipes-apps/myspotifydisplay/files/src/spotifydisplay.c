#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>

#include "spotifydata.h"

#define FRAME_US 50000
#define SCROLL_PIXELS_PER_FRAME 8
#define VISIBLE_CHARS 14

/* 8×8 bitmap font */
static const unsigned char font8x8_basic[128][8] = {
#include "font8x8_basic.inl"
};

static void draw_pixel(uint16_t *fb, int x, int y, int stride, uint16_t color) {
  if (x < 0 || y < 0) return;
  fb[y * stride + x] = color;
}

static void draw_char(uint16_t *fb, int x, int y, int stride, uint16_t color, char c) {
  unsigned char uc = (unsigned char)c;
  if (uc > 127) return;

  for (int row = 0; row < 8; row++) {
    unsigned char bits = font8x8_basic[uc][row];
    for (int col = 0; col < 8; col++) {
      if (bits & (1 << col)) {
        draw_pixel(fb, x + col, y + row, stride, color);
      }
    }
  }
}

static void draw_text(uint16_t *fb, int x, int y, int stride, uint16_t color, const char *str) {
  while (*str) {
    draw_char(fb, x, y, stride, color, *str);
    x += 8;
    str++;
  }
}

static void draw_char_scaled_int(uint16_t *fb, int x, int y, int stride, uint16_t color, char c, int scale) {
  unsigned char uc = (unsigned char)c;
  if (uc > 127 || scale <= 0) return;

  for (int row = 0; row < 8; row++) {
    unsigned char bits = font8x8_basic[uc][row];
    for (int col = 0; col < 8; col++) {
      if (bits & (1 << col)) {
        int px = x + col * scale;
        int py = y + row * scale;
        for (int yy = 0; yy < scale; yy++) {
          for (int xx = 0; xx < scale; xx++) {
            draw_pixel(fb, px + xx, py + yy, stride, color);
          }
        }
      }
    }
  }
}

static void draw_text_scaled_int(uint16_t *fb, int x, int y, int stride, uint16_t color, const char *str, int scale) {
  while (*str) {
    draw_char_scaled_int(fb, x, y, stride, color, *str, scale);
    x += 8 * scale;
    str++;
  }
}

static void draw_char_scaled_int_clipped(uint16_t *fb, int x, int y, int stride, uint16_t color, char c, int scale, int clip_x, int clip_w) {
  unsigned char uc = (unsigned char)c;
  if (uc > 127 || scale <= 0) return;

  int clip_x2 = clip_x + clip_w;

  for (int row = 0; row < 8; row++) {
    unsigned char bits = font8x8_basic[uc][row];
    for (int col = 0; col < 8; col++) {
      if (bits & (1 << col)) {
        int px0 = x + col * scale;
        int py0 = y + row * scale;
        for (int yy = 0; yy < scale; yy++) {
          int py = py0 + yy;
          for (int xx = 0; xx < scale; xx++) {
            int px = px0 + xx;
            if (px >= clip_x && px < clip_x2) {
              draw_pixel(fb, px, py, stride, color);
            }
          }
        }
      }
    }
  }
}

static void draw_text_scaled_int_clipped(uint16_t *fb, int x, int y, int stride, uint16_t color, const char *str, int scale, int clip_x, int clip_w) {
  while (*str) {
    draw_char_scaled_int_clipped(fb, x, y, stride, color, *str, scale, clip_x, clip_w);
    x += 8 * scale;
    str++;
  }
}

static const int reps_3_2[8] = {1, 2, 1, 2, 1, 2, 1, 2};

static void draw_char_scaled_3_2(uint16_t *fb, int x0, int y0, int stride, uint16_t color, char c) {
  unsigned char uc = (unsigned char)c;
  if (uc > 127) return;

  int dy_out = 0;
  for (int row = 0; row < 8; row++) {
    unsigned char bits = font8x8_basic[uc][row];
    int reps_y = reps_3_2[row];
    int dx_out = 0;

    for (int col = 0; col < 8; col++) {
      int reps_x = reps_3_2[col];
      if (bits & (1 << col)) {
        for (int ry = 0; ry < reps_y; ry++) {
          for (int rx = 0; rx < reps_x; rx++) {
            draw_pixel(fb, x0 + dx_out + rx, y0 + dy_out + ry, stride, color);
          }
        }
      }
      dx_out += reps_x;
    }
    dy_out += reps_y;
  }
}

static void draw_text_scaled_3_2(uint16_t *fb, int x, int y, int stride, uint16_t color, const char *str) {
  while (*str) {
    draw_char_scaled_3_2(fb, x, y, stride, color, *str);
    x += 12;
    str++;
  }
}

static void fill_rect(uint16_t *fb, int x, int y, int w, int h, int stride, uint16_t color) {
  for (int yy = 0; yy < h; yy++) {
    int dst_y = y + yy;
    if (dst_y < 0) continue;
    for (int xx = 0; xx < w; xx++) {
      int dst_x = x + xx;
      if (dst_x < 0) continue;
      fb[dst_y * stride + dst_x] = color;
    }
  }
}

static void blit_rgb565_scaled(uint16_t *fb, int fb_w, int fb_h, int stride, const uint16_t *src, int sw, int sh, int dw, int dh, int dst_x, int dst_y) {
  for (int y = 0; y < dh; y++) {
    float v = (float)y / (float)(dh - 1 ? dh - 1 : 1);
    int sy = (int)(v * (float)(sh - 1));
    int ty = dst_y + y;
    if (ty < 0 || ty >= fb_h) continue;

    for (int x = 0; x < dw; x++) {
      float u = (float)x / (float)(dw - 1 ? dw - 1 : 1);
      int sx = (int)(u * (float)(sw - 1));
      int tx = dst_x + x;
      if (tx < 0 || tx >= fb_w) continue;

      uint16_t pixel = src[sy * sw + sx];
      fb[ty * stride + tx] = pixel;
    }
  }
}

static void draw_play_icon(uint16_t *fb, int cx, int cy, int size, int stride, uint16_t color) {
  int h = size;
  for (int dy = -h/2; dy <= h/2; dy++) {
    for (int dx = 0; dx <= h; dx++) {
      if (abs(dy) * 2 <= (h - dx)) {
        int x = cx + dx;
        int y = cy + dy;
        draw_pixel(fb, x, y, stride, color);
      }
    }
  }
}

static void draw_pause_icon(uint16_t *fb, int cx, int cy, int size, int stride, uint16_t color) {
  int bar_w = size / 5;
  int bar_h = size;
  int gap   = bar_w;

  fill_rect(fb, cx - gap/2 - bar_w, cy - bar_h/2, bar_w, bar_h, stride, color);
  fill_rect(fb, cx + gap/2,         cy - bar_h/2, bar_w, bar_h, stride, color);
}


static int title_scroll_px  = 0;
static int artist_scroll_px = 0;
static char last_track[128]  = "";
static char last_artist[128] = "";

static void draw_scrolling_line(uint16_t *fb, int x, int y, int stride, uint16_t color, const char *text, int scale, int *scroll_px) {
  int len = (int)strlen(text);
  int char_w = 8 * scale;
  int visible_px = VISIBLE_CHARS * char_w;

  if (len <= VISIBLE_CHARS) {
    draw_text_scaled_int_clipped(fb, x, y, stride, color, text, scale, x, visible_px);
    *scroll_px = 0;
    return;
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "%s   ", text);
  int slen = (int)strlen(buf);
  int total_px = slen * char_w;
  if (total_px <= 0) return;

  int offset = *scroll_px % total_px;
  int base   = -offset;

  for (int copy = 0; copy < 2; copy++) {
    int start_x = x + base + copy * total_px;
    draw_text_scaled_int_clipped(fb, start_x, y, stride, color, buf, scale, x, visible_px);
  }
}

int main(void) {
  const char *API_URL = "IP_of_Host_RPI/state";

  int fb_fd = open("/dev/fb0", O_RDWR);
  if (fb_fd < 0) {
    perror("open framebuffer");
    return 1;
  }

  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;

  memset(&vinfo, 0, sizeof(vinfo));
  memset(&finfo, 0, sizeof(finfo));

  for (int tries = 0; tries < 50; tries++) {
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
      perror("FBIOGET_VSCREENINFO");
      sleep(1);
      continue;
    }
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
      perror("FBIOGET_FSCREENINFO");
      sleep(1);
      continue;
    }

    if (vinfo.xres > 0 && vinfo.yres > 0 && finfo.line_length > 0 && finfo.smem_len > 0) {
      break;
    }

    sleep(1);
  }

  if (vinfo.xres == 0 || vinfo.yres == 0 || finfo.line_length == 0 || finfo.smem_len == 0) {
    fprintf(stderr, "framebuffer not ready, aborting\n");
    close(fb_fd);
    return 1;
  }

  int width  = vinfo.xres;
  int height = vinfo.yres;
  int stride_pixels = finfo.line_length / 2;
  long screensize = (long)finfo.line_length * vinfo.yres;

  uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  if (fbp == MAP_FAILED) {
    perror("mmap");
    close(fb_fd);
    return 1;
  }

  uint16_t *backbuf = (uint16_t *)malloc(width * height * sizeof(uint16_t));
  if (!backbuf) {
    fprintf(stderr, "Failed to allocate backbuffer\n");
    munmap(fbp, screensize);
    close(fb_fd);
    return 1;
  }

  SpotifyInfo info;
  memset(&info, 0, sizeof(info));

  const int max_art_size = 200;
  const int frames_per_state = 1000000 / FRAME_US;
  int frame_counter = 0;
  int have_state = 0;

  while (1) {
    if (frame_counter % frames_per_state == 0) {
      int rc = fetch_spotify_state(API_URL, &info);
      if (rc == 0) {
        have_state = 1;
        if (strcmp(info.track, last_track) != 0) {
          strncpy(last_track, info.track, sizeof(last_track) - 1);
          last_track[sizeof(last_track) - 1] = '\0';
          title_scroll_px = 0;
        }
        if (strcmp(info.artist, last_artist) != 0) {
          strncpy(last_artist, info.artist, sizeof(last_artist) - 1);
          last_artist[sizeof(last_artist) - 1] = '\0';
          artist_scroll_px = 0;
        }
      }
    }

    memset(backbuf, 0x00, width * height * sizeof(uint16_t));

    if (have_state) {
      uint16_t white = 0xFFFF;
      uint16_t artist_grey = 0xC618;
      uint16_t green = 0x07E0;
      uint16_t yellow = 0xFFE0;
      uint16_t gray = 0x39E7;

      /* Album art */
      uint16_t *art_pixels = NULL;
      int art_w = 0, art_h = 0;
      int draw_w = max_art_size;
      int draw_h = max_art_size;

      int art_x = 10;
      int art_y = 0;

      if (get_album_art_rgb565(info.album_url, &art_pixels, &art_w, &art_h) == 0 &&
          art_pixels && art_w > 0 && art_h > 0) {
        float scale = 1.0f;
        if (art_w > max_art_size || art_h > max_art_size) {
          float sx = (float)max_art_size / (float)art_w;
          float sy = (float)max_art_size / (float)art_h;
          scale = (sx < sy) ? sx : sy;
        }

        draw_w = (int)(art_w * scale);
        draw_h = (int)(art_h * scale);

        art_y = (height - draw_h) / 2;

        blit_rgb565_scaled(backbuf, width, height, width, art_pixels, art_w, art_h, draw_w, draw_h, art_x, art_y);
      } else {
        draw_w = max_art_size;
        draw_h = max_art_size;
        art_y = (height - draw_h) / 2;
        fill_rect(backbuf, art_x, art_y, draw_w, draw_h, width, gray);
      }

      int art_top = art_y;
      int art_bottom = art_y + draw_h;

      int text_x = art_x + max_art_size + 20;

      /* Title & Artist */
      int title_scale = 2;
      int title_h = 8 * title_scale;
      int padding_title_artist = 4;
      int padding_artist_play  = 30;

      int title_y  = art_top;
      int artist_y = title_y + title_h + padding_title_artist;

      draw_scrolling_line(backbuf, text_x, title_y,  width, white, info.track,  title_scale, &title_scroll_px);
      draw_scrolling_line(backbuf, text_x, artist_y, width, artist_grey, info.artist, title_scale, &artist_scroll_px);

      /* Status */
      int status_char_h = 12;              /* from 3:2 scaling */
      int shuffle_y = art_bottom - status_char_h;
      int repeat_y = shuffle_y - status_char_h;

      /* PLAYING line */
      const char *play_text = info.is_playing ? "PLAYING" : "PAUSED";
      int play_y = artist_y + title_h + padding_artist_play;
      draw_text_scaled_3_2(backbuf, text_x, play_y, width, white, play_text);

      int play_text_len   = (int)strlen(play_text);
      int play_text_width = play_text_len * 12;
      int icon_size       = 28;

      /* icon */
      int bottom_play = play_y + status_char_h;
      int top_repeat = repeat_y;
      int mid_y = (bottom_play + top_repeat) / 2;
      int icon_center_y = mid_y;
      int icon_x = text_x + play_text_width / 2;

      if (info.is_playing) {
        draw_play_icon(backbuf, icon_x, icon_center_y, icon_size, width, green);
      } else {
        draw_pause_icon(backbuf, icon_x, icon_center_y, icon_size, width, yellow);
      }

      char buf[64];

      snprintf(buf, sizeof(buf), "Repeat: %s", info.repeat_state);
      draw_text_scaled_3_2(backbuf, text_x, repeat_y, width, white, buf);

      snprintf(buf, sizeof(buf), "Shuffle: %s", info.shuffle_state ? "ON" : "OFF");
      draw_text_scaled_3_2(backbuf, text_x, shuffle_y, width, white, buf);
    } else {
      draw_text(backbuf, 10, height/2, width, 0xFFFF, "Waiting for data...");
    }

    for (int y = 0; y < height; y++) {
      memcpy(&fbp[y * stride_pixels], &backbuf[y * width], width * sizeof(uint16_t));
    }

    title_scroll_px += SCROLL_PIXELS_PER_FRAME;
    artist_scroll_px += SCROLL_PIXELS_PER_FRAME;
    frame_counter++;

    usleep(FRAME_US);
  }

  free(backbuf);
  munmap(fbp, screensize);
  close(fb_fd);
  return 0;
}

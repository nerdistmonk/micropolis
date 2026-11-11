/* sdl3_screenshot.c: Saves the presented window to a timestamped PNG in the working directory.
 * The PNG uses stored (uncompressed) deflate blocks, so no compression library is needed.
 */

#include "sdl3_screenshot.h"
#include "sdl3_gui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PNG_MAX_STORED_BLOCK 65535u

static bool capture_requested = false;
static uint32_t crc_table[256];
static bool crc_table_ready = false;

static void InitCrcTable(void)
{
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
  crc_table_ready = true;
}

static uint32_t CrcUpdate(uint32_t crc, const uint8_t *data, size_t len)
{
  for (size_t i = 0; i < len; i++) crc = crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return crc;
}

static uint32_t Adler32(const uint8_t *data, size_t len)
{
  uint32_t a = 1;
  uint32_t b = 0;
  for (size_t i = 0; i < len; i++) {
    a = (a + data[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

static void PutBE32(uint8_t *out, uint32_t v)
{
  out[0] = (uint8_t)(v >> 24);
  out[1] = (uint8_t)(v >> 16);
  out[2] = (uint8_t)(v >> 8);
  out[3] = (uint8_t)v;
}

static bool PutBytes(FILE *f, uint32_t *crc, const uint8_t *data, size_t len)
{
  if (len == 0) return true;
  *crc = CrcUpdate(*crc, data, len);
  return fwrite(data, 1, len, f) == len;
}

static bool BeginChunk(FILE *f, const char type[4], uint32_t len, uint32_t *crc)
{
  uint8_t head[8];
  PutBE32(head, len);
  memcpy(head + 4, type, 4);
  if (fwrite(head, 1, sizeof head, f) != sizeof head) return false;
  *crc = CrcUpdate(0xFFFFFFFFu, head + 4, 4);
  return true;
}

static bool EndChunk(FILE *f, uint32_t crc)
{
  uint8_t tail[4];
  PutBE32(tail, crc ^ 0xFFFFFFFFu);
  return fwrite(tail, 1, sizeof tail, f) == sizeof tail;
}

/* IDAT payload: zlib header, stored deflate blocks over the raw scanlines, then the Adler-32 trailer. */
static bool WriteImageData(FILE *f, const uint8_t *raw, size_t raw_len)
{
  size_t blocks = (raw_len + PNG_MAX_STORED_BLOCK - 1) / PNG_MAX_STORED_BLOCK;
  size_t payload = 2 + raw_len + 5 * blocks + 4;
  if (payload > 0x7FFFFFFFu) return false;

  uint32_t crc = 0;
  if (!BeginChunk(f, "IDAT", (uint32_t)payload, &crc)) return false;

  const uint8_t zlib_header[2] = { 0x78, 0x01 };
  if (!PutBytes(f, &crc, zlib_header, sizeof zlib_header)) return false;

  size_t offset = 0;
  do {
    size_t len = raw_len - offset;
    if (len > PNG_MAX_STORED_BLOCK) len = PNG_MAX_STORED_BLOCK;
    uint16_t len16 = (uint16_t)len;
    uint16_t nlen16 = (uint16_t)~len16;

    uint8_t block_head[5];
    block_head[0] = (offset + len == raw_len) ? 1 : 0;
    block_head[1] = (uint8_t)(len16 & 0xFFu);
    block_head[2] = (uint8_t)(len16 >> 8);
    block_head[3] = (uint8_t)(nlen16 & 0xFFu);
    block_head[4] = (uint8_t)(nlen16 >> 8);
    if (!PutBytes(f, &crc, block_head, sizeof block_head)) return false;
    if (!PutBytes(f, &crc, raw + offset, len)) return false;
    offset += len;
  } while (offset < raw_len);

  uint8_t adler[4];
  PutBE32(adler, Adler32(raw, raw_len));
  if (!PutBytes(f, &crc, adler, sizeof adler)) return false;
  return EndChunk(f, crc);
}

static bool SavePng(const SDL_Surface *rgb, const char *path)
{
  int w = rgb->w;
  int h = rgb->h;
  if (w <= 0 || h <= 0) return false;
  if (!crc_table_ready) InitCrcTable();

  size_t row_bytes = (size_t)w * 3u;
  size_t stride = 1u + row_bytes;
  size_t raw_len = stride * (size_t)h;
  uint8_t *raw = malloc(raw_len);
  if (!raw) return false;
  for (int y = 0; y < h; y++) {
    uint8_t *dst = raw + (size_t)y * stride;
    dst[0] = 0;
    memcpy(dst + 1, (const uint8_t *)rgb->pixels + (size_t)y * (size_t)rgb->pitch, row_bytes);
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    free(raw);
    return false;
  }

  static const uint8_t signature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
  uint8_t ihdr[13];
  PutBE32(ihdr, (uint32_t)w);
  PutBE32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8;
  ihdr[9] = 2;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;

  uint32_t crc = 0;
  bool ok = fwrite(signature, 1, sizeof signature, f) == sizeof signature;
  ok = ok && BeginChunk(f, "IHDR", sizeof ihdr, &crc) && PutBytes(f, &crc, ihdr, sizeof ihdr) && EndChunk(f, crc);
  ok = ok && WriteImageData(f, raw, raw_len);
  ok = ok && BeginChunk(f, "IEND", 0, &crc) && EndChunk(f, crc);

  free(raw);
  if (fclose(f) != 0) ok = false;
  if (!ok) remove(path);
  return ok;
}

void Screenshot_Request(void)
{
  capture_requested = true;
}

/* Called between drawing the window and presenting it, while the frame is still readable. */
void Screenshot_CaptureIfRequested(SDL_Renderer *renderer)
{
  if (!capture_requested) return;
  capture_requested = false;

  SDL_Time now = 0;
  SDL_DateTime dt;
  memset(&dt, 0, sizeof dt);
  SDL_GetCurrentTime(&now);
  SDL_TimeToDateTime(now, &dt, true);

  char path[64];
  snprintf(path, sizeof path, "micropolis-%04d%02d%02d-%02d%02d%02d-%03d.png",
           dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.nanosecond / 1000000);

  bool ok = false;
  SDL_Surface *shot = SDL_RenderReadPixels(renderer, NULL);
  if (shot) {
    SDL_Surface *rgb = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(shot);
    if (rgb) {
      ok = SavePng(rgb, path);
      SDL_DestroySurface(rgb);
    }
  }

  if (ok) {
    char msg[96];
    snprintf(msg, sizeof msg, "Screenshot saved to %s", path);
    GUI_ShowStatus(msg);
  } else {
    GUI_ShowStatus("Screenshot failed");
  }
}

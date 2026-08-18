#ifndef DEBUG_H
#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
  void dbg_printf(const char *fmt, ...);
  #define DBG_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
  #define DBG_UDP(fmt, ...) dbg_printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_LOG(fmt, ...)
  #define DBG_UDP(fmt, ...)
#endif

#ifdef DEBUG_TEXTURE
  #define DBG_TEX(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_TEX(fmt, ...)
#endif

#ifdef DEBUG_GFX
  #define DBG_GFX(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_GFX(fmt, ...)
#endif

#ifdef DEBUG_AUDIO
  #define DBG_AUD(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_AUD(fmt, ...)
#endif

#ifdef DEBUG_INPUT
  #define DBG_INP(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_INP(fmt, ...)
#endif

#ifdef DEBUG_SPU
  #define DBG_SPU(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_SPU(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif

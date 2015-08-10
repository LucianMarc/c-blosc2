#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#if defined(USING_CMAKE)
  #include "config.h"
#endif /*  USING_CMAKE */
#include "blosc.h"

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>

  /* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

#else
  #include <stdint.h>
  #include <unistd.h>
  #include <inttypes.h>
#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif

/*
 * TODO: return api should be integer based to indicate success or failure
 *
 */

int delta_encoder8(uint8_t* src,  // - source to be delta'd against
                   uint8_t* dest, // - destination to be diff'd, result will
                                  //   overwrite dest
                   size_t nbytes) // - size of src, src and nbytes should be
                                  //   the same
{
  size_t i;  // byte iterator
  for (i=0; i < nbytes; i++) {
    // Overflow?
    dest[i] = dest[i] - src[i];
  }
  return nbytes;
}

int delta_encoder32(uint8_t* src,  // - source to be delta'd against
                    uint8_t* dest, // - destination to be diff'd, result will
                                   //   overwrite dest
                    size_t nbytes) // - size of src, src and nbytes should be
                                   //   the same
{
  size_t i;
  uint32_t* ui32src = (uint32_t*)src;
  uint32_t* ui32dest = (uint32_t*)dest;

  /* Get the delta in chunks of 4 bytes (uint32_t) */
  for (i=0; i<(nbytes/4); i++) {
    ui32dest[i] = ui32dest[i] - ui32src[i];
  }

  return nbytes;
}

int delta_decoder8(uint8_t* src,  // source to be added
                   uint8_t* dest, // destination to be added too
                   size_t nbytes) // size of src, must be equal with size of dest
{
  size_t i;

  /* Add the delta */
  for (i=0; i<(nbytes/4); i++) {
    dest[i] += src[i];
  }
  return nbytes;
}

int delta_decoder32(uint8_t* src,  // source to be added
                    uint8_t* dest, // destination to be added too
                    size_t nbytes) // size of src, must be equal with size of dest
{
  size_t i;
  uint32_t* ui32dest = (uint32_t*)dest;
  uint32_t* ui32src = (uint32_t*)src;

  /* Add the delta */
  for (i=0; i<(nbytes/4); i++) {
    ui32dest[i] += ui32src[i];
  }
  return nbytes;
}

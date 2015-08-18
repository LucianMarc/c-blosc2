/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  This is delta compressor, slightly based on
  https://tools.ietf.org/html/rfc3284

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-08-17

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "blosclz.h"

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>

  /* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif
#else
  #include <stdint.h>
#endif  /* _WIN32 */


/*
 * Always check for bound when decompressing.
 * Generally it is best to leave it defined.
 */
#define BDELTA_SAFE


/*
 * Prevent accessing more than 8-bit at once, except on x86 architectures.
 */
#if !defined(BDELTA_STRICT_ALIGN)
#define BDELTA_STRICT_ALIGN
#if defined(__i386__) || defined(__386) || defined (__amd64)  /* GNU C, Sun Studio */
#undef BDELTA_STRICT_ALIGN
#elif defined(__i486__) || defined(__i586__) || defined(__i686__)  /* GNU C */
#undef BDELTA_STRICT_ALIGN
#elif defined(_M_IX86) || defined(_M_X64)   /* Intel, MSVC */
#undef BDELTA_STRICT_ALIGN
#elif defined(__386)
#undef BDELTA_STRICT_ALIGN
#elif defined(_X86_) /* MinGW */
#undef BDELTA_STRICT_ALIGN
#elif defined(__I86__) /* Digital Mars */
#undef BDELTA_STRICT_ALIGN
/* Seems like unaligned access in ARM (at least ARMv6) is pretty
   expensive, so we are going to always enforce strict aligment in ARM.
   If anybody suggest that newer ARMs are better, we can revisit this. */
/* #elif defined(__ARM_FEATURE_UNALIGNED) */  /* ARM, GNU C */
/* #undef BDELTA_STRICT_ALIGN */
#endif
#endif


#define MAX_COPY       32
#define MAX_DISTANCE 8192
#define MAX_FARDISTANCE (65535+MAX_DISTANCE-1)

#if !defined(BDELTA_STRICT_ALIGN)
#define BDELTA_READU16(p) *((const uint16_t*)(p)) 
#else
#define BDELTA_READU16(p) ((p)[0] | (p)[1]<<8)
#endif

#define HASH_LOG  12
#define HASH_SIZE (1<< HASH_LOG)
#define HASH_MASK  (HASH_SIZE-1)
#define HASH_FUNCTION(v,p) { v = BDELTA_READU16(p); v ^= BDELTA_READU16(p+1)^(v>>(16-HASH_LOG));v &= HASH_MASK; }


int bdelta_compress(const void* input, int length, void* output, int maxout,
		    const void* dref, int drefsize)
{
  const uint8_t* ip = (const uint8_t*) input;
  const uint8_t* ibase = (const uint8_t*) input;
  const uint8_t* ip_bound = ip + length - 2;
  const uint8_t* ip_limit = ip + length - 12;
  uint8_t* op = (uint8_t*) output;

  uint32_t htab[HASH_SIZE];
  uint32_t* hslot;
  uint32_t hval;

  uint32_t copy;

  /* printf("bdelta_compress: %d, %d\n", drefsize, maxout); */
  /* sanity check */
  if(length < 4)
  {
    if(length)
    {
      /* create literal copy only */
      *op++ = length-1;
      ip_bound++;
      while(ip <= ip_bound)
        *op++ = *ip++;
      return length+1;
    }
    else
      return 0;
  }

  /* initializes hash table */
  for (hslot = htab; hslot < htab + HASH_SIZE; hslot++)
    *hslot = 0;

  /* we start with literal copy */
  copy = 2;
  *op++ = MAX_COPY-1;
  *op++ = *ip++;
  *op++ = *ip++;

  /* main loop */
  while(ip < ip_limit)
  {
    const uint8_t* ref;
    uint32_t distance;

    /* minimum match length */
    uint32_t len = 3;

    /* comparison starting-point */
    const uint8_t* anchor = ip;

    /* check for a run */
    if(ip[0] == ip[-1] && BDELTA_READU16(ip-1)==BDELTA_READU16(ip+1))
    {
      distance = 1;
      ip += 3;
      ref = anchor - 1 + 3;
      goto match;
    }

    /* find potential match */
    HASH_FUNCTION(hval, ip);
    hslot = htab + hval;
    ref = ibase + *hslot;

    /* calculate distance to the match */
    distance = anchor - ref;

    /* update hash table */
    *hslot = (uint32_t)(anchor - ibase);

    /* is this a match? check the first 3 bytes */
    if(distance==0 || 
       (distance >= MAX_FARDISTANCE) ||
       *ref++ != *ip++ || *ref++!=*ip++ || *ref++!=*ip++)
      goto literal;

    /* far, needs at least 5-byte match */
    if(distance >= MAX_DISTANCE)
    {
      if(*ip++ != *ref++ || *ip++!= *ref++) 
        goto literal;
      len += 2;
    }
    
    match:

    /* last matched byte */
    ip = anchor + len;

    /* distance is biased */
    distance--;

    if(!distance)
    {
      /* zero distance means a run */
      uint8_t x = ip[-1];
      while(ip < ip_bound)
        if(*ref++ != x) break; else ip++;
    }
    else
    for(;;)
    {
      /* safe because the outer check against ip limit */
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      if(*ref++ != *ip++) break;
      while(ip < ip_bound)
        if(*ref++ != *ip++) break;
      break;
    }

    /* if we have copied something, adjust the copy count */
    if(copy)
      /* copy is biased, '0' means 1 byte copy */
      *(op-copy-1) = copy-1;
    else
      /* back, to overwrite the copy count */
      op--;

    /* reset literal counter */
    copy = 0;

    /* length is biased, '1' means a match of 3 bytes */
    ip -= 3;
    len = ip - anchor;

    /* encode the match */
    if(distance < MAX_DISTANCE)
    {
      if(len < 7)
      {
        *op++ = (len << 5) + (distance >> 8);
        *op++ = (distance & 255);
      }
      else
      {
        *op++ = (7 << 5) + (distance >> 8);
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = (distance & 255);
      }
    }
    else
    {
      /* far away, but not yet in the another galaxy... */
      if(len < 7)
      {
        distance -= MAX_DISTANCE;
        *op++ = (len << 5) + 31;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
      else
      {
        distance -= MAX_DISTANCE;
        *op++ = (7 << 5) + 31;
        for(len-=7; len >= 255; len-= 255)
          *op++ = 255;
        *op++ = len;
        *op++ = 255;
        *op++ = distance >> 8;
        *op++ = distance & 255;
      }
    }

    /* update the hash at match boundary */
    HASH_FUNCTION(hval,ip);
    htab[hval] = (uint32_t)(ip++ - ibase);
    HASH_FUNCTION(hval,ip);
    htab[hval] = (uint32_t)(ip++ - ibase);

    /* assuming literal copy */
    *op++ = MAX_COPY-1;

    continue;

    literal:
      *op++ = *anchor++;
      ip = anchor;
      copy++;
      if(copy == MAX_COPY)
      {
        copy = 0;
        *op++ = MAX_COPY-1;
      }
  }

  /* left-over as literal copy */
  ip_bound++;
  while(ip <= ip_bound)
  {
    *op++ = *ip++;
    copy++;
    if(copy == MAX_COPY)
    {
      copy = 0;
      *op++ = MAX_COPY-1;
    }
  }

  /* if we have copied something, adjust the copy length */
  if(copy)
    *(op-copy-1) = copy-1;
  else
    op--;

  /* marker for bdelta2 */
  *(uint8_t*)output |= (1 << 5);

  return op - (uint8_t*)output;
}

int bdelta_decompress(const void* input, int length, void* output, int maxout,
		      const void* dref, int drefsize)
{
  const uint8_t* ip = (const uint8_t*) input;
  const uint8_t* ip_limit  = ip + length;
  uint8_t* op = (uint8_t*) output;
  uint8_t* op_limit = op + maxout;
  uint32_t ctrl = (*ip++) & 31;
  int loop = 1;

  /* printf("bdelta_decompress: %d\n", drefsize); */
  do
  {
    const uint8_t* ref = op;
    uint32_t len = ctrl >> 5;
    uint32_t ofs = (ctrl & 31) << 8;

    if(ctrl >= 32)
    {
      uint8_t code;
      len--;
      ref -= ofs;
      if (len == 7-1)
        do
	{
          code = *ip++;
          len += code;
        } while (code==255);
      code = *ip++;
      ref -= code;

      /* match from 16-bit distance */
      if(code==255)
      if(ofs==(31 << 8))
      {
        ofs = (*ip++) << 8;
        ofs += *ip++;
        ref = op - ofs - MAX_DISTANCE;
      }
      
#ifdef BDELTA_SAFE
      if (op + len + 3 > op_limit)
        return 0;

      if (ref-1 < (uint8_t *)output)
        return 0;
#endif

      if(ip < ip_limit)
        ctrl = *ip++;
      else
        loop = 0;

      if(ref == op)
      {
        /* optimize copy for a run */
        uint8_t b = ref[-1];
        *op++ = b;
        *op++ = b;
        *op++ = b;
        for(; len; --len)
          *op++ = b;
      }
      else
      {
#if !defined(BDELTA_STRICT_ALIGN)
        const uint16_t* p;
        uint16_t* q;
#endif
        /* copy from reference */
        ref--;
        *op++ = *ref++;
        *op++ = *ref++;
        *op++ = *ref++;

#if !defined(BDELTA_STRICT_ALIGN)
        /* copy a byte, so that now it's word aligned */
        if(len & 1)
        {
          *op++ = *ref++;
          len--;
        }

        /* copy 16-bit at once */
        q = (uint16_t*) op;
        op += len;
        p = (const uint16_t*) ref;
        for(len>>=1; len > 4; len-=4)
        {
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
          *q++ = *p++;
        }
        for(; len; --len)
          *q++ = *p++;
#else
        for(; len; --len)
          *op++ = *ref++;
#endif
      }
    }
    else
    {
      ctrl++;
#ifdef BDELTA_SAFE
      if (op + ctrl > op_limit)
        return 0;
      if (ip + ctrl > ip_limit)
        return 0;
#endif

      *op++ = *ip++; 
      for(--ctrl; ctrl; ctrl--)
        *op++ = *ip++;

      loop = (ip < ip_limit);
      if(loop)
        ctrl = *ip++;
    }
  }
  while(loop);

  return op - (uint8_t*)output;
}

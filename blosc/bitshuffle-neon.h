/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  Note: Adapted for NEON by Lucian Marc.

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated bitshuffle/bitunshuffle routines. */

#ifndef BITSHUFFLE_NEON_H
#define BITSHUFFLE_NEON_H

#include "blosc-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- bshuf_default_block_size ----
 *
 * The default block size as function of element size.
 *
 * This is the block size used by the blocked routines (any routine
 * taking a *block_size* argument) when the block_size is not provided
 * (zero is passed).
 *
 * The results of this routine are guaranteed to be stable such that
 * shuffled/compressed data can always be decompressed.
 *
 * Parameters
 * ----------
 *  elem_size : element size of data to be shuffled/compressed.
 *
 */
BLOSC_NO_EXPORT size_t bshuf_default_block_size(const size_t elem_size);

/* ---- bitshuffle_neon ----
 *
 * Bitshuffle the data.
 *
 * Transpose the bits within elements, in blocks of *block_size*
 * elements.
 *
 * Parameters
 * ----------
 *  in : input buffer, must be of size * elem_size bytes
 *  out : output buffer, must be of size * elem_size bytes
 *  size : number of elements in input
 *  elem_size : element size of typed data
 *  block_size : Do transpose in blocks of this many elements. Pass 0 to
 *  select automatically (recommended).
 *
 * Returns
 * -------
 *  number of bytes processed, negative error-code if failed.
 *
 */
BLOSC_NO_EXPORT int64_t bitshuffle_neon(const void* in, void* out, const size_t size,
        const size_t elem_size, size_t block_size);


/* ---- bitunshuffle_neon ----
 *
 * Unshuffle bitshuffled data.
 *
 * Untranspose the bits within elements, in blocks of *block_size*
 * elements.
 *
 * To properly unshuffle bitshuffled data, *size*, *elem_size* and *block_size*
 * must match the parameters used to shuffle the data.
 *
 * Parameters
 * ----------
 *  in : input buffer, must be of size * elem_size bytes
 *  out : output buffer, must be of size * elem_size bytes
 *  size : number of elements in input
 *  elem_size : element size of typed data
 *  block_size : Do transpose in blocks of this many elements. Pass 0 to
 *  select automatically (recommended).
 *
 * Returns
 * -------
 *  number of bytes processed, negative error-code if failed.
 *
 */
BLOSC_NO_EXPORT int64_t bitunshuffle_neon(const void* in, void* out, const size_t size,
        const size_t elem_size, size_t block_size);


#ifdef __cplusplus
}
#endif

#endif /* BITSHUFFLE_NEON_H */

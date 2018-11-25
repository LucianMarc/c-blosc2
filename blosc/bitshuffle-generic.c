/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "bitshuffle-generic.h"

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4146)
#endif

/* ---- Wrappers for implementing blocking ---- */

/* Wrap a function for processing a single block to process an entire buffer in
 * parallel. */
int64_t bshuf_blocked_wrap_fun(bshufBlockFunDef fun, const void* in, void* out, \
        const size_t size, const size_t elem_size, size_t block_size) {

    omp_size_t ii = 0;
    int64_t err = 0;
    int64_t count, cum_count=0;
    size_t last_block_size;
    size_t leftover_bytes;
    size_t this_iter;
    char *last_in;
    char *last_out;


    ioc_chain C;
    ioc_init(&C, in, out);


    if (block_size == 0) {
        block_size = bshuf_default_block_size(elem_size);
    }
    if (block_size % BSHUF_BLOCKED_MULT) return -81;

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic, 1) \
            private(count) reduction(+ : cum_count)
#endif
    for (ii = 0; ii < (omp_size_t)( size / block_size ); ii ++) {
        count = fun(&C, block_size, elem_size);
        if (count < 0) err = count;
        cum_count += count;
    }

    last_block_size = size % block_size;
    last_block_size = last_block_size - last_block_size % BSHUF_BLOCKED_MULT;
    if (last_block_size) {
        count = fun(&C, last_block_size, elem_size);
        if (count < 0) err = count;
        cum_count += count;
    }

    if (err < 0) return err;

    leftover_bytes = size % BSHUF_BLOCKED_MULT * elem_size;
    //this_iter;
    last_in = (char *) ioc_get_in(&C, &this_iter);
    ioc_set_next_in(&C, &this_iter, (void *) (last_in + leftover_bytes));
    last_out = (char *) ioc_get_out(&C, &this_iter);
    ioc_set_next_out(&C, &this_iter, (void *) (last_out + leftover_bytes));

    memcpy(last_out, last_in, leftover_bytes);

    ioc_destroy(&C);

    return cum_count + leftover_bytes;
}


/* Bitshuffle a single block. */
int64_t bshuf_bitshuffle_block(ioc_chain *C_ptr, \
        const size_t size, const size_t elem_size) {

    size_t this_iter;
    const void *in;
    void *out;
    int64_t count;


    
    in = ioc_get_in(C_ptr, &this_iter);
    ioc_set_next_in(C_ptr, &this_iter,
            (void*) ((char*) in + size * elem_size));
    out = ioc_get_out(C_ptr, &this_iter);
    ioc_set_next_out(C_ptr, &this_iter,
            (void *) ((char *) out + size * elem_size));

    count = bshuf_trans_bit_elem(in, out, size, elem_size);
    return count;
}


/* Bitunshuffle a single block. */
int64_t bshuf_bitunshuffle_block(ioc_chain* C_ptr, \
        const size_t size, const size_t elem_size) {


    size_t this_iter;
    const void *in;
    void *out;
    int64_t count;




    in = ioc_get_in(C_ptr, &this_iter);
    ioc_set_next_in(C_ptr, &this_iter,
            (void*) ((char*) in + size * elem_size));
    out = ioc_get_out(C_ptr, &this_iter);
    ioc_set_next_out(C_ptr, &this_iter,
            (void *) ((char *) out + size * elem_size));

    count = bshuf_untrans_bit_elem(in, out, size, elem_size);
    return count;
}


/* Write a 64 bit unsigned integer to a buffer in big endian order. */
void bshuf_write_uint64_BE(void* buf, uint64_t num) {
    int ii;
    uint8_t* b = (uint8_t*) buf;
    uint64_t pow28 = 1 << 8;
    for (ii = 7; ii >= 0; ii--) {
        b[ii] = num % pow28;
        num = num / pow28;
    }
}


/* Read a 64 bit unsigned integer from a buffer big endian order. */
uint64_t bshuf_read_uint64_BE(void* buf) {
    int ii;
    uint8_t* b = (uint8_t*) buf;
    uint64_t num = 0, pow28 = 1 << 8, cp = 1;
    for (ii = 7; ii >= 0; ii--) {
        num += b[ii] * cp;
        cp *= pow28;
    }
    return num;
}


/* Write a 32 bit unsigned integer to a buffer in big endian order. */
void bshuf_write_uint32_BE(void* buf, uint32_t num) {
    int ii;
    uint8_t* b = (uint8_t*) buf;
    uint32_t pow28 = 1 << 8;
    for (ii = 3; ii >= 0; ii--) {
        b[ii] = num % pow28;
        num = num / pow28;
    }
}


/* Read a 32 bit unsigned integer from a buffer big endian order. */
uint32_t bshuf_read_uint32_BE(const void* buf) {
    int ii;
    uint8_t* b = (uint8_t*) buf;
    uint32_t num = 0, pow28 = 1 << 8, cp = 1;
    for (ii = 3; ii >= 0; ii--) {
        num += b[ii] * cp;
        cp *= pow28;
    }
    return num;
}

size_t bshuf_default_block_size(const size_t elem_size) {
    // This function needs to be absolutely stable between versions.
    // Otherwise encoded data will not be decodable.

    size_t block_size = BSHUF_TARGET_BLOCK_SIZE_B / elem_size;
    // Ensure it is a required multiple.
    block_size = (block_size / BSHUF_BLOCKED_MULT) * BSHUF_BLOCKED_MULT;
    return MAX(block_size, BSHUF_MIN_RECOMMEND_BLOCK);
}


int64_t bitshuffle_neon(const void* in, void* out, const size_t size,
        const size_t elem_size, size_t block_size) {

    return bshuf_blocked_wrap_fun(&bshuf_bitshuffle_block, in, out, size,
            elem_size, block_size);
}


int64_t bitunshuffle_neon(const void* in, void* out, const size_t size,
        const size_t elem_size, size_t block_size) {

    return bshuf_blocked_wrap_fun(&bshuf_bitunshuffle_block, in, out, size,
            elem_size, block_size);
}

#undef TRANS_BIT_8X8
#undef TRANS_ELEM_TYPE
#undef MAX
#undef CHECK_MULT_EIGHT
#undef CHECK_ERR_FREE


#ifdef _MSC_VER
#pragma warning (pop)
#endif

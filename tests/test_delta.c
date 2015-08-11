/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for Blosc delta filer.

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "../blosc/delta.h"

#define BUFFER_ALIGN_SIZE   8

int tests_run = 0;

/* Global vars */
uint8_t *src, *dest;
size_t size = 1*MB;
size_t i;


static char *test_encoder8(){
  memset(src, 1, size);
  memset(dest, 1, size);
  delta_encoder8((uint8_t*)src, (uint8_t*)dest, size);
  for (i=0; i < size; i++) {
      mu_assert("ERROR: delta_encoder8 result incorrect", dest[i] == 0);
  }

  return 0;
}

static char *test_delta_two(){
  return 0;
}

static char *all_tests() {
  mu_run_test(test_encoder8);
  mu_run_test(test_delta_two);
  return 0;
}


int main(int argc, char **argv) {
  char *result;

  printf("STARTING TESTS for %s\n", argv[0]);

  blosc_init();
  blosc_set_nthreads(1);

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  memset(src, 0, size);
  memset(dest, 0, size);

  /* Run all the suite */
  result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(src);
  blosc_test_free(dest);

  blosc_destroy();

  return result != 0;
}

#include "cobs_encode_max_c.h"

#include "../cobs.h"

enum { WORKS_AT_COMPILE_TIME = COBS_ENCODE_MAX(123) };

size_t cobs_encode_max_c(size_t decoded_len) {
  return COBS_ENCODE_MAX(decoded_len);
}

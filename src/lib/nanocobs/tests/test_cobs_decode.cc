#include "../cobs.h"
#include "byte_vec.h"
#include "doctest.h"

TEST_CASE("Decoding validation") {
  unsigned char dec[32];
  size_t dec_len;

  SUBCASE("Invalid payload: jump past end") {
    byte_vec_t enc{ 3, 0 };
    REQUIRE(cobs_decode(enc.data(), enc.size(), dec, sizeof(dec), &dec_len) ==
            COBS_RET_ERR_BAD_PAYLOAD);
  }

  SUBCASE("Invalid payload: jump over internal zeroes") {
    byte_vec_t enc{ 5, 1, 0, 0, 1, 0 };
    REQUIRE(cobs_decode(enc.data(), enc.size(), dec, sizeof(dec), &dec_len) ==
            COBS_RET_ERR_BAD_PAYLOAD);
  }
}

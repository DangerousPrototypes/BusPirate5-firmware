#include "cobs_encode_max_c.h"
#include "../cobs.h"
#include "doctest.h"

enum { WORKS_AT_COMPILE_TIME = COBS_ENCODE_MAX(123) };

TEST_CASE("COBS_ENCODE_MAX") {
  SUBCASE("0 bytes") {
    REQUIRE(COBS_ENCODE_MAX(0) == 2);
    REQUIRE(cobs_encode_max_c(0) == 2);
  }
  SUBCASE("1 byte") {
    REQUIRE(COBS_ENCODE_MAX(1) == 3);
    REQUIRE(cobs_encode_max_c(1) == 3);
  }
  SUBCASE("2 bytes") {
    REQUIRE(COBS_ENCODE_MAX(2) == 4);
    REQUIRE(cobs_encode_max_c(2) == 4);
  }

  SUBCASE("3 - 254 bytes") {
    for (auto i{ 3u }; i <= 254u; ++i) {
      REQUIRE(COBS_ENCODE_MAX(i) == i + 2);
      REQUIRE(cobs_encode_max_c(i) == i + 2);
    }
  }

  SUBCASE("255-508 bytes") {
    for (auto i{ 255u }; i <= 508u; ++i) {
      REQUIRE(COBS_ENCODE_MAX(i) == i + 3);
      REQUIRE(cobs_encode_max_c(i) == i + 3);
    }
  }

  SUBCASE("Input size plus boilerplate plus ceil(x/254)") {
    REQUIRE(COBS_ENCODE_MAX(12345) == 1 + 12345 + ((12345 + 253) / 254));
    REQUIRE(cobs_encode_max_c(12345) == 1 + 12345 + ((12345 + 253) / 254));
  }
}

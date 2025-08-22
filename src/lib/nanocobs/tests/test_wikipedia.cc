#include "../cobs.h"
#include "byte_vec.h"
#include "doctest.h"

#include <numeric>

namespace {

void round_trip_inplace(byte_vec_t const &decoded, byte_vec_t const &encoded) {
  byte_vec_t decoded_inplace{ COBS_TINYFRAME_SENTINEL_VALUE };
  decoded_inplace.insert(std::end(decoded_inplace), decoded.begin(), decoded.end());
  decoded_inplace.push_back(COBS_TINYFRAME_SENTINEL_VALUE);

  byte_vec_t x(decoded_inplace);

  REQUIRE(cobs_encode_tinyframe(x.data(), x.size()) == COBS_RET_SUCCESS);
  REQUIRE(x == encoded);
  REQUIRE(cobs_decode_tinyframe(x.data(), x.size()) == COBS_RET_SUCCESS);
  REQUIRE(x == decoded_inplace);
}

void round_trip(byte_vec_t const &decoded, byte_vec_t const &encoded) {
  std::array<byte_t, 512> enc_actual, dec_actual;
  size_t enc_actual_len{ 0u }, dec_actual_len{ 0u };

  REQUIRE(cobs_encode(decoded.data(),
                      decoded.size(),
                      enc_actual.data(),
                      enc_actual.size(),
                      &enc_actual_len) == COBS_RET_SUCCESS);

  REQUIRE(enc_actual_len == encoded.size());
  REQUIRE(encoded == byte_vec_t(enc_actual.data(), enc_actual.data() + enc_actual_len));

  REQUIRE(cobs_decode(enc_actual.data(),
                      enc_actual_len,
                      dec_actual.data(),
                      dec_actual.size(),
                      &dec_actual_len) == COBS_RET_SUCCESS);

  REQUIRE(dec_actual_len == decoded.size());
  REQUIRE(decoded == byte_vec_t(dec_actual.data(), dec_actual.data() + dec_actual_len));

  // Additionaly, in-place decode atop enc_actual using cobs_decode.

  REQUIRE(cobs_decode(enc_actual.data(),
                      enc_actual_len,
                      enc_actual.data(),
                      enc_actual.size(),
                      &dec_actual_len) == COBS_RET_SUCCESS);

  REQUIRE(dec_actual_len == decoded.size());
  REQUIRE(decoded == byte_vec_t(enc_actual.data(), enc_actual.data() + dec_actual_len));
}
}  // namespace

// https://wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing#Encoding_examples
TEST_CASE("Wikipedia round-trip examples") {
  SUBCASE("Example 1") {
    const byte_vec_t decoded{ 0x00 };
    const byte_vec_t encoded{ 0x01, 0x01, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 2") {
    const byte_vec_t decoded{ 0x00, 0x00 };
    const byte_vec_t encoded{ 0x01, 0x01, 0x01, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 3") {
    const byte_vec_t decoded{ 0x00, 0x11, 0x00 };
    const byte_vec_t encoded{ 0x01, 0x02, 0x11, 0x01, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 4") {
    const byte_vec_t decoded{ 0x11, 0x22, 0x00, 0x33 };
    const byte_vec_t encoded{ 0x03, 0x11, 0x22, 0x02, 0x33, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 5") {
    const byte_vec_t decoded{ 0x11, 0x22, 0x33, 0x44 };
    const byte_vec_t encoded{ 0x05, 0x11, 0x22, 0x33, 0x44, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 6") {
    const byte_vec_t decoded{ 0x11, 0x00, 0x00, 0x00 };
    const byte_vec_t encoded{ 0x02, 0x11, 0x01, 0x01, 0x01, 0x00 };
    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 7") {
    // 01 02 03 ... FD FE
    byte_vec_t decoded(254);
    std::iota(decoded.begin(), decoded.end(), byte_t{ 0x01 });

    // FF 01 02 03 ... FD FE 00
    byte_vec_t encoded(255);
    std::iota(encoded.begin(), encoded.end(), byte_t{ 0x00 });
    encoded[0] = 0xFF;
    encoded.push_back(0x00);

    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 8") {
    // 00 01 02 ... FC FD FE
    byte_vec_t decoded(255);
    std::iota(decoded.begin(), decoded.end(), byte_t{ 0x00 });

    // 01 FF 01 02 ... FC FD FE 00
    byte_vec_t encoded{ 0x01, 0xFF };
    for (byte_t i{ 0x01u }; i <= 0xFE; ++i) {
      encoded.push_back(i);
    }
    encoded.push_back(0x00);

    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }

  SUBCASE("Example 9") {
    // 01 02 03 ... FD FE FF
    byte_vec_t decoded(255);
    std::iota(decoded.begin(), decoded.end(), byte_t{ 0x01 });

    // FF 01 02 03 ... FD FE 02 FF 00
    byte_vec_t encoded(255);
    std::iota(encoded.begin(), encoded.end(), byte_t{ 0x00 });
    encoded[0] = 0xFF;
    encoded.insert(encoded.end(), { 0x02, 0xFF, 0x00 });

    round_trip(decoded, encoded);
  }

  SUBCASE("Example 10") {
    // 02 03 04 ... FE FF 00
    byte_vec_t decoded(255);
    std::iota(decoded.begin(), decoded.end(), byte_t{ 0x02 });
    decoded[decoded.size() - 1] = 0x00;

    // FF 02 03 04 ... FE FF 01 01 00
    byte_vec_t encoded(255);
    std::iota(encoded.begin(), encoded.end(), byte_t{ 0x01 });
    encoded[0] = 0xFF;
    encoded.insert(std::end(encoded), { 0x01, 0x01, 0x00 });

    round_trip(decoded, encoded);
  }

  SUBCASE("Example 11") {
    // 03 04 05 ... FF 00 01
    byte_vec_t decoded(253);
    std::iota(decoded.begin(), decoded.end(), byte_t{ 0x03 });
    decoded.insert(decoded.end(), { 0x00, 0x01 });

    // FE 03 04 05 ... FF 02 01 00
    byte_vec_t encoded{ 0xFE };
    for (auto i{ 0x03u }; i <= 0xFF; ++i) {
      encoded.push_back(static_cast<byte_t>(i));
    }
    encoded.insert(encoded.end(), { 0x02, 0x01, 0x00 });

    round_trip_inplace(decoded, encoded);
    round_trip(decoded, encoded);
  }
}

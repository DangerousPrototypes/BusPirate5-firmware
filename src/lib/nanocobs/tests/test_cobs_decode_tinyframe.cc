#include "../cobs.h"
#include "byte_vec.h"
#include "doctest.h"

#include <algorithm>
#include <cstring>
#include <numeric>

static constexpr byte_t CSV = COBS_TINYFRAME_SENTINEL_VALUE;

namespace {
cobs_ret_t cobs_decode_vec(byte_vec_t &v) {
  return cobs_decode_tinyframe(v.data(), static_cast<unsigned>(v.size()));
}
}  // namespace

TEST_CASE("Inplace decoding validation") {
  SUBCASE("Null buffer pointer") {
    REQUIRE(cobs_decode_tinyframe(nullptr, 123) == COBS_RET_ERR_BAD_ARG);
  }

  SUBCASE("Invalid buf_len") {
    char buf;
    REQUIRE(cobs_decode_tinyframe(&buf, 0) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_decode_tinyframe(&buf, 1) == COBS_RET_ERR_BAD_ARG);
  }

  SUBCASE("Invalid payload") {
    byte_vec_t buf{ 0x00, 0x00 };  // can't start with 0x00
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x01, 0x01 };  // must end with 0x00
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x01, 0x02, 0x00 };  // internal byte jumps past the end
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x03, 0x01, 0x00 };  // first byte jumps past end
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x01, 0x00, 0x00 };  // land on an interior 0x00
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x02, 0x00, 0x00 };  // jump over an interior 0x00
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
    buf = byte_vec_t{ 0x04, 0x01, 0x00, 0x01, 0x00 };  // jump over interior 0x00
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_ERR_BAD_PAYLOAD);
  }
}

TEST_CASE("Inplace decoding") {
  SUBCASE("Empty") {
    byte_vec_t buf{ 0x01, 0x00 };
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);
    REQUIRE(buf == byte_vec_t{ CSV, CSV });
  }

  SUBCASE("One nonzero byte") {
    byte_vec_t buf{ 0x02, 0x01, 0x00 };
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);
    REQUIRE(buf == byte_vec_t{ CSV, 0x01, CSV });
  }

  SUBCASE("One zero byte") {
    byte_vec_t buf{ 0x01, 0x01, 0x00 };
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);
    REQUIRE(buf == byte_vec_t{ CSV, 0x00, CSV });
  }

  SUBCASE("Safe payload, all zero bytes") {
    byte_vec_t buf(COBS_TINYFRAME_SAFE_BUFFER_SIZE);
    std::fill(std::begin(buf), std::end(buf), byte_t{ 0x01 });
    buf[buf.size() - 1] = 0x00;
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);

    byte_vec_t expected(buf.size());
    std::fill(std::begin(expected), std::end(expected), byte_t{ 0x00 });
    expected[0] = CSV;
    expected[expected.size() - 1] = CSV;

    REQUIRE(buf == expected);
  }

  SUBCASE("Safe payload, no zero bytes") {
    byte_vec_t buf(COBS_TINYFRAME_SAFE_BUFFER_SIZE);
    std::iota(std::begin(buf), std::end(buf), byte_t{ 0x00 });
    buf[0] = 0xFF;
    buf[buf.size() - 1] = 0x00;
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);

    byte_vec_t expected(buf.size());
    std::iota(std::begin(expected), std::end(expected), byte_t{ 0x00 });
    expected[0] = CSV;
    expected[expected.size() - 1] = CSV;
    REQUIRE(buf == expected);
  }

  SUBCASE("Unsafe payload with 254B jumps") {
    byte_vec_t buf{ 0xFF };
    buf.insert(std::end(buf), 254, 0x01);
    buf.push_back(0xFF);
    buf.insert(std::end(buf), 254, 0x01);
    buf.push_back(0xFF);
    buf.insert(std::end(buf), 254, 0x01);
    buf.push_back(0x00);
    REQUIRE(cobs_decode_vec(buf) == COBS_RET_SUCCESS);

    byte_vec_t expected{ CSV };
    expected.insert(std::end(expected), 254, 0x01);
    expected.push_back(0x00);
    expected.insert(std::end(expected), 254, 0x01);
    expected.push_back(0x00);
    expected.insert(std::end(expected), 254, 0x01);
    expected.push_back(CSV);
    REQUIRE(expected == buf);
  }
}

namespace {
void verify_decode_inplace(unsigned char *inplace, size_t payload_len) {
  byte_vec_t external(std::max(payload_len, size_t(1)));
  size_t external_len{ 0u };
  REQUIRE_MESSAGE(cobs_decode(inplace,
                              payload_len + 2,
                              external.data(),
                              external.size(),
                              &external_len) == COBS_RET_SUCCESS,
                  payload_len);

  REQUIRE(external_len == payload_len);
  REQUIRE(cobs_decode_tinyframe(inplace, payload_len + 2) == COBS_RET_SUCCESS);
  REQUIRE(byte_vec_t(inplace + 1, inplace + external_len + 1) ==
          byte_vec_t(external.data(), external.data() + external_len));
}

void fill_encode_inplace(byte_t *inplace, size_t payload_len, byte_t f) {
  inplace[0] = COBS_TINYFRAME_SENTINEL_VALUE;
  memset(inplace + 1, f, payload_len);
  inplace[payload_len + 1] = COBS_TINYFRAME_SENTINEL_VALUE;
  REQUIRE_MESSAGE(cobs_encode_tinyframe(inplace, payload_len + 2) == COBS_RET_SUCCESS,
                  payload_len);
}
}  // namespace

TEST_CASE("Decode: Inplace == External") {
  std::array<byte_t, COBS_TINYFRAME_SAFE_BUFFER_SIZE> inplace;

  SUBCASE("Fill with zeros") {
    for (auto i{ 0u }; i < inplace.size() - 2; ++i) {
      fill_encode_inplace(inplace.data(), i, 0x00);
      verify_decode_inplace(inplace.data(), i);
    }
  }

  SUBCASE("Fill with nonzeros") {
    for (auto i{ 0u }; i < inplace.size() - 2; ++i) {
      fill_encode_inplace(inplace.data(), i, 0x01);
      verify_decode_inplace(inplace.data(), i);
    }
  }

  SUBCASE("Fill with 0xFF") {
    for (auto i{ 0u }; i < inplace.size() - 2; ++i) {
      fill_encode_inplace(inplace.data(), i, 0xFF);
      verify_decode_inplace(inplace.data(), i);
    }
  }

  SUBCASE("Fill with zero/one pattern") {
    for (auto i{ 0u }; i < inplace.size() - 2; ++i) {
      inplace[0] = COBS_TINYFRAME_SENTINEL_VALUE;
      for (auto j{ 1u }; j < i; ++j) {
        inplace[j] = j & 1;
      }
      inplace[i + 1] = COBS_TINYFRAME_SENTINEL_VALUE;
      REQUIRE(cobs_encode_tinyframe(inplace.data(), i + 2) == COBS_RET_SUCCESS);
      verify_decode_inplace(inplace.data(), i);
    }
  }

  SUBCASE("Fill with one/zero pattern") {
    for (auto i{ 0u }; i < inplace.size() - 2; ++i) {
      inplace[0] = COBS_TINYFRAME_SENTINEL_VALUE;
      for (auto j{ 1u }; j < i; ++j) {
        inplace[j] = (j & 1) ^ 1;
      }
      inplace[i + 1] = COBS_TINYFRAME_SENTINEL_VALUE;
      REQUIRE(cobs_encode_tinyframe(inplace.data(), i + 2) == COBS_RET_SUCCESS);
      verify_decode_inplace(inplace.data(), i);
    }
  }
}

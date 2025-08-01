#include "../cobs.h"
#include "byte_vec.h"
#include "doctest.h"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

TEST_CASE("cobs_encode_inc_begin") {
  cobs_enc_ctx_t ctx;
  byte_vec_t buf(1024);

  SUBCASE("bad args") {
    REQUIRE(cobs_encode_inc_begin(nullptr, buf.size(), &ctx) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_encode_inc_begin(buf.data(), 0, &ctx) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_encode_inc_begin(buf.data(), 1, &ctx) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_encode_inc_begin(buf.data(), 2, nullptr) == COBS_RET_ERR_BAD_ARG);
  }

  SUBCASE("initializes context") {
    ctx.cur = 123;
    ctx.code_idx = 456;
    ctx.code = 789;
    ctx.need_advance = 1;
    REQUIRE(cobs_encode_inc_begin(buf.data(), buf.size(), &ctx) == COBS_RET_SUCCESS);
    REQUIRE(ctx.dst == buf.data());
    REQUIRE(ctx.dst_max == buf.size());
    REQUIRE(ctx.cur == 1);
    REQUIRE(ctx.code == 1);
    REQUIRE(ctx.code_idx == 0);
    REQUIRE(ctx.need_advance == 0);
  }
}

TEST_CASE("cobs_encode_inc") {
  cobs_enc_ctx_t ctx{ .dst = nullptr,
                      .dst_max = 0,
                      .cur = 0,
                      .code_idx = 0,
                      .code = 0,
                      .need_advance = 0 };
  size_t constexpr enc_max{ 1024 };
  byte_vec_t enc_buf(enc_max);
  size_t constexpr dec_max{ 1024 };
  byte_vec_t dec_buf(dec_max);

  REQUIRE(cobs_encode_inc_begin(enc_buf.data(), enc_max, &ctx) == COBS_RET_SUCCESS);

  SUBCASE("bad args") {
    REQUIRE(cobs_encode_inc(nullptr, dec_buf.data(), 16) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_encode_inc(&ctx, nullptr, 16) == COBS_RET_ERR_BAD_ARG);
  }

  SUBCASE("zero-byte write") {
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 0) == COBS_RET_SUCCESS);
    ctx.cur = ctx.dst_max - 1;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 0) == COBS_RET_SUCCESS);
  }

  SUBCASE("exhausted") {
    ctx.cur = ctx.dst_max - 1;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_ERR_EXHAUSTED);
    ctx.cur = ctx.dst_max - 2;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 2) == COBS_RET_ERR_EXHAUSTED);
    ctx.cur = 0;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), ctx.dst_max) == COBS_RET_ERR_EXHAUSTED);
  }

  SUBCASE("accumulates nonzero bytes into buffer") {
    dec_buf[0] = 0x12;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(enc_buf[1] == 0x12);

    for (byte_t i{ 0 }; i < 10; ++i) {
      dec_buf[i] = i;
    }
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 10) == COBS_RET_SUCCESS);
    for (byte_t i{ 0 }; i < 10; ++i) {
      REQUIRE(enc_buf[2 + i] == i);
    }
  }

  SUBCASE("advances cursor with every byte written") {
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(ctx.cur == 2);
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 12) == COBS_RET_SUCCESS);
    REQUIRE(ctx.cur == 14);
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 37) == COBS_RET_SUCCESS);
    REQUIRE(ctx.cur == 51);
  }

  SUBCASE("Nonzero bytes increment code") {
    std::fill(std::begin(dec_buf), std::end(dec_buf), byte_t{ 1 });
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(ctx.code == 2);
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 13) == COBS_RET_SUCCESS);
    REQUIRE(ctx.code == 15);
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 59) == COBS_RET_SUCCESS);
    REQUIRE(ctx.code == 74);
  }

  SUBCASE("Encoding a zero byte writes and resets the code") {
    ctx.code = 23;
    dec_buf[0] = 0;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(enc_buf[0] == 23);
    REQUIRE(ctx.code == 1);
  }

  SUBCASE("Encoding the 255th non-zero byte writes + resets the code") {
    ctx.code = 254;
    dec_buf[0] = 1;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(enc_buf[0] == 255);
    REQUIRE(ctx.code == 1);
  }

  SUBCASE("Remembers the need to advance when final src byte is 255th nonzero") {
    ctx.code = 254;
    dec_buf[0] = 1;
    ctx.need_advance = 0;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(ctx.need_advance == 1);
  }

  SUBCASE("Advances the cursor before additional encoding when flagged") {
    ctx.need_advance = 1;
    ctx.cur = 12;
    dec_buf[0] = 234;
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), 1) == COBS_RET_SUCCESS);
    REQUIRE(ctx.need_advance == 0);
    REQUIRE(enc_buf[13] == 234);
  }
}

TEST_CASE("cobs_encode_inc_end") {
  cobs_enc_ctx_t ctx;
  size_t constexpr enc_max{ 1024 };
  byte_vec_t enc_buf(enc_max);
  size_t enc_len;

  REQUIRE(cobs_encode_inc_begin(enc_buf.data(), enc_max, &ctx) == COBS_RET_SUCCESS);

  SUBCASE("bad args") {
    REQUIRE(cobs_encode_inc_end(nullptr, &enc_len) == COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_encode_inc_end(&ctx, nullptr) == COBS_RET_ERR_BAD_ARG);
  }

  SUBCASE("Writes final code") {
    ctx.code = 123;
    ctx.code_idx = 7;
    REQUIRE(cobs_encode_inc_end(&ctx, &enc_len) == COBS_RET_SUCCESS);
    REQUIRE(enc_buf[ctx.code_idx] == ctx.code);
  }

  SUBCASE("Writes final frame delimiter") {
    ctx.cur = 97;
    enc_buf[97] = 0xFD;
    REQUIRE(cobs_encode_inc_end(&ctx, &enc_len) == COBS_RET_SUCCESS);
    REQUIRE(enc_buf[97] == 0);
  }

  SUBCASE("Encoded length is final cursor position") {
    ctx.cur = 331;
    REQUIRE(cobs_encode_inc_end(&ctx, &enc_len) == COBS_RET_SUCCESS);
    REQUIRE(enc_len == 332);
  }
}

namespace {
byte_vec_t encode_single(byte_vec_t const &dec) {
  byte_vec_t enc(COBS_ENCODE_MAX(dec.size()));
  size_t enc_len{ 0u };
  REQUIRE(cobs_encode(dec.data(), dec.size(), enc.data(), enc.size(), &enc_len) ==
          COBS_RET_SUCCESS);
  enc.resize(enc_len);
  return enc;
}

byte_vec_t encode_incremental(byte_vec_t const &decoded, size_t chunk_size) {
  byte_vec_t encoded(COBS_ENCODE_MAX(decoded.size()));

  cobs_enc_ctx_t ctx;
  REQUIRE(cobs_encode_inc_begin(encoded.data(), encoded.size(), &ctx) == COBS_RET_SUCCESS);

  size_t cur = 0;
  while (cur < decoded.size()) {
    size_t const encode_size{ std::min(chunk_size, decoded.size() - cur) };
    REQUIRE(cobs_encode_inc(&ctx, &decoded[cur], encode_size) == COBS_RET_SUCCESS);
    cur += encode_size;
  }

  size_t len{ 0u };
  REQUIRE(cobs_encode_inc_end(&ctx, &len) == COBS_RET_SUCCESS);
  encoded.resize(len);
  return encoded;
}

void require_equal(byte_vec_t const &v1, byte_vec_t const &v2) {
  std::ostringstream ss;
  ss << std::endl << "v1 (" << v1.size() << "):" << std::endl;
  std::ostringstream ss_v1;
  ss_v1 << std::hex << std::setfill('0');
  for (auto const &b : v1) {
    ss_v1 << std::setw(2) << unsigned{ b } << ' ';
  }
  ss << ss_v1.str() << std::endl << std::endl << "v2 (" << v2.size() << "):" << std::endl;
  std::ostringstream ss_v2;
  ss_v2 << std::hex << std::setfill('0');
  for (auto const &b : v2) {
    ss_v2 << std::setw(2) << unsigned{ b } << ' ';
  }
  ss << ss_v2.str() << std::endl;
  REQUIRE_MESSAGE(v1 == v2, ss.str().c_str());
}
}  // namespace

TEST_CASE("Single/multi-encode equivalences") {
  cobs_enc_ctx_t ctx;
  size_t constexpr enc_max{ 4096 };
  byte_vec_t enc_buf(enc_max);
  size_t constexpr dec_max{ 4096 };
  byte_vec_t dec_buf(dec_max);

  REQUIRE(cobs_encode_inc_begin(enc_buf.data(), enc_max, &ctx) == COBS_RET_SUCCESS);

  SUBCASE("One byte at a time") {
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 1);
    require_equal(single, incremental);
  }

  SUBCASE("One byte at a time, all zero payload") {
    std::fill(std::begin(dec_buf), std::end(dec_buf), byte_t{ 0 });
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 1);
    require_equal(single, incremental);
  }

  SUBCASE("Two bytes at a time") {
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 2);
    require_equal(single, incremental);
  }

  SUBCASE("Three bytes at a time") {
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 3);
    require_equal(single, incremental);
  }

  SUBCASE("Eleven bytes at a time") {
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 11);
    require_equal(single, incremental);
  }

  SUBCASE("Many bytes at a time, all zero payload") {
    std::fill(std::begin(dec_buf), std::end(dec_buf), byte_t{ 0 });
    for (auto i{ 0u }; i < 1500; ++i) {
      dec_buf[i] = i & 0xFF;
    }
    dec_buf.resize(1500);
    byte_vec_t const single = encode_single(dec_buf);
    byte_vec_t const incremental = encode_incremental(dec_buf, 31);
    require_equal(single, incremental);
  }

  SUBCASE("Header / payload split") {
    byte_t constexpr h[]{ 0x02, 0x03, 0xCC, 0xDF, 0x13, 0x49 };

    dec_buf.resize(400);
    std::iota(std::begin(dec_buf), std::end(dec_buf), byte_t{ 0x01 });
    dec_buf[4] = 0x00;
    dec_buf[27] = 0x00;
    dec_buf[45] = 0x00;
    dec_buf[68] = 0x00;

    REQUIRE(cobs_encode_inc(&ctx, h, sizeof(h)) == COBS_RET_SUCCESS);
    REQUIRE(cobs_encode_inc(&ctx, dec_buf.data(), dec_buf.size()) == COBS_RET_SUCCESS);

    size_t len{ 0u };
    REQUIRE(cobs_encode_inc_end(&ctx, &len) == COBS_RET_SUCCESS);
    enc_buf.resize(len);

    byte_vec_t single_dec(h, h + sizeof(h));
    single_dec.insert(std::end(single_dec),
                      dec_buf.data(),
                      dec_buf.data() + dec_buf.size());
    REQUIRE(enc_buf == encode_single(single_dec));
  }
}

#include "../cobs.h"
#include "byte_vec.h"
#include "doctest.h"

#include <algorithm>
#include <cstring>
#include <numeric>

TEST_CASE("cobs_decode_inc_begin") {
  REQUIRE(cobs_decode_inc_begin(nullptr) == COBS_RET_ERR_BAD_ARG);

  cobs_decode_inc_ctx_t c;
  c.state = cobs_decode_inc_ctx_t::cobs_decode_inc_state(
      cobs_decode_inc_ctx_t::COBS_DECODE_READ_CODE + 1);
  REQUIRE(cobs_decode_inc_begin(&c) == COBS_RET_SUCCESS);
  REQUIRE(c.state == cobs_decode_inc_ctx_t::COBS_DECODE_READ_CODE);
}

TEST_CASE("cobs_decode_inc") {
  cobs_decode_inc_ctx_t ctx;
  REQUIRE(cobs_decode_inc_begin(&ctx) == COBS_RET_SUCCESS);

  byte_vec_t enc(1024), dec(enc.size() * 2);
  cobs_decode_inc_args_t args{ .enc_src = enc.data(),
                               .dec_dst = dec.data(),
                               .enc_src_max = enc.size(),
                               .dec_dst_max = dec.size() };
  size_t enc_len{ 0u }, dec_len{ 0u };
  bool done{ false };

  SUBCASE("bad args") {
    REQUIRE(cobs_decode_inc(nullptr, &args, &enc_len, &dec_len, &done) ==
            COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_decode_inc(&ctx, nullptr, &enc_len, &dec_len, &done) ==
            COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_decode_inc(&ctx, &args, nullptr, &dec_len, &done) ==
            COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_decode_inc(&ctx, &args, &enc_len, nullptr, &done) ==
            COBS_RET_ERR_BAD_ARG);
    REQUIRE(cobs_decode_inc(&ctx, &args, &enc_len, &dec_len, nullptr) ==
            COBS_RET_ERR_BAD_ARG);

    SUBCASE("args.src") {
      args.enc_src = nullptr;
      REQUIRE(cobs_decode_inc(&ctx, &args, &enc_len, &dec_len, &done) ==
              COBS_RET_ERR_BAD_ARG);
    }
    SUBCASE("args.dst") {
      args.dec_dst = nullptr;
      REQUIRE(cobs_decode_inc(&ctx, &args, &enc_len, &dec_len, &done) ==
              COBS_RET_ERR_BAD_ARG);
    }
  }

  // fill the buffer with patterns and 0x00 runs
  dec_len = 900;
  memset(dec.data(), 0xAA, dec_len);
  memset(&dec[10], 0x00, 3);
  memset(&dec[99], 0x00, 5);
  memset(&dec[413], 0x00, 9);
  std::iota(&dec[500], &dec[500 + 300], byte_t{ 0u });

  // encode the test buffer into |enc|
  REQUIRE(cobs_encode(dec.data(), dec_len, enc.data(), enc.size(), &enc_len) ==
          COBS_RET_SUCCESS);
  std::fill(dec.begin(), dec.end(), byte_t{ 0u });
  REQUIRE(enc_len >= dec_len);
  REQUIRE(enc_len <= COBS_ENCODE_MAX(dec_len));

  // Do a full decode into a reference buffer for comparison later
  byte_vec_t oneshot(enc.size());
  {
    size_t oneshot_len{ 0u };
    REQUIRE(
        cobs_decode(enc.data(), enc_len, oneshot.data(), oneshot.size(), &oneshot_len) ==
        COBS_RET_SUCCESS);
    oneshot.resize(oneshot_len);
  }

  size_t cur_dec{ 0 }, cur_enc{ 0 };

  auto const decode_inc{ [&](size_t const enc_size, size_t const dec_size) {
    while (!done) {
      args.enc_src = &enc[cur_enc];
      args.dec_dst = &dec[cur_dec];
      args.enc_src_max = std::min(enc_size, enc_len - cur_enc);
      args.dec_dst_max = std::min(dec_size, oneshot.size() - cur_dec);

      size_t this_enc_len{ 0u }, this_dec_len{ 0u };
      REQUIRE(cobs_decode_inc(&ctx, &args, &this_enc_len, &this_dec_len, &done) ==
              COBS_RET_SUCCESS);
      cur_dec += this_dec_len;
      cur_enc += this_enc_len;
    }

    REQUIRE(cur_dec == oneshot.size());
    dec.resize(cur_dec);
    REQUIRE(dec == oneshot);
  } };

  SUBCASE("1 byte enc, full dec") {
    decode_inc(1, oneshot.size());
  }

  SUBCASE("full enc, 1 byte dec") {
    decode_inc(enc_len, 1);
  }

  SUBCASE("1 byte enc, 1 byte dec") {
    decode_inc(1, 1);
  }

  SUBCASE("relative primes") {
    decode_inc(19, 29);
  }

  SUBCASE("full") {
    decode_inc(enc_len, oneshot.size());
  }
}

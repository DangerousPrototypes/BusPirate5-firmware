// SPDX-License-Identifier: Unlicense OR 0BSD

// nanocobs v0.1.0, Charles Nicholson (charles.nicholson@gmail.com)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  COBS_RET_SUCCESS = 0,
  COBS_RET_ERR_BAD_ARG,
  COBS_RET_ERR_BAD_PAYLOAD,
  COBS_RET_ERR_EXHAUSTED
} cobs_ret_t;

enum {
  // All COBS frames end with this value. If you're scanning a data source for
  // frame delimiters, the presence of this zero byte indicates the completion
  // of a frame.
  COBS_FRAME_DELIMITER = 0x00,

  // In-place encoding mandatory placeholder byte values.
  COBS_TINYFRAME_SENTINEL_VALUE = 0x5A,

  // In-place encodings that fit in a buffer of this size will always succeed.
  COBS_TINYFRAME_SAFE_BUFFER_SIZE = 256
};

// COBS_ENCODE_MAX
//
// Returns the maximum possible size in bytes of the buffer required to encode a buffer of
// length |dec_len|. Cannot fail. Defined as macro/constexpr to facilitate compile-time
// sizing of buffers.
#ifdef __cplusplus
inline constexpr size_t COBS_ENCODE_MAX(size_t DECODED_LEN) {
  return 1 + DECODED_LEN + ((DECODED_LEN + 253) / 254) + (DECODED_LEN == 0);
}
#else
// In C, DECODED_LEN is evaluated multiple times; don't call with mutating expressions!
// e.g. Don't do "COBS_ENCODE_MAX(i++)".
#define COBS_ENCODE_MAX(DECODED_LEN) \
  (1 + (DECODED_LEN) + (((DECODED_LEN) + 253) / 254) + ((DECODED_LEN) == 0))
#endif

// cobs_encode_tinyframe
//
// Encode in-place the contents of the provided buffer |buf| of length |len|. Returns
// COBS_RET_SUCCESS on successful encoding.
//
// Because encoding adds leading and trailing bytes, your buffer must reserve bytes 0 and
// len-1 for the encoding. If the first and last bytes of |buf| are not set to
// COBS_INPLACE_SENTINEL_VALUE, the function will fail with COBS_RET_ERR_BAD_PAYLOAD.
//
// If a null pointer or invalid length are provided, the function will fail with
// COBS_RET_ERR_BAD_ARG.
//
// If |len| is less than or equal to COBS_INPLACE_SAFE_BUFFER_SIZE, the contents of |buf|
// will never cause encoding to fail. If |len| is larger than
// COBS_INPLACE_SAFE_BUFFER_SIZE, encoding can possibly fail with COBS_RET_ERR_BAD_PAYLOAD
// if there are more than 254 bytes between zeros.
//
// If the function returns COBS_RET_ERR_BAD_PAYLOAD, the contents of |buf| are left
// indeterminate and must not be relied on to be fully encoded or decoded.
cobs_ret_t cobs_encode_tinyframe(void *buf, size_t len);

// cobs_decode_tinyframe
//
// Decode in-place the contents of the provided buffer |buf| of length |len|.
// Returns COBS_RET_SUCCESS on successful decoding.
//
// Because decoding is in-place, the first and last bytes of |buf| will be set to the value
// COBS_INPLACE_SENTINEL_VALUE if decoding succeeds. The decoded contents are stored in the
// inclusive span defined by buf[1] and buf[len-2].
//
// If a null pointer or invalid length are provided, the function will fail with
// COBS_RET_ERR_BAD_ARG.
//
// If the encoded buffer contains any code bytes that exceed |len|, the function will fail
// with COBS_RET_ERR_BAD_PAYLOAD. If the buffer starts with a 0 byte, or ends in a nonzero
// byte, the function will fail with COBS_RET_ERR_BAD_PAYLOAD.
//
// If the function returns COBS_RET_ERR_BAD_PAYLOAD, the contents of |buf| are left
// indeterminate and must not be relied on to be fully encoded or decoded.
cobs_ret_t cobs_decode_tinyframe(void *buf, size_t len);

// cobs_decode
//
// Decode |enc_len| encoded bytes from |enc| into |out_dec|, storing the decoded length in
// |out_dec_len|. Returns COBS_RET_SUCCESS on successful decoding.
//
// If any of the input pointers are null, or if any of the lengths are invalid, the
// function will fail with COBS_RET_ERR_BAD_ARG.
//
// If |enc| starts with a 0 byte, or does not end with a 0 byte, the function will fail
// with COBS_RET_ERR_BAD_PAYLOAD.
//
// If the decoding exceeds |dec_max| bytes, the function will fail with
// COBS_RET_ERR_EXHAUSTED.
cobs_ret_t cobs_decode(void const *enc,
                       size_t enc_len,
                       void *out_dec,
                       size_t dec_max,
                       size_t *out_dec_len);

// cobs_encode
//
// Encode |dec_len| decoded bytes from |dec| into |out_enc|, storing the encoded length in
// |out_enc_len|. Returns COBS_RET_SUCCESS on successful encoding.
//
// If any of the input pointers are null, or if any of the lengths are invalid, the
// function will fail with COBS_RET_ERR_BAD_ARG.
//
// If the encoding exceeds |enc_max| bytes, the function will fail with
// COBS_RET_ERR_EXHAUSTED.
cobs_ret_t cobs_encode(void const *dec,
                       size_t dec_len,
                       void *out_enc,
                       size_t enc_max,
                       size_t *out_enc_len);

// Incremental encoding API

typedef struct cobs_enc_ctx {
  void *dst;
  size_t dst_max;
  size_t cur;
  size_t code_idx;
  unsigned code;
  int need_advance;
} cobs_enc_ctx_t;

// cobs_encode_inc_begin
//
// Begin an incremental encoding of data into |out_enc|. The intermediate encoding state is
// stored in |out_ctx|, which can then be passed into calls to cobs_encode_inc. Returns
// COBS_RET_SUCCESS if |out_ctx| can be used in future calls to cobs_encode_inc.
//
// If |out_enc| or |out_ctx| are null, or if |enc_max| is not large enough to hold the
// smallest possible encoding, the function will return COBS_RET_ERR_BAD_ARG.
cobs_ret_t cobs_encode_inc_begin(void *out_enc, size_t enc_max, cobs_enc_ctx_t *out_ctx);

// cobs_encode_inc
//
// Continue an encoding in progress with the new |dec| buffer of length |dec_len|. Encodes
// |dec_len| decoded bytes from |dec| into the buffer that |ctx| was initialized with in
// cobs_encode_inc_begin.
//
// If any of the input pointers are null, or |dec_len| is zero, the function will fail with
// COBS_RET_ERR_BAD_ARG.
//
// If the contents pointed to by |dec| can not be encoded in the remaining available buffer
// space, the function returns COBS_RET_ERR_EXHAUSTED. In this case, |ctx| remains
// unchanged and incremental encoding can be attempted again with different data, or
// finished with cobs_encode_inc_end.
//
// If the contents of |dec| are successfully encoded, the function returns
// COBS_RET_SUCCESS.
cobs_ret_t cobs_encode_inc(cobs_enc_ctx_t *ctx, void const *dec_src, size_t dec_len);

// cobs_encode_inc_end
//
// Finish an incremental encoding by writing the final code and delimiter.
// Returns COBS_RET_SUCCESS on success, and no further calls to cobs_encode_inc or
// cobs_encode_inc_end can be safely made until |ctx| is re-initialized via a new call to
// cobs_encode_inc_begin.
//
// The final encoded length is written to |out_enc_len|, and the buffer passed to
// cobs_encode_inc_begin holds the full COBS-encoded frame.
//
// If null pointers are provided, the function returns COBS_RET_ERR_BAD_ARG.
cobs_ret_t cobs_encode_inc_end(cobs_enc_ctx_t *ctx, size_t *out_enc_len);

// Incremental decoding API

typedef struct cobs_decode_inc_ctx {
  enum cobs_decode_inc_state {
    COBS_DECODE_READ_CODE,
    COBS_DECODE_RUN,
    COBS_DECODE_FINISH_RUN
  } state;
  uint8_t block, code;
} cobs_decode_inc_ctx_t;

typedef struct cobs_decode_inc_args {
  void const *enc_src;  // pointer to current position of encoded payload
  void *dec_dst;        // pointer to decoded output buffer.
  size_t enc_src_max;   // length of the |src| input buffer.
  size_t dec_dst_max;   // length of the |dst| output buffer.
} cobs_decode_inc_args_t;

cobs_ret_t cobs_decode_inc_begin(cobs_decode_inc_ctx_t *ctx);
cobs_ret_t cobs_decode_inc(cobs_decode_inc_ctx_t *ctx,
                           cobs_decode_inc_args_t const *args,
                           size_t *out_enc_src_len,  // how many bytes of src were read
                           size_t *out_dec_dst_len,  // how many bytes written to dst
                           bool *out_decode_complete);

#ifdef __cplusplus
}
#endif

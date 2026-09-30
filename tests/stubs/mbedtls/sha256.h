#pragma once
// Host substitute for the hash API. Production links ESP-IDF's mbedTLS.
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
using mbedtls_sha256_context = CC_SHA256_CTX;
inline void mbedtls_sha256_init(mbedtls_sha256_context *) {}
inline int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int) { return CC_SHA256_Init(ctx) == 1 ? 0 : -1; }
inline int mbedtls_sha256_update(mbedtls_sha256_context *ctx, const uint8_t *data, size_t size) {
  return CC_SHA256_Update(ctx, data, static_cast<CC_LONG>(size)) == 1 ? 0 : -1;
}
inline int mbedtls_sha256_finish(mbedtls_sha256_context *ctx, uint8_t *digest) {
  return CC_SHA256_Final(digest, ctx) == 1 ? 0 : -1;
}
inline void mbedtls_sha256_free(mbedtls_sha256_context *) {}
#else
#include <openssl/evp.h>
struct mbedtls_sha256_context { EVP_MD_CTX *ctx; };
inline void mbedtls_sha256_init(mbedtls_sha256_context *ctx) { ctx->ctx = EVP_MD_CTX_new(); }
inline int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int) {
  return ctx->ctx && EVP_DigestInit_ex(ctx->ctx, EVP_sha256(), nullptr) == 1 ? 0 : -1;
}
inline int mbedtls_sha256_update(mbedtls_sha256_context *ctx, const uint8_t *data, size_t size) {
  return EVP_DigestUpdate(ctx->ctx, data, size) == 1 ? 0 : -1;
}
inline int mbedtls_sha256_finish(mbedtls_sha256_context *ctx, uint8_t *digest) {
  return EVP_DigestFinal_ex(ctx->ctx, digest, nullptr) == 1 ? 0 : -1;
}
inline void mbedtls_sha256_free(mbedtls_sha256_context *ctx) { EVP_MD_CTX_free(ctx->ctx); }
#endif
inline int mbedtls_sha256(const uint8_t *data, size_t size, uint8_t *digest, int mode) {
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  const int result = mbedtls_sha256_starts(&context, mode) || mbedtls_sha256_update(&context, data, size) ||
      mbedtls_sha256_finish(&context, digest);
  mbedtls_sha256_free(&context);
  return result;
}

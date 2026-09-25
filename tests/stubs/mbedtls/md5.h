#pragma once
#include <cstddef>
#include <cstdint>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
inline int mbedtls_md5(const uint8_t* data, size_t size, uint8_t* out) {
  return CC_MD5(data, static_cast<CC_LONG>(size), out) ? 0 : -1;
}
#else
#include <openssl/evp.h>
inline int mbedtls_md5(const uint8_t* data, size_t size, uint8_t* out) {
  auto* context = EVP_MD_CTX_new();
  if (!context) return -1;
  const bool ok = EVP_DigestInit_ex(context, EVP_md5(), nullptr) == 1 && EVP_DigestUpdate(context, data, size) == 1 &&
                  EVP_DigestFinal_ex(context, out, nullptr) == 1;
  EVP_MD_CTX_free(context);
  return ok ? 0 : -1;
}
#endif

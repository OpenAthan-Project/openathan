#pragma once
#include <cstddef>
#include <cstdint>
inline bool signature_valid=true;
struct mbedtls_pk_context{};
constexpr int MBEDTLS_MD_SHA256=1;
inline void mbedtls_pk_init(mbedtls_pk_context *){}
inline void mbedtls_pk_free(mbedtls_pk_context *){}
inline int mbedtls_pk_parse_public_key(mbedtls_pk_context *,const uint8_t *,size_t){return 0;}
inline int mbedtls_pk_verify(mbedtls_pk_context *,int,const uint8_t *,size_t,const uint8_t *,size_t){return signature_valid?0:-1;}

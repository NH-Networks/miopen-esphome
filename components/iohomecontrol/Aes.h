#ifndef AES_H_
#define AES_H_

#include <cstdint>
#include <cstddef>

typedef uint8_t state_t[4][4];

#ifndef CBC
  #define CBC 0
#endif

#ifndef ECB
  #define ECB 1
#endif

#ifndef CTR
  #define CTR 0
#endif

#define AES128 1
#define AES192 0
#define AES256 0

#define AES_BLOCKLEN 16

#if defined(AES256) && (AES256 == 1)
    #define AES_KEYLEN 32
    #define AES_keyExpSize 240
#elif defined(AES192) && (AES192 == 1)
    #define AES_KEYLEN 24
    #define AES_keyExpSize 208
#else
    #define AES_KEYLEN 16
    #define AES_keyExpSize 176
#endif

struct AES_ctx {
  uint8_t RoundKey[AES_keyExpSize];
#if (defined(CBC) && (CBC == 1)) || (defined(CTR) && (CTR == 1))
  uint8_t Iv[AES_BLOCKLEN];
#endif
} __attribute__((aligned(4)));

void AES_init_ctx(AES_ctx* ctx, const uint8_t* key);
#if (defined(CBC) && (CBC == 1)) || (defined(CTR) && (CTR == 1))
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
void AES_ctx_set_iv(struct AES_ctx* ctx, const uint8_t* iv);
#endif

#if defined(ECB) && (ECB == 1)
void AES_ECB_encrypt(const AES_ctx* ctx, uint8_t* buf);
void AES_ECB_decrypt(const AES_ctx* ctx, uint8_t* buf);
#endif

#if defined(CBC) && (CBC == 1)
void AES_CBC_encrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
void AES_CBC_decrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#if defined(CTR) && (CTR == 1)
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
#endif

#endif

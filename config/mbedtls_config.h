/* config/mbedtls_config.h
 * Pico W (RP2040) + FreeRTOS + HTTPS client, Mbed TLS 3.x
 * TLS 1.2, ECDHE-ECDSA/ECDHE-RSA, AES-GCM, SHA-256, X.509 (PEM).
 * No include guards on purpose to avoid being skipped by the SDK wrapper.
 */

/* --- Neutralize PSA paths that pull extra prereqs in Pico SDK --- */
#undef MBEDTLS_USE_PSA_CRYPTO
#undef MBEDTLS_PSA_CRYPTO_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C

#define MBEDTLS_PLATFORM_MS_TIME_ALT

/* Timing is often pulled in by TLS; keep if you already use it */
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_NET_C

/* You already have a millisecond time source via MBEDTLS_PLATFORM_MS_TIME_ALT */
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE

/* --- Core error/platform --- */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_ERROR_C

/* --- Entropy / RNG (Pico provides mbedtls_hardware_poll) --- */
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_CTR_DRBG_C

/* --- Hash / message-digest --- */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
/* Optional; helps with some legacy chains */
#define MBEDTLS_SHA1_C

/* --- Base64 for PEM --- */
#define MBEDTLS_BASE64_C

/* --- Symmetric / AEAD --- */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_GCM_C

/* --- Big-num / PK --- */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

/* --- ECC + curves --- */
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED

/* --- RSA (for ECDHE-RSA servers) --- */
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

/* --- ASN.1 / OID --- */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_OID_C

/* --- X.509 & PEM parsing --- */
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

/* --- Time support (X.509 verification often expects time available) --- */
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
/* Pico provides platform time hooks via alt; leave default unless you wire a custom one
   #define MBEDTLS_PLATFORM_TIME_ALT
*/

/* --- TLS client --- */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

/* Key exchanges we actually want */
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED

/* --- Memory limits (tune if needed) --- */
#ifndef MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_MAX_CONTENT_LEN 4096
#endif

/* Optional debug
   #define MBEDTLS_DEBUG_C
*/

#include <Arduino.h>
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

const char *publicKeyPem = R"KEY(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEYg...
-----END PUBLIC KEY-----
)KEY";

// Example serial and signature from server
const char *serial = "ACDE482FD123";
const char *signatureBase64 = "MEYCIQD9Pq6mJ...FNhg==";

bool verifySignature(const char *serial, const char *signatureB64, const char *pubKeyPem) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)pubKeyPem, strlen(pubKeyPem) + 1);
  if (ret != 0) {
    Serial.printf("Public key parse failed: %d\n", ret);
    return false;
  }

  // Decode base64 signature
  unsigned char sig[128];
  size_t sigLen;
  mbedtls_base64_decode(sig, sizeof(sig), &sigLen, (const unsigned char *)signatureB64, strlen(signatureB64));

  // Hash the serial
  unsigned char hash[32];
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, (const unsigned char *)serial, strlen(serial));
  mbedtls_sha256_finish(&sha, hash);

  // Verify
  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 0, sig, sigLen);

  mbedtls_pk_free(&pk);
  return (ret == 0);
}

void setup() {
  Serial.begin(115200);
  bool ok = verifySignature(serial, signatureBase64, publicKeyPem);
  if (ok)
    Serial.println("✅ Activation verified — genuine device.");
  else
    Serial.println("❌ Invalid activation code!");
}

void loop() {}

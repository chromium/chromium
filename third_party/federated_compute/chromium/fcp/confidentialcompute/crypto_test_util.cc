#include "fcp/confidentialcompute/crypto_test_util.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/check_op.h"
#include "fcp/base/monitoring.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/confidentialcompute/crypto.h"
#include "openssl/hpke.h"

namespace fcp::confidential_compute {

std::pair<std::string, std::string> GenerateHpkeKeyPair(
    absl::string_view key_id) {
  bssl::ScopedEVP_HPKE_KEY key;
  CHECK_EQ(EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256()), 1);

  size_t key_len;
  std::string raw_public_key(EVP_HPKE_MAX_PUBLIC_KEY_LENGTH, '\0');
  CHECK_EQ(EVP_HPKE_KEY_public_key(
               key.get(), reinterpret_cast<uint8_t*>(raw_public_key.data()),
               &key_len, raw_public_key.size()),
           1);
  raw_public_key.resize(key_len);

  std::string raw_private_key(EVP_HPKE_MAX_PRIVATE_KEY_LENGTH, '\0');
  CHECK_EQ(EVP_HPKE_KEY_private_key(
               key.get(), reinterpret_cast<uint8_t*>(raw_private_key.data()),
               &key_len, raw_private_key.size()),
           1);
  raw_private_key.resize(key_len);

  absl::StatusOr<std::string> public_cwt = OkpCwt{
      .public_key = OkpKey{
          .key_id = std::string(key_id),
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = std::move(raw_public_key),
      }}.Encode();
  FCP_CHECK_STATUS(public_cwt.status());

  absl::StatusOr<std::string> private_key =
      OkpKey{
          .key_id = std::string(key_id),
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpDecrypt},
          .curve = crypto_internal::kX25519,
          .d = std::move(raw_private_key),
      }
          .Encode();
  FCP_CHECK_STATUS(private_key.status());

  return {*public_cwt, *private_key};
}

}  // namespace fcp::confidential_compute

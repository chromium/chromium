// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/private_membership/src/internal/aes_ctr_256_with_fixed_iv.h"

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include <openssl/rand.h>
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

std::string GetRandomBytes(size_t length) {
  std::unique_ptr<uint8_t[]> buf(new uint8_t[length]);
  // BoringSSL documentation says that it always returns 1; while
  // OpenSSL documentation says that it returns 1 on success, 0 otherwise. We
  // use BoringSSL, so we don't check the return value.
  RAND_bytes(buf.get(), length);
  return std::string(reinterpret_cast<const char*>(buf.get()), length);
}

TEST(AesCtr256WithFixedIVTest, TestEncryptDecrypt) {
  std::string key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  ASSERT_OK_AND_ASSIGN(auto cipher, AesCtr256WithFixedIV::Create(key));
  for (int i = 0; i < 256; ++i) {
    std::string message(GetRandomBytes(i));
    ASSERT_OK_AND_ASSIGN(auto ciphertext, cipher->Encrypt(message));
    EXPECT_EQ(ciphertext.size(), i);
    ASSERT_OK_AND_ASSIGN(auto plaintext, cipher->Decrypt(ciphertext));
    EXPECT_EQ(plaintext, message);
  }
}

TEST(AesCtr256WithFixedIVTest, TestEncryptDifferentFromMessage) {
  std::string key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  ASSERT_OK_AND_ASSIGN(auto cipher, AesCtr256WithFixedIV::Create(key));
  // Check for non-empty messages.
  for (int i = 1; i < 256; ++i) {
    std::string message(GetRandomBytes(i));
    ASSERT_OK_AND_ASSIGN(auto ciphertext, cipher->Encrypt(message));
    EXPECT_NE(message, ciphertext);
  }
}

TEST(AesCtr256WithFixedIVTest, InvalidKeySize) {
  std::string short_key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize - 1));

  EXPECT_THAT(AesCtr256WithFixedIV::Create(short_key),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Key size is invalid")));

  std::string long_key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize + 1));

  EXPECT_THAT(AesCtr256WithFixedIV::Create(long_key),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Key size is invalid")));
}

TEST(AesCtr256WithFixedIVTest, TestMultipleEncryptionsSameKeyAndMessage) {
  std::string key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  ASSERT_OK_AND_ASSIGN(auto cipher, AesCtr256WithFixedIV::Create(key));
  for (int i = 0; i < 256; ++i) {
    std::string message(GetRandomBytes(i));
    ASSERT_OK_AND_ASSIGN(auto ciphertext1, cipher->Encrypt(message));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, cipher->Encrypt(message));
    EXPECT_EQ(ciphertext1, ciphertext2);
  }
}

TEST(AesCtr256WithFixedIVTest, TestMultipleEncryptionsDifferentKey) {
  std::string key1(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  std::string key2(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  ASSERT_OK_AND_ASSIGN(auto cipher1, AesCtr256WithFixedIV::Create(key1));
  ASSERT_OK_AND_ASSIGN(auto cipher2, AesCtr256WithFixedIV::Create(key2));
  // Check non-empty messages.
  for (int i = 1; i < 256; ++i) {
    std::string message(GetRandomBytes(i));
    ASSERT_OK_AND_ASSIGN(auto ciphertext1, cipher1->Encrypt(message));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, cipher2->Encrypt(message));
    EXPECT_NE(ciphertext1, ciphertext2);
  }
}

TEST(AesCtr256WithFixedIVTest, TestMultipleEncryptionsDifferentMessages) {
  std::string key(GetRandomBytes(AesCtr256WithFixedIV::kKeySize));
  ASSERT_OK_AND_ASSIGN(auto cipher, AesCtr256WithFixedIV::Create(key));
  // Check non-empty messages.
  for (int i = 1; i < 256; ++i) {
    std::string message1(GetRandomBytes(i));
    std::string message2(GetRandomBytes(i));
    while (message2 == message1) {  // Ensure the messages are different.
      message2 = GetRandomBytes(i);
    }
    ASSERT_OK_AND_ASSIGN(auto ciphertext1, cipher->Encrypt(message1));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, cipher->Encrypt(message2));
    EXPECT_NE(ciphertext1, ciphertext2);
  }
}

}  // namespace
}  // namespace private_membership

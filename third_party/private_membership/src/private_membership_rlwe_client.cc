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

#include "third_party/private_membership/src/private_membership_rlwe_client.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"
#include "third_party/private_membership/src/internal/crypto_utils.h"
#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/internal/constants.h"
#include "third_party/private_membership/src/internal/encrypted_bucket_id.h"
#include "third_party/private_membership/src/internal/hashed_bucket_id.h"
#include "third_party/private_membership/src/internal/rlwe_id_utils.h"
#include "third_party/private_membership/src/internal/rlwe_params.h"
#include "third_party/private_membership/src/internal/utils.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "third_party/shell-encryption/src/polynomial.h"
#include "third_party/shell-encryption/src/status_macros.h"
#include "third_party/shell-encryption/src/symmetric_encryption_with_prng.h"
#include "third_party/shell-encryption/src/transcription.h"

namespace private_membership {
namespace rlwe {

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
PrivateMembershipRlweClient::Create(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<RlwePlaintextId>& plaintext_ids) {
  return CreateInternal(use_case, plaintext_ids, std::optional<std::string>(),
                        internal::PrngSeedGenerator::Create(),
                        internal::SecurePrngGenerator::Create());
}

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
PrivateMembershipRlweClient::CreateForTesting(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<RlwePlaintextId>& plaintext_ids,
    absl::string_view ec_cipher_key, absl::string_view seed) {
  RLWE_ASSIGN_OR_RETURN(auto prng_seed_generator,
                        internal::PrngSeedGenerator::CreateDeterministic(seed));
  return CreateInternal(
      use_case, plaintext_ids, std::optional<std::string>(ec_cipher_key),
      std::move(prng_seed_generator), internal::SecurePrngGenerator::Create());
}

::rlwe::StatusOr<std::unique_ptr<PrivateMembershipRlweClient>>
PrivateMembershipRlweClient::CreateInternal(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<RlwePlaintextId>& plaintext_ids,
    std::optional<std::string> ec_cipher_key,
    std::unique_ptr<internal::PrngSeedGenerator> prng_seed_generator,
    std::unique_ptr<internal::PrngGenerator> prng_generator) {
  if (use_case == private_membership::rlwe::RLWE_USE_CASE_UNDEFINED) {
    return absl::InvalidArgumentError("Use case must be defined.");
  }
  if (plaintext_ids.empty()) {
    return absl::InvalidArgumentError("Plaintext ids must not be empty.");
  }

  // Remove duplicate IDs.
  absl::flat_hash_set<std::string> hashed_rlwe_plaintext_ids;
  std::vector<RlwePlaintextId> unique_plaintext_ids;
  for (int i = 0; i < plaintext_ids.size(); ++i) {
    std::string hash = HashRlwePlaintextId(plaintext_ids[i]);
    if (!hashed_rlwe_plaintext_ids.contains(hash)) {
      unique_plaintext_ids.push_back(plaintext_ids[i]);
    }
    hashed_rlwe_plaintext_ids.insert(hash);
  }

  // Create the cipher with new key or from existing key depending on whether
  // the key was provided.
  auto ec_cipher =
      ec_cipher_key.has_value()
          ? ::private_join_and_compute::ECCommutativeCipher::CreateFromKey(
                kCurveId, ec_cipher_key.value(),
                ::private_join_and_compute::ECCommutativeCipher::HashType::SHA256)
          : ::private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
                kCurveId, ::private_join_and_compute::ECCommutativeCipher::HashType::SHA256);
  if (!ec_cipher.ok()) {
    return ec_cipher.status();
  }

  return absl::WrapUnique<PrivateMembershipRlweClient>(
      new PrivateMembershipRlweClient(
          use_case, unique_plaintext_ids, std::move(ec_cipher).value(),
          std::move(prng_seed_generator), std::move(prng_generator)));
}

PrivateMembershipRlweClient::PrivateMembershipRlweClient(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<RlwePlaintextId>& plaintext_ids,
    std::unique_ptr<::private_join_and_compute::ECCommutativeCipher> ec_cipher,
    std::unique_ptr<internal::PrngSeedGenerator> prng_seed_generator,
    std::unique_ptr<internal::PrngGenerator> prng_generator)
    : use_case_(use_case),
      plaintext_ids_(plaintext_ids),
      ec_cipher_(std::move(ec_cipher)),
      prng_seed_generator_(std::move(prng_seed_generator)),
      prng_generator_(std::move(prng_generator)) {}

::rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
PrivateMembershipRlweClient::CreateOprfRequest() {
  private_membership::rlwe::PrivateMembershipRlweOprfRequest request;
  request.set_use_case(use_case_);
  // Encrypt the plaintext ids with the client generated key.
  for (const auto& plaintext_id : plaintext_ids_) {
    std::string whole_id = HashRlwePlaintextId(plaintext_id);
    auto client_encrypted_id = ec_cipher_->Encrypt(whole_id);
    if (!client_encrypted_id.ok()) {
      return client_encrypted_id.status();
    }
    *request.add_encrypted_ids() = client_encrypted_id.value();

    // Populate the map of client encrypted id to plaintext id.
    client_encrypted_id_to_plaintext_id_[client_encrypted_id.value()] =
        plaintext_id;
  }
  return request;
}

absl::Status PrivateMembershipRlweClient::ValidateOprfResponse(
    const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
        oprf_response) const {
  // Check for valid bucket ID lengths.
  int encrypted_bucket_id_length =
      oprf_response.encrypted_buckets_parameters().encrypted_bucket_id_length();
  if (encrypted_bucket_id_length < 0 ||
      encrypted_bucket_id_length > kMaxEncryptedBucketIdLength) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Encrypted bucket ID length must be non-negative and at most ",
        kMaxEncryptedBucketIdLength, "."));
  }

  // Check number of responses.
  if (oprf_response.doubly_encrypted_ids_size() <
      client_encrypted_id_to_plaintext_id_.size()) {
    return absl::InvalidArgumentError(
        "OPRF response missing a response to a requested ID.");
  } else if (oprf_response.doubly_encrypted_ids_size() >
             client_encrypted_id_to_plaintext_id_.size()) {
    return absl::InvalidArgumentError(
        "OPRF response contains too many responses.");
  }
  return absl::OkStatus();
}

::rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
PrivateMembershipRlweClient::CreateQueryRequest(
    const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
        oprf_response) {
  auto validation_result = ValidateOprfResponse(oprf_response);
  if (!validation_result.ok()) {
    return validation_result;
  }

  // Initialize PIR client.
  int encrypted_bucket_id_length =
      oprf_response.encrypted_buckets_parameters().encrypted_bucket_id_length();
  int encrypted_buckets_count = 1 << encrypted_bucket_id_length;
  RLWE_ASSIGN_OR_RETURN(
      pir_client_, internal::PirClient::Create(
                       oprf_response.rlwe_parameters(), encrypted_buckets_count,
                       prng_seed_generator_.get(), prng_generator_.get()));

  private_membership::rlwe::PrivateMembershipRlweQueryRequest request;
  request.set_use_case(use_case_);
  request.set_key_version(oprf_response.key_version());

  // Keep track of seen plaintext IDs to check for duplicates.
  absl::flat_hash_set<std::string> seen_encrypted_ids;

  for (const auto& doubly_encrypted_id : oprf_response.doubly_encrypted_ids()) {
    private_membership::rlwe::PrivateMembershipRlweQuery single_query;
    single_query.set_queried_encrypted_id(
        doubly_encrypted_id.queried_encrypted_id());
    const std::string& encrypted_id =
        doubly_encrypted_id.queried_encrypted_id();

    // Check validity of returned queried ID.
    if (!client_encrypted_id_to_plaintext_id_.contains(encrypted_id)) {
      return absl::InvalidArgumentError(
          "OPRF response contains a response to an erroneous encrypted ID.");
    }

    // Already processed a response for this encrypted ID. Ignore this one.
    if (seen_encrypted_ids.contains(encrypted_id)) {
      return absl::InvalidArgumentError(
          "OPRF response contains duplicate responses for the same ID.");
    }
    seen_encrypted_ids.insert(encrypted_id);

    // Compute the hashed bucket id if the hashed bucket parameter is set in
    // the response.
    if (oprf_response.hashed_buckets_parameters().hashed_bucket_id_length() >
        0) {
      const RlwePlaintextId& plaintext_id =
          client_encrypted_id_to_plaintext_id_[encrypted_id];
      RLWE_ASSIGN_OR_RETURN(
          HashedBucketId hashed_bucket_id,
          HashedBucketId::Create(plaintext_id,
                                 oprf_response.hashed_buckets_parameters(),
                                 &context_));
      *single_query.mutable_hashed_bucket_id() = hashed_bucket_id.ToApiProto();
    }

    // Decrypt doubly encrypted id to retrieve id encrypted only by the server
    // key.
    auto server_encrypted_id =
        ec_cipher_->Decrypt(doubly_encrypted_id.doubly_encrypted_id());
    if (!server_encrypted_id.ok()) {
      return server_encrypted_id.status();
    }

    // Truncate the hash of the server encrypted id by the first
    // encrypted_bucket_id_length bits to compute the encrypted bucket id.
    RLWE_ASSIGN_OR_RETURN(
        EncryptedBucketId encrypted_bucket_id_obj,
        EncryptedBucketId::Create(server_encrypted_id.value(),
                                  oprf_response.encrypted_buckets_parameters(),
                                  &context_));
    RLWE_ASSIGN_OR_RETURN(int encrypted_bucket_id,
                          encrypted_bucket_id_obj.ToUint32());

    // Create query request.
    RLWE_ASSIGN_OR_RETURN(*single_query.mutable_pir_request(),
                          pir_client_->CreateRequest(encrypted_bucket_id));

    client_encrypted_id_to_server_encrypted_id_[encrypted_id] =
        std::move(server_encrypted_id).value();

    *request.add_queries() = single_query;
  }

  hashed_bucket_params_ = oprf_response.hashed_buckets_parameters();
  encrypted_bucket_params_ = oprf_response.encrypted_buckets_parameters();
  return request;
}

absl::Status PrivateMembershipRlweClient::ValidateQueryResponse(
    const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
        query_response) const {
  // Check response length for missing responses.
  if (query_response.pir_responses_size() <
      client_encrypted_id_to_plaintext_id_.size()) {
    return absl::InvalidArgumentError(
        "Query response missing a response to a requested ID.");
  } else if (query_response.pir_responses_size() >
             client_encrypted_id_to_plaintext_id_.size()) {
    return absl::InvalidArgumentError(
        "Query response contains too many responses.");
  }
  return absl::OkStatus();
}

::rlwe::StatusOr<RlweMembershipResponses>
PrivateMembershipRlweClient::ProcessQueryResponse(
    const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
        query_response) {
  auto validation_result = ValidateQueryResponse(query_response);
  if (!validation_result.ok()) {
    return validation_result;
  }

  // Keep track of seen encrypted IDs to avoid duplicates.
  absl::flat_hash_set<std::string> seen_encrypted_ids;

  RlweMembershipResponses membership_responses;
  for (const auto& pir_response : query_response.pir_responses()) {
    const std::string& encrypted_id = pir_response.queried_encrypted_id();
    if (!client_encrypted_id_to_plaintext_id_.contains(encrypted_id) ||
        !client_encrypted_id_to_server_encrypted_id_.contains(encrypted_id)) {
      return absl::InvalidArgumentError(
          "Query response contains a response to an erroneous encrypted ID.");
    }

    // Already processed this encrypted ID. Ignore this one.
    if (seen_encrypted_ids.contains(encrypted_id)) {
      return absl::InvalidArgumentError(
          "Query response contains duplicate responses for the same ID.");
    }
    seen_encrypted_ids.insert(encrypted_id);

    RLWE_ASSIGN_OR_RETURN(
        std::vector<uint8_t> serialized_encrypted_bucket_byte,
        pir_client_->ProcessResponse(pir_response.pir_response()));

    std::string serialized_encrypted_bucket;
    if (!serialized_encrypted_bucket_byte.empty()) {
      RLWE_ASSIGN_OR_RETURN(serialized_encrypted_bucket,
                            private_membership::Unpad(std::string(
                                serialized_encrypted_bucket_byte.begin(),
                                serialized_encrypted_bucket_byte.end())));
    }

    private_membership::rlwe::EncryptedBucket encrypted_bucket;
    if (!serialized_encrypted_bucket.empty() &&
        !encrypted_bucket.ParseFromString(serialized_encrypted_bucket)) {
      return absl::InternalError("Parsing serialized encrypted bucket failed.");
    }

    // Plaintext id associated with the client encrypted id.
    const RlwePlaintextId& plaintext_id =
        client_encrypted_id_to_plaintext_id_[encrypted_id];
    // Server key encrypted id associated with the client encrypted id.
    const std::string& server_encrypted_id =
        client_encrypted_id_to_server_encrypted_id_[encrypted_id];
    RLWE_ASSIGN_OR_RETURN(auto membership, CheckMembership(server_encrypted_id,
                                                           encrypted_bucket));
    auto* response = membership_responses.add_membership_responses();
    *response->mutable_plaintext_id() = plaintext_id;
    *response->mutable_membership_response() = membership;
  }

  return membership_responses;
}

::rlwe::StatusOr<private_membership::MembershipResponse>
PrivateMembershipRlweClient::CheckMembership(
    absl::string_view server_encrypted_id,
    const private_membership::rlwe::EncryptedBucket& encrypted_bucket) {
  private_membership::MembershipResponse membership_response;
  RLWE_ASSIGN_OR_RETURN(
      std::string to_match_hash,
      ComputeBucketStoredEncryptedId(server_encrypted_id,
                                     encrypted_bucket_params_, &context_));
  for (const auto& encrypted_id_value_pair :
       encrypted_bucket.encrypted_id_value_pairs()) {
    const auto& encrypted_id = encrypted_id_value_pair.encrypted_id();
    // Check encrypted_id is a prefix of to_match_hash. If it is, then the id
    // is a member.
    if (std::equal(encrypted_id.begin(), encrypted_id.end(),
                   to_match_hash.begin())) {
      membership_response.set_is_member(true);
      if (!encrypted_id_value_pair.encrypted_value().empty()) {
        RLWE_ASSIGN_OR_RETURN(
            std::string decrypted_value,
            private_membership::DecryptValue(
                server_encrypted_id, encrypted_id_value_pair.encrypted_value(),
                &context_));
        membership_response.set_value(decrypted_value);
      }
      break;
    }
  }
  return membership_response;
}

namespace internal {

std::unique_ptr<PrngSeedGenerator> PrngSeedGenerator::Create() {
  return absl::WrapUnique<PrngSeedGenerator>(new PrngSeedGenerator());
}

::rlwe::StatusOr<std::unique_ptr<PrngSeedGenerator>>
PrngSeedGenerator::CreateDeterministic(absl::string_view seed) {
  RLWE_ASSIGN_OR_RETURN(auto prng_seed_generator,
                        SingleThreadPrng::Create(seed));
  return absl::WrapUnique<PrngSeedGenerator>(
      new PrngSeedGenerator(std::move(prng_seed_generator)));
}

::rlwe::StatusOr<std::string> PrngSeedGenerator::GeneratePrngSeed() const {
  if (deterministic_prng_seed_generator_.has_value()) {
    std::string res(SingleThreadPrng::SeedLength(), 0);
    for (int i = 0; i < res.length(); ++i) {
      RLWE_ASSIGN_OR_RETURN(
          res[i], deterministic_prng_seed_generator_.value()->Rand8());
    }
    return res;
  }
  return SingleThreadPrng::GenerateSeed();
}

PrngSeedGenerator::PrngSeedGenerator(
    std::unique_ptr<SingleThreadPrng> prng_seed_generator)
    : deterministic_prng_seed_generator_(
          std::optional<std::unique_ptr<SingleThreadPrng>>(
              std::move(prng_seed_generator))) {}

template <typename ModularInt>
::rlwe::StatusOr<std::unique_ptr<PirClientImpl<ModularInt>>>
PirClientImpl<ModularInt>::Create(const RlweParameters& rlwe_params,
                                  int total_entry_count,
                                  const PrngSeedGenerator* prng_seed_generator,
                                  const PrngGenerator* prng_generator) {
  if (rlwe_params.log_degree() < 0 ||
      rlwe_params.log_degree() > kMaxLogDegree) {
    return absl::InvalidArgumentError(
        "Degree must be positive and at most 2^20.");
  }
  int levels_of_recursion = rlwe_params.levels_of_recursion();
  if (levels_of_recursion <= 0 || levels_of_recursion > kMaxLevelsOfRecursion) {
    return absl::InvalidArgumentError(
        absl::StrCat("Levels of recursion, ", levels_of_recursion,
                     ", must be positive and at most ", kMaxLevelsOfRecursion));
  }
  // Create parameters.
  std::vector<std::unique_ptr<const typename ModularInt::Params>>
      modulus_params;
  modulus_params.reserve(rlwe_params.modulus_size());
  std::vector<std::unique_ptr<const ::rlwe::NttParameters<ModularInt>>>
      ntt_params;
  ntt_params.reserve(rlwe_params.modulus_size());
  std::vector<std::unique_ptr<const ::rlwe::ErrorParams<ModularInt>>>
      error_params;
  error_params.reserve(rlwe_params.modulus_size());
  for (int i = 0; i < rlwe_params.modulus_size(); ++i) {
    RLWE_ASSIGN_OR_RETURN(
        auto temp_modulus_params,
        CreateModulusParams<ModularInt>(rlwe_params.modulus(i)));
    modulus_params.push_back(std::move(temp_modulus_params));
    RLWE_ASSIGN_OR_RETURN(
        auto temp_ntt_params,
        CreateNttParams<ModularInt>(rlwe_params, modulus_params[i].get()));
    ntt_params.push_back(std::move(temp_ntt_params));
    RLWE_ASSIGN_OR_RETURN(
        auto temp_error_params,
        CreateErrorParams<ModularInt>(rlwe_params, modulus_params[i].get(),
                                      ntt_params[i].get()));
    error_params.push_back(std::move(temp_error_params));
  }

  RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                        prng_seed_generator->GeneratePrngSeed());
  RLWE_ASSIGN_OR_RETURN(auto prng, prng_generator->CreatePrng(prng_seed));
  RLWE_ASSIGN_OR_RETURN(
      auto key,
      ::rlwe::SymmetricRlweKey<ModularInt>::Sample(
          rlwe_params.log_degree(), rlwe_params.variance(), rlwe_params.log_t(),
          modulus_params[0].get(), ntt_params[0].get(), prng.get()));

  return absl::WrapUnique<>(new PirClientImpl(
      rlwe_params, std::move(modulus_params), std::move(ntt_params),
      std::move(error_params), key, total_entry_count, prng_seed_generator,
      prng_generator));
}

template <typename ModularInt>
PirClientImpl<ModularInt>::PirClientImpl(
    const RlweParameters& rlwe_params,
    std::vector<std::unique_ptr<const typename ModularInt::Params>>
        modulus_params,
    std::vector<std::unique_ptr<const ::rlwe::NttParameters<ModularInt>>>
        ntt_params,
    std::vector<std::unique_ptr<const ::rlwe::ErrorParams<ModularInt>>>
        error_params,
    const ::rlwe::SymmetricRlweKey<ModularInt>& key, int total_entry_count,
    const PrngSeedGenerator* prng_seed_generator,
    const PrngGenerator* prng_generator)
    : rlwe_params_(rlwe_params),
      modulus_params_(std::move(modulus_params)),
      ntt_params_(std::move(ntt_params)),
      error_params_(std::move(error_params)),
      key_(key),
      total_entry_count_(total_entry_count),
      prng_seed_generator_(prng_seed_generator),
      prng_generator_(prng_generator) {}

template <typename ModularInt>
::rlwe::StatusOr<PirRequest> PirClientImpl<ModularInt>::CreateRequest(
    int index) {
  if (index < 0 || index >= total_entry_count_) {
    return absl::InvalidArgumentError("Index out of bounds.");
  }

  PirRequest req;

  // The number of virtual entries per level of recursion = the
  // (levels_of_recursion)th root of the number of items in the database.
  double exact_entries_per_level =
      pow(total_entry_count_, 1.0 / rlwe_params_.levels_of_recursion());
  // Round this number up to the nearest whole integer.
  int branching_factor = static_cast<int>(ceil(exact_entries_per_level));

  // Create the ciphertexts for each level of recursion. This two-dimensional
  // table is flattened when it is put into the proto.

  // Determine the number of actual database items stored in each virtual
  // database block at this level. This is the number of items remaining
  // divided by the branching factor, rounded up.
  int items_in_block =
      (total_entry_count_ + branching_factor - 1) / branching_factor;

  // The index of the item we want to request at the current level of recursion.
  int index_remaining = index;

  // Create useful zero polynomial.
  std::vector<ModularInt> zeroes(
      1 << rlwe_params_.log_degree(),
      ModularInt::ImportZero(modulus_params_[0].get()));
  ::rlwe::Polynomial<ModularInt> zero_poly =
      ::rlwe::Polynomial<ModularInt>(zeroes);

  // Create useful indicator polynomial.
  std::vector<ModularInt> indicator(zeroes);
  indicator[0] = ModularInt::ImportOne(modulus_params_[0].get());
  const ::rlwe::Polynomial<ModularInt> indicator_poly =
      ::rlwe::Polynomial<ModularInt>::ConvertToNtt(indicator, *(ntt_params_[0]),
                                                   modulus_params_[0].get());

  // Fill plaintext indicator vector with only zeroes at first.
  if (branching_factor * rlwe_params_.levels_of_recursion() >
      kMaxRequestEntries) {
    return absl::InvalidArgumentError(
        absl::StrCat("Number of request entries exceeds ", kMaxRequestEntries));
  }
  std::vector<::rlwe::Polynomial<ModularInt>> plaintexts(
      branching_factor * rlwe_params_.levels_of_recursion(), zero_poly);

  // Fill appropriate indicator for each level of recursion.
  for (int level = 0; level < rlwe_params_.levels_of_recursion(); ++level) {
    // Determine which block contains the item we wish to request.
    int index_at_level = index_remaining / items_in_block;
    int index_in_plaintext = (level * branching_factor) + index_at_level;
    plaintexts[index_in_plaintext] = indicator_poly;

    // Determine the index of the desired item within that block. This is
    // the index within the items that remain after this level of recursion.
    index_remaining = index_remaining % items_in_block;

    // Update the block size for the next level of recursion.
    items_in_block = (items_in_block + branching_factor - 1) / branching_factor;
  }

  RLWE_ASSIGN_OR_RETURN(auto prng_seed,
                        prng_seed_generator_->GeneratePrngSeed());
  req.set_prng_seed(prng_seed);
  RLWE_ASSIGN_OR_RETURN(auto prng, prng_generator_->CreatePrng(prng_seed));
  RLWE_ASSIGN_OR_RETURN(std::string prng_encryption_seed,
                        prng_seed_generator_->GeneratePrngSeed());
  RLWE_ASSIGN_OR_RETURN(auto prng_encryption,
                        prng_generator_->CreatePrng(prng_encryption_seed));
  RLWE_ASSIGN_OR_RETURN(std::vector<::rlwe::Polynomial<ModularInt>> ciphertexts,
                        ::rlwe::EncryptWithPrng(key_, plaintexts, prng.get(),
                                                prng_encryption.get()));
  for (int i = 0; i < ciphertexts.size(); ++i) {
    RLWE_ASSIGN_OR_RETURN(*req.add_request(),
                          ciphertexts[i].Serialize(modulus_params_[0].get()));
  }

  return req;
}

template <typename ModularInt>
::rlwe::StatusOr<std::vector<uint8_t>>
PirClientImpl<ModularInt>::ProcessResponse(const PirResponse& response) {
  if (response.plaintext_entry_size() < 0 ||
      response.plaintext_entry_size() > kMaxPlaintextEntrySize) {
    return absl::InvalidArgumentError(
        "Invalid plaintext entry size that must be at most 10 MB in length.");
  }
  std::vector<uint8_t> raw_bytes;
  for (int i = 0; i < response.response_size(); i++) {
    const typename ModularInt::Params* decrypt_modulus_params;
    const ::rlwe::NttParameters<ModularInt>* decrypt_ntt_params;
    const ::rlwe::ErrorParams<ModularInt>* decrypt_error_params;
    ::rlwe::SymmetricRlweKey<ModularInt> decrypt_key = key_;
    if (modulus_params_.size() == 2) {
      decrypt_modulus_params = modulus_params_[1].get();
      decrypt_ntt_params = ntt_params_[1].get();
      decrypt_error_params = error_params_[1].get();
      RLWE_ASSIGN_OR_RETURN(
          decrypt_key,
          key_.SwitchModulus(decrypt_modulus_params, decrypt_ntt_params));
    } else if (modulus_params_.size() == 1) {
      decrypt_modulus_params = modulus_params_[0].get();
      decrypt_ntt_params = ntt_params_[0].get();
      decrypt_error_params = error_params_[0].get();
    } else {
      return absl::InternalError("More than two moduli.");
    }
    RLWE_ASSIGN_OR_RETURN(
        auto ciphertext,
        ::rlwe::SymmetricRlweCiphertext<ModularInt>::Deserialize(
            response.response(i), decrypt_modulus_params,
            decrypt_error_params));
    RLWE_ASSIGN_OR_RETURN(std::vector<typename ModularInt::Int> plaintext,
                          ::rlwe::Decrypt(decrypt_key, ciphertext));
    RLWE_ASSIGN_OR_RETURN(
        std::vector<uint8_t> column,
        (::rlwe::TranscribeBits<typename ModularInt::Int, uint8_t>(
            plaintext, key_.Len() * key_.BitsPerCoeff(), key_.BitsPerCoeff(),
            8)));

    raw_bytes.insert(raw_bytes.end(), std::make_move_iterator(column.begin()),
                     std::make_move_iterator(column.end()));
  }
  raw_bytes.resize(response.plaintext_entry_size());
  return raw_bytes;
}

::rlwe::StatusOr<std::unique_ptr<internal::PirClient>>
internal::PirClient::Create(const RlweParameters& rlwe_params,
                            int total_entry_count,
                            const PrngSeedGenerator* prng_seed_generator,
                            const PrngGenerator* prng_generator) {
  if (rlwe_params.modulus_size() <= 0) {
    return absl::InvalidArgumentError("Must provide at least one modulus.");
  }
  if (rlwe_params.modulus(0).hi() > 0 ||
      (rlwe_params.modulus(0).lo() >> 62) > 0) {
    RLWE_ASSIGN_OR_RETURN(
        auto client, PirClientImpl<ModularInt128>::Create(
                         rlwe_params, total_entry_count, prng_seed_generator,
                         prng_generator));
    return std::unique_ptr<internal::PirClient>(std::move(client));
  } else {
    RLWE_ASSIGN_OR_RETURN(
        auto client, PirClientImpl<ModularInt64>::Create(
                         rlwe_params, total_entry_count, prng_seed_generator,
                         prng_generator));
    return std::unique_ptr<internal::PirClient>(std::move(client));
  }
}

template class PirClientImpl<ModularInt64>;
template class PirClientImpl<ModularInt128>;

}  // namespace internal

}  // namespace rlwe
}  // namespace private_membership

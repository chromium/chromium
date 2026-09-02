/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/metadata_obu.h"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::StatusOr<uint64_t> InferItuT35PayloadSize(
    const int64_t payload_size, const uint8_t metadata_type_size,
    bool has_country_code_extension_byte) {
  // metadataTypeSize, 1 byte for the country code, and 1 byte for
  // the country code extension byte if present.
  const int64_t overhead =
      metadata_type_size + (has_country_code_extension_byte ? 2 : 1);
  if (payload_size < overhead) {
    return absl::InvalidArgumentError(
        absl::StrCat("ITU-T T.35 payload_size= ", payload_size,
                     " is too small for the required overhead= ", overhead));
  }
  return static_cast<uint64_t>(payload_size - overhead);
}

absl::Status ReadAndValidateMetadataITUTT35(
    const int64_t payload_size, uint8_t metadata_type_size, ReadBitBuffer& rb,
    MetadataITUTT35& metadata_itu_t_t35) {
  RETURN_IF_NOT_OK(
      rb.ReadUnsignedLiteral(8, metadata_itu_t_t35.itu_t_t35_country_code));
  if (metadata_itu_t_t35.itu_t_t35_country_code == 0xFF) {
    uint8_t itu_t_t35_country_code_extension_byte;
    RETURN_IF_NOT_OK(
        rb.ReadUnsignedLiteral(8, itu_t_t35_country_code_extension_byte));
    metadata_itu_t_t35.itu_t_t35_country_code_extension_byte =
        itu_t_t35_country_code_extension_byte;
  }
  auto inferred_size =
      InferItuT35PayloadSize(payload_size, metadata_type_size,
                             metadata_itu_t_t35.itu_t_t35_country_code == 0xFF);
  if (!inferred_size.ok()) {
    return inferred_size.status();
  }
  std::vector<uint8_t> temp_itu_t_t35_payload_bytes;
  temp_itu_t_t35_payload_bytes.resize(*inferred_size);
  RETURN_IF_NOT_OK(
      rb.ReadUint8Span(absl::MakeSpan(temp_itu_t_t35_payload_bytes)));
  metadata_itu_t_t35.itu_t_t35_payload_bytes =
      std::move(temp_itu_t_t35_payload_bytes);
  return absl::OkStatus();
}

absl::Status ReadAndValidateMetadataIamfTags(
    ReadBitBuffer& rb, MetadataIamfTags& metadata_iamf_tags) {
  uint8_t num_tags;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, num_tags));
  for (int i = 0; i < num_tags; ++i) {
    MetadataIamfTags::Tag tag;
    RETURN_IF_NOT_OK(rb.ReadString(tag.tag_name));
    RETURN_IF_NOT_OK(rb.ReadString(tag.tag_value));
    metadata_iamf_tags.tags.push_back(std::move(tag));
  }
  return absl::OkStatus();
}

absl::Status WriteMetadataITUTT35(const MetadataITUTT35& metadata_itu_t_t35,
                                  WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(metadata_itu_t_t35.itu_t_t35_country_code, 8));
  if (metadata_itu_t_t35.itu_t_t35_country_code == 0xFF) {
    if (!metadata_itu_t_t35.itu_t_t35_country_code_extension_byte.has_value()) {
      return absl::InvalidArgumentError(
          "ITU-T T35 country code is 0xFF but country code extension byte is "
          "not present.");
    }
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
        metadata_itu_t_t35.itu_t_t35_country_code_extension_byte.value(), 8));
  }
  RETURN_IF_NOT_OK(
      wb.WriteUint8Span(metadata_itu_t_t35.itu_t_t35_payload_bytes));
  return absl::OkStatus();
}

absl::Status WriteMetadataIamfTags(const MetadataIamfTags& metadata_iamf_tags,
                                   WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(metadata_iamf_tags.tags.size(), 8));
  for (const auto& tag : metadata_iamf_tags.tags) {
    RETURN_IF_NOT_OK(wb.WriteString(tag.tag_name));
    RETURN_IF_NOT_OK(wb.WriteString(tag.tag_value));
  }
  return absl::OkStatus();
}

}  // namespace

MetadataObu MetadataObu::Create(const ObuHeader& header,
                                MetadataVariant metadata_variant) {
  MetadataType metadata_type = std::visit(
      absl::Overload{
          [](const MetadataITUTT35& arg) { return kMetadataTypeITUT_T35; },
          [](const MetadataIamfTags& arg) { return kMetadataTypeIamfTags; }},
      metadata_variant);
  MetadataObu metadata_obu(header);
  metadata_obu.metadata_type_ = metadata_type;
  metadata_obu.metadata_variant_ = std::move(metadata_variant);
  return metadata_obu;
}

absl::StatusOr<MetadataObu> MetadataObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  MetadataObu metadata_obu(header);
  RETURN_IF_NOT_OK(metadata_obu.ReadAndValidatePayload(payload_size, rb));
  return metadata_obu;
}

void MetadataObu::PrintObu() const {}

absl::Status MetadataObu::ValidateAndWritePayload(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(metadata_type_));
  if (metadata_type_ == kMetadataTypeITUT_T35) {
    RETURN_IF_NOT_OK(
        WriteMetadataITUTT35(std::get<MetadataITUTT35>(metadata_variant_), wb));
  } else if (metadata_type_ == kMetadataTypeIamfTags) {
    RETURN_IF_NOT_OK(WriteMetadataIamfTags(
        std::get<MetadataIamfTags>(metadata_variant_), wb));
  }
  return absl::OkStatus();
}

absl::Status MetadataObu::ReadAndValidatePayloadDerived(int64_t payload_size,
                                                        ReadBitBuffer& rb) {
  DecodedUleb128 metadata_type;
  int8_t metadata_type_size;
  RETURN_IF_NOT_OK(rb.ReadULeb128(metadata_type, metadata_type_size));
  metadata_type_ = static_cast<MetadataType>(metadata_type);
  if (metadata_type_ == kMetadataTypeITUT_T35) {
    if (payload_size < metadata_type_size) {
      return absl::InvalidArgumentError(
          "ITUT-T35 metadata must have payload greater than metadata type "
          "size.");
    }
    MetadataITUTT35 metadata_itu_t_t35;
    RETURN_IF_NOT_OK(ReadAndValidateMetadataITUTT35(
        payload_size, metadata_type_size, rb, metadata_itu_t_t35));
    metadata_variant_ = std::move(metadata_itu_t_t35);
  } else if (metadata_type_ == kMetadataTypeIamfTags) {
    MetadataIamfTags metadata_iamf_tags;
    RETURN_IF_NOT_OK(ReadAndValidateMetadataIamfTags(rb, metadata_iamf_tags));
    metadata_variant_ = std::move(metadata_iamf_tags);
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools

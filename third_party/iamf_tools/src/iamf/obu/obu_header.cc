/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/obu_header.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

bool IsAudioFrame(ObuType type) {
  return (kObuIaAudioFrameId0 <= type && type <= kObuIaAudioFrameId17) ||
         type == kObuIaAudioFrame;
}

// Returns `true` if this `ObuType` is allowed to have the `obu_redundant_copy`
// flag set. `false` otherwise.
bool IsRedundantCopyAllowed(ObuType type) {
  if (IsAudioFrame(type) || type == kObuIaTemporalDelimiter ||
      type == kObuIaParameterBlock) {
    return false;
  }
  return true;
}

bool IsTypeSpecificFlagAllowed(ObuType type) {
  return IsAudioFrame(type) || type == kObuIaTemporalDelimiter ||
         type == kObuIaMixPresentation;
}

// Validates the OBU and returns an error if anything is non-comforming.
// Requires that all fields including `obu_size_` are initialized.
absl::Status Validate(const ObuHeader& header) {
  // Validate IAMF imposed requirements.
  if (header.obu_redundant_copy && !IsRedundantCopyAllowed(header.obu_type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The redundant copy flag is not allowed to be set for obu_type= ",
        header.obu_type));
  }

  if (header.type_specific_flag &&
      !IsTypeSpecificFlagAllowed(header.obu_type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The type-specific flag is reserved for obu_type= ", header.obu_type,
        " and SHALL be set to 0"));
  }

  return absl::OkStatus();
}

absl::Status WriteFieldsAfterObuSize(const ObuHeader& header,
                                     WriteBitBuffer& wb) {
  // These fields are conditionally in the OBU.
  if (header.GetObuTrimmingStatusFlag()) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(header.num_samples_to_trim_at_end));
    RETURN_IF_NOT_OK(wb.WriteUleb128(header.num_samples_to_trim_at_start));
  }

  // These fields are conditionally in the OBU.
  if (header.GetExtensionHeaderFlag()) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(header.GetExtensionHeaderSize()));
    RETURN_IF_NOT_OK(
        wb.WriteUint8Span(absl::MakeConstSpan(*header.extension_header_bytes)));
  }

  return absl::OkStatus();
}

// IAMF imposes two restrictions on the size of an entire OBU.
//   - IAMF v1.1.0 imposes a maximum size of an entire OBU must be 2 MB or less.
//   - IAMF v1.1.0 also imposes a maximum size of `obu_size` must be 2^21 - 4 or
//     less.
//
// The second restriction is equivalent when `obu_size` is written using the
// minimal number of bytes. It is less strict than the first restriction if
// `obu_size` is written using padded bytes. Therefore the second restriction is
// irrelevant.
absl::Status ValidateObuIsUnderTwoMegabytes(DecodedUleb128 obu_size,
                                            size_t size_of_obu_size) {
  ABSL_CHECK_LE(size_of_obu_size, static_cast<size_t>(kMaxLeb128Size));

  // Subtract out `obu_size` and all preceding data (one byte).
  const uint32_t max_obu_size =
      kEntireObuSizeMaxTwoMegabytes - 1 - size_of_obu_size;

  if (obu_size > max_obu_size) {
    return absl::InvalidArgumentError(
        absl::StrCat("obu_size= ", obu_size,
                     " results in an OBU greater than 2 MB in size."));
  }

  return absl::OkStatus();
}

absl::Status GetSizeOfEncodedLeb128(const LebGenerator& leb_generator,
                                    DecodedUleb128 leb128, size_t& size) {
  // Calculate how many bytes `obu_size` will take up based on the current leb
  // generator.
  WriteBitBuffer temp_wb_obu_size_only(8, leb_generator);
  RETURN_IF_NOT_OK(temp_wb_obu_size_only.WriteUleb128(leb128));
  size = temp_wb_obu_size_only.bit_buffer().size();
  return absl::OkStatus();
}

// Validates the header and initializes the output argument. On success
// `obu_size` is set to imply the associated payload has a size of
// `payload_serialized_size`.
absl::Status GetObuSizeAndValidate(const LebGenerator& leb_generator,
                                   const ObuHeader& header,
                                   int64_t payload_serialized_size,
                                   DecodedUleb128& obu_size) {
  // Validate to avoid issues with the `static_cast` below.
  if (0 > payload_serialized_size ||
      payload_serialized_size > std::numeric_limits<uint32_t>::max()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Payload size must fit into a `uint32_t`. payload_serialized_size= ",
        payload_serialized_size));
  }

  // Set `obu_size`. It depends on the size of all fields after `obu_size` and
  // `payload_serialized_size`.
  {
    WriteBitBuffer temp_wb_after_obu_size(64, leb_generator);
    RETURN_IF_NOT_OK(WriteFieldsAfterObuSize(header, temp_wb_after_obu_size));

    if (!temp_wb_after_obu_size.IsByteAligned() ||
        temp_wb_after_obu_size.bit_buffer().size() >
            std::numeric_limits<uint32_t>::max()) {
      return absl::UnknownError(absl::StrCat(
          "Result from `WriteFieldsAfterObuSize()` was not byte-aligned ",
          "or it did not fit into a `uint32_t`. `bit_offset` is ",
          temp_wb_after_obu_size.bit_offset()));
    }
    // Get the size of fields after `obu_size`.
    const uint32_t fields_after_obu_size =
        temp_wb_after_obu_size.bit_buffer().size();

    // `obu_size` represents the size of all fields after itself and
    // serialized_size. Sum them to get `obu_size`, while ensuring they fit into
    // a `uint32_t`.
    RETURN_IF_NOT_OK(AddUint32CheckOverflow(
        static_cast<uint32_t>(fields_after_obu_size),
        static_cast<uint32_t>(payload_serialized_size), obu_size));

    size_t size_of_obu_size;
    RETURN_IF_NOT_OK(
        GetSizeOfEncodedLeb128(leb_generator, obu_size, size_of_obu_size));

    RETURN_IF_NOT_OK(
        ValidateObuIsUnderTwoMegabytes(obu_size, size_of_obu_size));
  }

  // Validate the OBU.
  RETURN_IF_NOT_OK(Validate(header));

  return absl::OkStatus();
}

// Returns the size of the payload associated with the OBU, i.e. the number of
// bytes that contain payload data. See
// https://aomediacodec.github.io/iamf/#obu_size for more details.
int64_t GetObuPayloadSize(DecodedUleb128 obu_size,
                          int8_t num_samples_to_trim_at_end_size,
                          int8_t num_samples_to_trim_at_start_size,
                          int8_t extension_header_size_size,
                          DecodedUleb128 extension_header_bytes_size) {
  return static_cast<int64_t>(obu_size) -
         (static_cast<int64_t>(num_samples_to_trim_at_end_size) +
          static_cast<int64_t>(num_samples_to_trim_at_start_size) +
          static_cast<int64_t>(extension_header_size_size) +
          static_cast<int64_t>(extension_header_bytes_size));
}

absl::Status FillHeaderMetadata(ReadBitBuffer& rb,
                                HeaderMetadata& output_header_metadata) {
  uint64_t obu_type_uint64_t = 0;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, obu_type_uint64_t));
  output_header_metadata.obu_type = static_cast<ObuType>(obu_type_uint64_t);
  // We don't care about the next three bits.
  bool dummy_bool;
  RETURN_IF_NOT_OK(rb.ReadBoolean(dummy_bool));
  RETURN_IF_NOT_OK(rb.ReadBoolean(dummy_bool));
  RETURN_IF_NOT_OK(rb.ReadBoolean(dummy_bool));
  DecodedUleb128 obu_size;
  int8_t size_of_obu_size = 0;
  RETURN_IF_NOT_OK(rb.ReadULeb128(obu_size, size_of_obu_size));
  RETURN_IF_NOT_OK(ValidateObuIsUnderTwoMegabytes(obu_size, size_of_obu_size));
  // The extra byte is for the `obu_type` field + the three boolean fields.
  output_header_metadata.total_obu_size =
      static_cast<int64_t>(obu_size) + size_of_obu_size + 1;
  return absl::OkStatus();
}

}  // namespace

bool ObuHeader::IsTemporalUnitObuType(const ObuType obu_type) {
  static const absl::NoDestructor<absl::flat_hash_set<ObuType>>
      kTemporalUnitObuTypes(absl::flat_hash_set<ObuType>(
          {kObuIaAudioFrame,       kObuIaAudioFrameId0,
           kObuIaAudioFrameId1,    kObuIaAudioFrameId2,
           kObuIaAudioFrameId3,    kObuIaAudioFrameId4,
           kObuIaAudioFrameId5,    kObuIaAudioFrameId6,
           kObuIaAudioFrameId7,    kObuIaAudioFrameId8,
           kObuIaAudioFrameId9,    kObuIaAudioFrameId10,
           kObuIaAudioFrameId11,   kObuIaAudioFrameId12,
           kObuIaAudioFrameId13,   kObuIaAudioFrameId14,
           kObuIaAudioFrameId15,   kObuIaAudioFrameId16,
           kObuIaAudioFrameId17,   kObuIaParameterBlock,
           kObuIaTemporalDelimiter}));
  return kTemporalUnitObuTypes->contains(obu_type);
}

absl::StatusOr<HeaderMetadata> ObuHeader::PeekObuTypeAndTotalObuSize(
    ReadBitBuffer& rb) {
  const uint64_t position_before_header = rb.Tell();
  HeaderMetadata header_metadata;
  auto header_metadata_status = FillHeaderMetadata(rb, header_metadata);
  RETURN_IF_NOT_OK(rb.Seek(position_before_header));
  if (!header_metadata_status.ok()) {
    return header_metadata_status;
  }
  return header_metadata;
}

absl::Status ObuHeader::ValidateAndWrite(int64_t payload_serialized_size,
                                         WriteBitBuffer& wb) const {
  DecodedUleb128 obu_size;
  RETURN_IF_NOT_OK(GetObuSizeAndValidate(wb.leb_generator_, *this,
                                         payload_serialized_size, obu_size));

  // Write the OBU Header to the buffer.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(obu_type, 5));
  RETURN_IF_NOT_OK(wb.WriteBoolean(obu_redundant_copy));
  RETURN_IF_NOT_OK(wb.WriteBoolean(type_specific_flag));
  RETURN_IF_NOT_OK(wb.WriteBoolean(GetExtensionHeaderFlag()));
  RETURN_IF_NOT_OK(wb.WriteUleb128(obu_size));

  RETURN_IF_NOT_OK(WriteFieldsAfterObuSize(*this, wb));

  return absl::OkStatus();
}

// Reads all the fields of the OBU Header as defined in the IAMF spec
// (https://aomediacodec.github.io/iamf/#obu-header-syntax). Most of these
// fields are stored directly in the ObuHeader struct; however, for reasons
// relating to the existing encoder, `obu_type` and `obu_size` are not. We
// instead use `output_obu_type` and `output_payload_serialized_size` as output
// parameters. Note that `output_payload_serialized_size` is a derived value
// from `obu_size`, as this is the value the caller is more interested in.
absl::Status ObuHeader::ReadAndValidate(
    ReadBitBuffer& rb, int64_t& output_payload_serialized_size) {
  uint64_t obu_type_uint64_t = 0;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, obu_type_uint64_t));
  obu_type = static_cast<ObuType>(obu_type_uint64_t);
  RETURN_IF_NOT_OK(rb.ReadBoolean(obu_redundant_copy));
  RETURN_IF_NOT_OK(rb.ReadBoolean(type_specific_flag));
  bool obu_extension_flag;
  RETURN_IF_NOT_OK(rb.ReadBoolean(obu_extension_flag));
  DecodedUleb128 obu_size;
  int8_t size_of_obu_size = 0;
  RETURN_IF_NOT_OK(rb.ReadULeb128(obu_size, size_of_obu_size));
  RETURN_IF_NOT_OK(ValidateObuIsUnderTwoMegabytes(obu_size, size_of_obu_size));
  int8_t num_samples_to_trim_at_end_size = 0;
  int8_t num_samples_to_trim_at_start_size = 0;
  if (GetObuTrimmingStatusFlag()) {
    // Sometimes the type-specific flag is part of an audio frame, and it
    // signals the trimming information.
    RETURN_IF_NOT_OK(rb.ReadULeb128(num_samples_to_trim_at_end,
                                    num_samples_to_trim_at_end_size));
    RETURN_IF_NOT_OK(rb.ReadULeb128(num_samples_to_trim_at_start,
                                    num_samples_to_trim_at_start_size));
  }
  int8_t extension_header_size_size = 0;
  DecodedUleb128 extension_header_size = 0;
  if (obu_extension_flag) {
    RETURN_IF_NOT_OK(
        rb.ReadULeb128(extension_header_size, extension_header_size_size));
    RETURN_IF_NOT_OK(
        // Any extension cannot be more than 2MB also.
        ValidateObuIsUnderTwoMegabytes(extension_header_size,
                                       extension_header_size_size));
    // Avoid reading in the (possibly large) extension, until we know it is
    // plausible.
  }

  // `obu_size` measures the size of the entire OBU after itself. It's
  // possible that the trimming information and/or the extension header are
  // implausibly large.
  output_payload_serialized_size =
      GetObuPayloadSize(obu_size, num_samples_to_trim_at_end_size,
                        num_samples_to_trim_at_start_size,
                        extension_header_size_size, extension_header_size);
  if (output_payload_serialized_size < 0) {
    return absl::InvalidArgumentError(
        "obu_size not valid for OBU flags. Negative remaining payload size.");
  }
  if (obu_extension_flag) {
    extension_header_bytes = std::vector<uint8_t>(extension_header_size);
    RETURN_IF_NOT_OK(rb.ReadUint8Span(absl::MakeSpan(*extension_header_bytes)));
  }

  RETURN_IF_NOT_OK(Validate(*this));

  return absl::OkStatus();
}

bool ObuHeader::GetObuTrimmingStatusFlag() const {
  if (IsAudioFrame(obu_type)) {
    return type_specific_flag;
  } else {
    // Under other types, this flag is named something else, or is reserved.
    return false;
  }
}

bool ObuHeader::GetOptionalFieldsFlag() const {
  if (obu_type == kObuIaMixPresentation) {
    return type_specific_flag;
  } else {
    // Under other types, this flag is named something else, or is reserved.
    return false;
  }
}

bool ObuHeader::GetIsKeyFrame() const {
  if (obu_type == kObuIaTemporalDelimiter) {
    // `type_specific_flag`  represents `is_not_key_frame` for temporal
    // delimiter OBUs. Invert it to provide a cleaner function name without the
    // negation.
    return !type_specific_flag;
  } else {
    // Under other types, this flag is named something else, or is reserved.
    return false;
  }
}

bool ObuHeader::GetExtensionHeaderFlag() const {
  return extension_header_bytes.has_value();
}

DecodedUleb128 ObuHeader::GetExtensionHeaderSize() const {
  return extension_header_bytes.has_value() ? extension_header_bytes->size()
                                            : 0;
}

void ObuHeader::Print(const LebGenerator& leb_generator,
                      int64_t payload_serialized_size) const {
  // Generate the header. Solely for the purpose of getting the correct
  // `obu_size`.
  DecodedUleb128 obu_size;
  if (!GetObuSizeAndValidate(leb_generator, *this, payload_serialized_size,
                             obu_size)
           .ok()) {
    ABSL_LOG(ERROR) << "Error printing OBU header";
    return;
  }
  ABSL_LOG(INFO) << "  obu_type= " << absl::StrCat(obu_type);
  ABSL_LOG(INFO) << "  size_of(payload_) " << payload_serialized_size;

  ABSL_LOG(INFO) << "  obu_redundant_copy= " << obu_redundant_copy;
  if (IsAudioFrame(obu_type)) {
    ABSL_LOG(INFO) << "  obu_trimming_status_flag= " << type_specific_flag;
  } else if (obu_type == kObuIaTemporalDelimiter) {
    ABSL_LOG(INFO) << "  is_not_key_frame= " << type_specific_flag;
  } else if (obu_type == kObuIaMixPresentation) {
    ABSL_LOG(INFO) << "  optional_fields_flag= " << type_specific_flag;
  } else {
    ABSL_LOG(INFO) << "  reserved= " << type_specific_flag;
  }

  ABSL_LOG(INFO) << "  obu_extension_flag= " << GetExtensionHeaderFlag();

  ABSL_LOG(INFO) << "  obu_size=" << obu_size;

  if (GetObuTrimmingStatusFlag()) {
    ABSL_LOG(INFO) << "  num_samples_to_trim_at_end= "
                   << num_samples_to_trim_at_end;
    ABSL_LOG(INFO) << "  num_samples_to_trim_at_start= "
                   << num_samples_to_trim_at_start;
  }

  if (GetExtensionHeaderFlag()) {
    ABSL_LOG(INFO) << "  extension_header_size= " << GetExtensionHeaderSize();
    ABSL_LOG(INFO) << "  extension_header_bytes omitted.";
  }
}

}  // namespace iamf_tools

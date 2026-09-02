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
#ifndef OBU_ARBITRARY_OBU_H_
#define OBU_ARBITRARY_OBU_H_

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief An arbitrary OBU.
 *
 * This class is useful to create edge-cases, invalid streams, or to allow
 * encoding features that are not otherwise directly supported.
 *
 * Usually this class is used in a way that means any side effects of the OBU
 * are not taken into account.
 */
class ArbitraryObu : public ObuBase {
 public:
  /*!\brief A hook describing how the OBU will be put into the bitstream. */
  enum InsertionHook {
    kInsertionHookBeforeDescriptors,
    kInsertionHookAfterDescriptors,
    kInsertionHookAfterIaSequenceHeader,
    kInsertionHookAfterCodecConfigs,
    kInsertionHookAfterAudioElements,
    kInsertionHookAfterMixPresentations,
    kInsertionHookBeforeParameterBlocksAtTick,
    kInsertionHookAfterParameterBlocksAtTick,
    kInsertionHookAfterAudioFramesAtTick,
  };

  /*!\brief Constructor.
   *
   * \param obu_type Type of the OBU.
   * \param header Header of the OBU.
   * \param payload Payload of the OBU.
   * \param insertion_hook Hook describing when to insert the OBU.
   * \param insertion_tick Optional hook to describe the tick to insert the OBU.
   * \param invalidates_bitstream Whether writing the OBU invalidates the
   *        bitstream.
   */
  ArbitraryObu(
      ObuType obu_type, const ObuHeader& header,
      absl::Span<const uint8_t> payload, InsertionHook insertion_hook,
      const std::optional<InternalTimestamp>& insertion_tick = std::nullopt,
      bool invalidates_bitstream = false)
      : ObuBase(header, obu_type),
        payload_(payload.begin(), payload.end()),
        insertion_hook_(insertion_hook),
        insertion_tick_(insertion_tick),
        invalidates_bitstream_(invalidates_bitstream) {}

  /*!\brief Destructor. */
  ~ArbitraryObu() override = default;

  friend bool operator==(const ArbitraryObu& lhs,
                         const ArbitraryObu& rhs) = default;

  /*!\brief Writes arbitrary OBUs with the specified hook.
   *
   * \param insertion_hook Hook of OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \param wb Write buffer to write to.
   * \return `absl::OkStatus()` on success. A specific status if
   *         writing any of the OBUs fail.
   */
  static absl::Status WriteObusWithHook(
      InsertionHook insertion_hook,
      const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb);

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  const std::vector<uint8_t> payload_;

  // Metadata.
  const InsertionHook insertion_hook_;
  const std::optional<InternalTimestamp> insertion_tick_;
  const bool invalidates_bitstream_;

 private:
  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific error if
   *         `invalidates_bitstream_` is true. Other specific statuses on
   *         failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // OBU_ARBITRARY_OBU_H_

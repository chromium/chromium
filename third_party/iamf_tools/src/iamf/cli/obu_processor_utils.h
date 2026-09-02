/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_OBU_PROCESSOR_UTILS_H_
#define CLI_OBU_PROCESSOR_UTILS_H_

#include <cstdint>
#include <list>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief A selected Mix and Layout indices found in the Mixes. */
struct SelectedMixPresentation {
  MixPresentationObu* absl_nonnull mix_presentation;
  Layout output_layout;
  // The index of the sub-mix where the selected Layout was found.
  int sub_mix_index;
  // The index of the selected Layout of possible sub-mix Layouts.
  int layout_index;
};

/*!\brief Finds a MixPresentation/Layout given optional ID/Layout.
 *
 * If the ID is specified and found, we use that Mix Presentation.
 *
 * Otherwise we select a MixPresentation based on section 7.4.1 of
 * the IAMF specification.
 *
 * If neither ID nor Layout is specified, we default to the first.
 *
 * If the selected Mix (found by ID, layout or default first) does
 * not contain the desired Layout, we will push back a new Layout.
 *
 * \param audio_elements Associated audio elements.
 * \param supported_mix_presentations All usable MixPresentations.
 * \param desired_layout If specified, the decoding target Layout.
 * \param desired_mix_presentation_id The target Mix if specified.
 * \return If no error, returns Mix Presentation, ID, and Layout.
 */
absl::StatusOr<SelectedMixPresentation> FindMixPresentationAndLayout(
    const DescriptorObus::AudioElementsById& audio_elements,
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const std::optional<Layout>& desired_layout,
    std::optional<uint32_t> desired_mix_presentation_id);

/*!\brief Creates a simplified MixPresentation/Layout for rendering.
 *
 * The simplified MixPresentation will only have a single sub-mix and a single
 * layout.
 *
 * \param mix_presentation MixPresentation to create a simplified version of.
 * \param sub_mix_index The index of the sub-mix to keep.
 * \param layout_index The index of the layout to keep.
 * \return Simplified `MixPresentationObu`, or an error if the indices are out
 *         of bounds.
 */
absl::StatusOr<MixPresentationObu> CreateSimplifiedMixPresentationForRendering(
    const MixPresentationObu& mix_presentation, int sub_mix_index,
    int layout_index);

}  // namespace iamf_tools

#endif  // CLI_OBU_PROCESSOR_UTILS_H_

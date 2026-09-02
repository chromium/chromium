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

#ifndef CLI_IAMF_COMPONENTS_H_
#define CLI_IAMF_COMPONENTS_H_

#include <memory>
#include <string>
#include <vector>

#include "iamf/cli/layout_renderer_factory.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {

/*!\brief Creates an instance of `LayoutRendererFactory`.
 *
 * This is useful for binding different kinds of layout renderer factories in
 * an IAMF Encoder.
 *
 * \return Unique pointer to the created layout renderer factory.
 */
std::unique_ptr<LayoutRendererFactory> CreateLayoutRendererFactory();

/*!\brief Creates an instance of `LoudnessCalculatorFactoryBase`.
 *
 * This is useful for binding different kinds of loudness calculator factories
 * in an IAMF Encoder.
 *
 * \return Unique pointer to the created loudness calculator factory.
 */
std::unique_ptr<LoudnessCalculatorFactoryBase>
CreateLoudnessCalculatorFactory();

/*!\brief Creates instances of `ObuSequencerBase`.
 *
 * This is useful for binding different kinds of sequencers in an IAMF Encoder.
 *
 * \param user_metadata Input user metadata.
 * \param output_iamf_directory Directory to output IAMF files to.
 * \param include_temporal_delimiters Whether the serialized data should
 *        include temporal delimiters.
 * \return Vector of unique pointers to the created OBU sequencers.
 */
std::vector<std::unique_ptr<ObuSequencerBase>> CreateObuSequencers(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::string& output_iamf_directory, bool include_temporal_delimiters);

}  // namespace iamf_tools

#endif  // CLI_IAMF_COMPONENTS_H_

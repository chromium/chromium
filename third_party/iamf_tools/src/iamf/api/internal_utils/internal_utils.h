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

#ifndef API_INTERNAL_UTILS_INTERNAL_UTILS_H_
#define API_INTERNAL_UTILS_INTERNAL_UTILS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "iamf/cli/wav_writer.h"
#include "iamf/include/iamf_tools/iamf_decoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
namespace iamf_tools {

/*!\ This file contains utility functions that use the public iamf_decoder
 * API to perform internal tasks. This is not intended to be used by external
 * clients.
 */

/*!\brief Configures the wav writer and output sample buffer.
 *
 * Configuration is based on the output properties of the decoder.
 *
 * \param decoder Decoder used to determine output properties.
 * \param output_filename Filename to use for the wav writer.
 * \param wav_writer Wav writer to configure.
 * \param reusable_sample_buffer Sample buffer to configure.
 */
api::IamfStatus SetupAfterDescriptors(
    const api::IamfDecoderInterface& decoder,
    const std::string& output_filename, std::unique_ptr<WavWriter>& wav_writer,
    std::vector<uint8_t>& reusable_sample_buffer);

/*!\brief Dump all pending temporal units from the decoder to the wav writer.
 *
 * \param decoder Decoder which holds the pending temporal units.
 * \param reusable_sample_buffer Buffer into which the decoder will write
 *        decoded temporal units.
 * \param wav_writer Wav writer to write to.
 * \param output_num_temporal_units_processed Number of temporal units
 *        processed.
 */
api::IamfStatus DumpPendingTemporalUnitsToWav(
    api::IamfDecoderInterface& decoder,
    std::vector<uint8_t>& reusable_sample_buffer, WavWriter& wav_writer,
    int32_t& output_num_temporal_units_processed);

}  // namespace iamf_tools

#endif  // API_INTERNAL_UTILS_INTERNAL_UTILS_H_

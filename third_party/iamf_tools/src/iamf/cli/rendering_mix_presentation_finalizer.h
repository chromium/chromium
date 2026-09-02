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

#ifndef CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_
#define CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_

#include <cstddef>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/layout_renderer_factory.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A class that renders and finalizes IAMF mixes.
 *
 * The use pattern of this class is:
 *   // Call the factory function and handle any errors.
 *   auto finalizer = RenderingMixPresentationFinalizer::Create(...);
 *   if(!finalizer.ok()) {
 *     // Handle error.
 *   }
 *
 *   while (source has temporal units) {
 *     // Push the next temporal unit.
 *     RETURN_IF_NOT_OK(finalizer->PushTemporalUnit(...));
 *     // Get the post-processed samples for each relevant layout. Relevant
 *     // layouts depend on use-case.
 *     RETURN_IF_NOT_OK(finalizer->GetPostProcessedSamplesAsSpan(...));
 *   }
 *   RETURN_IF_NOT_OK(finalizer->FinalizePushingTemporalUnits());
 *   // Get the post-processed samples for each relevant layout. Relevant
 *   // layouts depend on use-case.
 *   RETURN_IF_NOT_OK(finalizer->GetPostProcessedSamplesAsSpan(...));
 *   // Get the final OBUs, with measured loudness information.
 *   absl::StatusOr<...> mix_presentation_obus =
 *     finalizer->GetFinalizedMixPresentationOBUs();
 *   // Handle any errors, or use the output mix presentation OBUs.
 */
class RenderingMixPresentationFinalizer {
 public:
  // Contains rendering metadata for all audio elements in a given layout.
  struct LayoutRenderingMetadata {
    // Renderer for this layout; may be `nullptr` if this layout cannot be
    // rendered.
    std::unique_ptr<LayoutRendererBase> layout_renderer;

    // Controlled by the `SampleProcessorFactory`; may be `nullptr` if the user
    // does not want post-processing this layout.
    std::unique_ptr<SampleProcessorBase> sample_processor;

    // Controlled by the `LoudnessCalculatorFactory`; may be `nullptr` if the
    // user does not want loudness calculated for this layout.
    std::unique_ptr<LoudnessCalculatorBase> loudness_calculator;

    // Reusable buffer for storing rendered samples.
    std::vector<std::vector<InternalSampleType>> rendered_samples;

    // Vector of views into the valid portions of the channels in
    // `rendered_samples`.
    std::vector<absl::Span<const InternalSampleType>> valid_rendered_samples;
  };

  // Each sub-mix may have many layouts. Rendering metadata for layouts
  // belonging to the same sub-mix are grouped and referred to as
  // `SubMixRenderingMetadata`.
  using SubMixRenderingMetadata = std::vector<LayoutRenderingMetadata>;

  /*!\brief Factory for a sample processor.
   *
   * Used to create a sample processor for use in post-processing the rendering.
   *
   * For example, if the user only wants a particular layout (e.g. stereo), or a
   * particular mix presentation to be saved to a wav file, then a factory could
   * select relevant layouts and mix presentations to create a `WavWriter` for.
   *
   * \param mix_presentation_id Mix presentation ID.
   * \param sub_mix_index Index of the sub-mix within the mix presentation.
   * \param layout_index Index of the layout within the sub-mix.
   * \param layout Associated layout.
   * \param prefix Prefix for the output file.
   * \param num_channels Number of channels.
   * \param sample_rate Sample rate of the input audio.
   * \param bit_depth Bit depth of the input audio.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Unique pointer to a sample processor or `nullptr` if none is
   *         desired.
   */
  typedef absl::AnyInvocable<std::unique_ptr<SampleProcessorBase>(
      DecodedUleb128 mix_presentation_id, size_t sub_mix_index,
      size_t layout_index, const Layout& layout, int num_channels,
      int sample_rate, int bit_depth, size_t num_samples_per_frame) const>
      SampleProcessorFactory;

  /*!\brief Factory that never returns a sample processor.
   *
   * For convenience to use with `Create`.
   */
  static std::unique_ptr<SampleProcessorBase> ProduceNoSampleProcessors(
      DecodedUleb128 /*mix_presentation_id*/, size_t /*sub_mix_index*/,
      size_t /*layout_index*/, const Layout& /*layout*/, int /*num_channels*/,
      int /*sample_rate*/, int /*bit_depth*/,
      size_t /*num_samples_per_frame*/) {
    return nullptr;
  }

  /*!\brief Creates a rendering mix presentation finalizer.
   *
   * Rendering metadata is extracted from the mix presentation OBUs, which will
   * be used to render the mix presentations in PushTemporalUnit.
   *
   * \param layout_renderer_factory Factory to create layout renderers, or
   *        `nullptr` to disable rendering.
   * \param loudness_calculator_factory Factory to create loudness calculators
   *        or `nullptr` to disable loudness calculation.
   * \param audio_elements Audio elements with data.
   * \param sample_processor_factory Factory to create sample processors for use
   *        after rendering.
   * \param mix_presentation_obus OBUs to render and measure the loudness of.
   * \return Instance of `RenderingMixPresentationFinalizer` on success. A
   *         specific status on failure.
   */
  static absl::StatusOr<RenderingMixPresentationFinalizer> Create(
      const LayoutRendererFactory* absl_nullable layout_renderer_factory,
      const LoudnessCalculatorFactoryBase* absl_nullable
          loudness_calculator_factory,
      const DescriptorObus::AudioElementsById& audio_elements,
      const SampleProcessorFactory& sample_processor_factory,
      const DescriptorObus::MixPresentationObus& mix_presentation_obus);

  /*!\brief Move constructor. */
  RenderingMixPresentationFinalizer(RenderingMixPresentationFinalizer&&) =
      default;
  /*!\brief Destructor. */
  ~RenderingMixPresentationFinalizer() = default;

  /*!\brief Renders and writes a single temporal unit.
   *
   * Renders a single temporal unit for all mix presentations. It also
   * accumulates the loudness of the rendered samples which will be finalized
   * once FinalizePushingTemporalUnits() is called. This function must not be
   * called after FinalizePushingTemporalUnits() has been called.
   *
   * \param id_to_labeled_frame Data structure of samples for a given timestamp,
   *        keyed by audio element ID and channel label.
   * \param start_timestamp Start timestamp of this temporal unit.
   * \param end_timestamp End timestamp of this temporal unit.
   * \param parameter_blocks Parameter Block OBUs associated with this temporal
   *        unit.
   * \param mix_presentation_obus Output list of OBUs to finalize with initial
   *        user-provided loudness information.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushTemporalUnit(
      const IdLabeledFrameMap& id_to_labeled_frame,
      InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
      const std::list<ParameterBlockWithData>& parameter_blocks);

  /*!\brief Retrieves cached post-processed samples.
   *
   * Retrieves the post-processed samples for a given mix presentation, sub-mix,
   * and layout. Or the rendered samples if no post-processor is available. New
   * data is available after each call to `PushTemporalUnit` or
   * `FinalizePushingTemporalUnits`. The output span is invalidated by any
   * further calls to `PushTemporalUnit` or `FinalizePushingTemporalUnits` and
   * typically should be consumed or copied immediately.
   *
   * Simple use pattern:
   *   - Call based on the same layout each time. E.g. to always render the
   *     same stereo layout.
   *
   * More complex use pattern:
   *   - Call multiple times based on a small set of layouts. (E.g. to back a
   *     buffer to support seamless transitions when a GUI element is clicked to
   *     toggle between mixes, language, or loudnspeaker layout).
   *   - Call for each layout, to cache and save all possible rendered layouts
   *     to a file.
   *
   * \param mix_presentation_id Mix presentation ID
   * \param sub_mix_index Index of the sub-mix to retrieve.
   * \param layout_index Index of the layout to retrieve.
   * \param Post-processed samples, or rendered samples if no post-processor is
   *        available. A specific status on failure.
   */
  absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
  GetPostProcessedSamplesAsSpan(DecodedUleb128 mix_presentation_id,
                                size_t sub_mix_index,
                                size_t layout_index) const;

  /*!\brief Signals that `PushTemporalUnit` will no longer be called.
   *
   * \return `absl::OkStatus()` on success. `absl::FailedPreconditionError` if
   *         this function has already been called.
   */
  absl::Status FinalizePushingTemporalUnits();

  /*!\brief Retrieves the finalized mix presentation OBUs.
   *
   * Will return mix presentation OBUs with updated loudness information. Should
   * only be called after `FinalizePushingTemporalUnits` has been called.
   *
   * \param validate_loudness If true, validate the computed loudness matches
   *        the original user-provided provided loudness.
   * \return List of finalized OBUs with calculated loudness information. A
   *         specific status on failure.
   */
  absl::StatusOr<DescriptorObus::MixPresentationObus>
  GetFinalizedMixPresentationObus(bool validate_loudness);

 private:
  enum State {
    kAcceptingTemporalUnits,
    kFinalizePushTemporalUnitCalled,
    kFlushedFinalizedMixPresentationObus
  };

  /*!\brief Private constructor.
   *
   * Used only by the factory method.
   *
   * \param mix_presentation_id_to_sub_mix_rendering_metadata Mix presentation
   *        ID to rendering metadata for each sub-mix.
   * \param mix_presentation_obus Mix presentation OBUs to render and measure
   *        the loudness of.
   */
  RenderingMixPresentationFinalizer(
      absl::flat_hash_map<DecodedUleb128,
                          std::vector<SubMixRenderingMetadata>>&&
          mix_presentation_id_to_sub_mix_rendering_metadata,
      DescriptorObus::MixPresentationObus&& mix_presentation_obus)
      : mix_presentation_id_to_sub_mix_rendering_metadata_(
            std::move(mix_presentation_id_to_sub_mix_rendering_metadata)),
        mix_presentation_obus_(std::move(mix_presentation_obus)) {}

  State state_ = kAcceptingTemporalUnits;

  // Mapping from Mix Presentation ID to rendering metadata. Slots are absent
  // for Mix Presentations that have no layouts which can be rendered.
  absl::flat_hash_map<DecodedUleb128, std::vector<SubMixRenderingMetadata>>
      mix_presentation_id_to_sub_mix_rendering_metadata_;

  // Mix Presentation OBUs to render and measure the loudness of.
  DescriptorObus::MixPresentationObus mix_presentation_obus_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_

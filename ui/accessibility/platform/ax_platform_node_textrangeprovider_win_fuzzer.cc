// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_fuzzer_util.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

#include <UIAutomationClient.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

using Microsoft::WRL::ComPtr;

// We generate positions using fuzz data, this constant should be aligned
// with the amount of bytes needed to generate a new text range.
constexpr size_t kBytesNeededToGenerateTextRange = 4;
// This should be aligned with the amount of bytes needed to mutate a text range
// in ...MutateTextRangeProvider...
constexpr size_t kBytesNeededToMutateTextRange = 4;

// Min/Max node size for the initial tree.
constexpr size_t kMinNodeCount = 10;
constexpr size_t kMaxNodeCount = kMinNodeCount + 50;

// Min fuzz data needed for fuzzer to function.
// We need to ensure we have enough data to create a tree with text, as well as
// generate a couple of text ranges and mutate them.
constexpr size_t kMinFuzzDataSize =
    kMinNodeCount * AXTreeFuzzerGenerator::kMinimumNewNodeFuzzDataSize +
    kMinNodeCount * AXTreeFuzzerGenerator::kMinTextFuzzDataSize +
    2 * kBytesNeededToGenerateTextRange + 2 * kBytesNeededToMutateTextRange;

// Cap fuzz data to avoid slowness.
constexpr size_t kMaxFuzzDataSize = 3500;

ui::AXPlatformNode* AXPlatformNodeFromNode(ui::AXTree* tree, ui::AXNode* node) {
  const ui::TestAXNodeWrapper* wrapper =
      ui::TestAXNodeWrapper::GetOrCreate(tree, node);
  return wrapper ? wrapper->ax_platform_node() : nullptr;
}

template <typename T>
ComPtr<T> QueryInterfaceFromNode(ui::AXTree* tree, ui::AXNode* node) {
  ui::AXPlatformNode* ax_platform_node = AXPlatformNodeFromNode(tree, node);
  if (!ax_platform_node)
    return ComPtr<T>();
  ComPtr<T> result;
  ax_platform_node->GetNativeViewAccessible()->QueryInterface(__uuidof(T),
                                                              &result);
  return result;
}

// This method returns a text range scoped to this node.
void GetTextRangeProviderFromTextNode(
    ui::AXTree* tree,
    ui::AXNode* text_node,
    ComPtr<ITextRangeProvider>& text_range_provider) {
  ComPtr<IRawElementProviderSimple> provider_simple =
      QueryInterfaceFromNode<IRawElementProviderSimple>(tree, text_node);
  if (!provider_simple.Get())
    return;
  ComPtr<ITextProvider> text_provider;
  provider_simple->GetPatternProvider(UIA_TextPatternId, &text_provider);

  if (!text_provider.Get())
    return;

  text_provider->get_DocumentRange(&text_range_provider);

  ComPtr<ui::AXPlatformNodeTextRangeProviderWin> text_range_provider_interal;
  text_range_provider->QueryInterface(
      IID_PPV_ARGS(&text_range_provider_interal));
  ui::AXPlatformNode* ax_platform_node =
      AXPlatformNodeFromNode(tree, text_node);
  text_range_provider_interal->SetOwnerForTesting(
      static_cast<ui::AXPlatformNodeWin*>(ax_platform_node));
}

void CallComparisonAPIs(const ComPtr<ITextRangeProvider>& text_range,
                        const ComPtr<ITextRangeProvider>& other_text_range) {
  BOOL are_same;
  std::ignore = text_range->Compare(other_text_range.Get(), &are_same);
  int compare_endpoints_result;
  std::ignore = text_range->CompareEndpoints(
      TextPatternRangeEndpoint_Start, other_text_range.Get(),
      TextPatternRangeEndpoint_Start, &compare_endpoints_result);
  std::ignore = text_range->CompareEndpoints(
      TextPatternRangeEndpoint_End, other_text_range.Get(),
      TextPatternRangeEndpoint_Start, &compare_endpoints_result);
  std::ignore = text_range->CompareEndpoints(
      TextPatternRangeEndpoint_Start, other_text_range.Get(),
      TextPatternRangeEndpoint_End, &compare_endpoints_result);
  std::ignore = text_range->CompareEndpoints(
      TextPatternRangeEndpoint_End, other_text_range.Get(),
      TextPatternRangeEndpoint_End, &compare_endpoints_result);
}

TextPatternRangeEndpoint GenerateEndpoint(unsigned char byte) {
  return (byte % 2) ? TextPatternRangeEndpoint_Start
                    : TextPatternRangeEndpoint_End;
}

TextUnit GenerateTextUnit(unsigned char byte) {
  return static_cast<TextUnit>(byte % 7);
}

enum class TextRangeMutation {
  kMoveEndpointByRange,
  kExpandToEnclosingUnit,
  kMove,
  kMoveEndpointByUnit,
  kLast
};

TextRangeMutation GenerateTextRangeMutation(unsigned char byte) {
  constexpr unsigned char max =
      static_cast<unsigned char>(TextRangeMutation::kLast);
  return static_cast<TextRangeMutation>(byte % max);
}

void MutateTextRangeProvider(ComPtr<ITextRangeProvider>& text_range,
                             const ComPtr<ITextRangeProvider>& other_text_range,
                             FuzzerData& fuzz_data) {
  const int kMaxMoveCount = 20;
  TextUnit unit = GenerateTextUnit(fuzz_data.NextByte());
  int units_moved;

  TextRangeMutation mutation_type =
      GenerateTextRangeMutation(fuzz_data.NextByte());
  switch (mutation_type) {
    case TextRangeMutation::kMoveEndpointByRange:
      if (other_text_range.Get()) {
        text_range->MoveEndpointByRange(GenerateEndpoint(fuzz_data.NextByte()),
                                        other_text_range.Get(),
                                        GenerateEndpoint(fuzz_data.NextByte()));
        return;
      }
      ABSL_FALLTHROUGH_INTENDED;
    case TextRangeMutation::kExpandToEnclosingUnit:
      text_range->ExpandToEnclosingUnit(unit);
      return;
    case TextRangeMutation::kMove:
      text_range->Move(unit, fuzz_data.NextByte() % kMaxMoveCount,
                       &units_moved);
      return;
    case TextRangeMutation::kMoveEndpointByUnit:
      text_range->MoveEndpointByUnit(GenerateEndpoint(fuzz_data.NextByte()),
                                     unit, fuzz_data.NextByte() % kMaxMoveCount,
                                     &units_moved);
      return;
    case TextRangeMutation::kLast:
      NOTREACHED_IN_MIGRATION();
  }
}

struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  if (size < kMinFuzzDataSize || size > kMaxFuzzDataSize)
    return 0;
  static Environment env;

  FuzzerData fuzz_data(data, size);
  AXTreeFuzzerGenerator generator;

  // Create initial tree.
  const size_t node_count =
      kMinNodeCount + fuzz_data.NextByte() % (kMaxNodeCount - kMinNodeCount);
  generator.GenerateInitialUpdate(fuzz_data, node_count);

  ui::AXTree* tree = generator.GetTree();

  // Run with --v=1 to aid in debugging a specific crash.
  VLOG(1) << tree->ToString();

  // Loop until data is expended.
  std::vector<ComPtr<ITextRangeProvider>> created_ranges;
  while (fuzz_data.RemainingBytes() > kBytesNeededToGenerateTextRange) {
    // Create a new position on a random node in the tree.
    {
      // To ensure that anchor_id is between |ui::kInvalidAXNodeID| and the max
      // ID of the tree (non-inclusive), get a number [0, max_id - 1) and then
      // shift by 1 to get [1, max_id)
      ui::AXNodeID anchor_id =
          (fuzz_data.NextByte() % (generator.GetMaxAssignedID() - 1)) + 1;
      ui::AXNode* anchor = tree->GetFromId(anchor_id);
      if (!anchor)
        continue;
      ComPtr<ITextRangeProvider> text_range_provider;
      GetTextRangeProviderFromTextNode(tree, anchor, text_range_provider);
      if (text_range_provider)
        created_ranges.push_back(std::move(text_range_provider));
    }
    for (size_t i = 0; i < created_ranges.size(); ++i) {
      ComPtr<ITextRangeProvider> text_range = created_ranges[i];
      ComPtr<ITextRangeProvider> other_range;
      if (i > 0)
        other_range = created_ranges[i - 1].Get();
      if (!other_range.Get())
        continue;

      CallComparisonAPIs(text_range.Get(), other_range);
      if (fuzz_data.RemainingBytes() > kBytesNeededToMutateTextRange)
        MutateTextRangeProvider(text_range, other_range, fuzz_data);
    }
    // Do a Tree Update.
    if (!generator.GenerateTreeUpdate(fuzz_data, 5))
      break;
  }
  tree->Destroy();
  ui::TestAXNodeWrapper::ResetGlobalState();
  return 0;
}

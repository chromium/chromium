// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_
#define UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXNode;
class AXTree;

// This module implements language detection enabling Chrome to automatically
// detect the language for runs of text within the page.
//
// Node-level language detection runs once per page after the load complete
// event. This involves two passes:
//   *Detect* walks the tree from the given root using cld3 to detect up to 3
//            potential languages per node. A ranked list is created enumerating
//            all potential languages on a page.
//   *Label* re-walks the tree, assigning a language to each node considering
//           the potential languages from the detect phase, page level
//           statistics, and the assigned languages of ancestor nodes.
//
// Optionally an embedder may run *sub-node* language detection which attempts
// to assign languages for runs of text within a node, potentially down to the
// individual character level. This is useful in cases where a single paragraph
// involves switching between multiple languages, and where the speech engine
// doesn't automatically switch voices to handle different character sets.
// Due to the potentially small lengths of text runs involved this tends to be
// lower in accuracy, and works best when a node is composed of multiple
// languages with easily distinguishable scripts.

// AXLanguageInfo represents the local language detection data for all text
// within an AXNode. Stored on AXNode.
struct AX_EXPORT AXLanguageInfo {
  AXLanguageInfo();
  ~AXLanguageInfo();

  // This is the final language we have assigned for this node during the
  // 'label' step, it is the result of merging:
  //  a) The detected language for this node
  //  b) The declared lang attribute on this node
  //  c) the (recursive) language of the parent (detected or declared).
  //
  // This will be the empty string if no language was assigned during label
  // phase.
  //
  // IETF BCP 47 Language code (rfc5646).
  // examples:
  //  'de'
  //  'de-DE'
  //  'en'
  //  'en-US'
  //  'es-ES'
  //
  // This should not be read directly by clients of AXNode, instead clients
  // should call AXNode::GetLanguage().
  std::string language;

  // Detected languages for this node sorted as returned by
  // FindTopNMostFreqLangs, which sorts in decreasing order of probability,
  // filtered to remove any unreliable results.
  std::vector<std::string> detected_languages;
};

// Each AXLanguageSpan contains a language, a probability, and start and end
// indices. The indices are used to specify the substring that contains the
// associated language. The string which the indices are relative to is not
// included in this structure.
// Also, the indices are relative to a Utf8 string.
// See documentation on GetLanguageAnnotationForStringAttribute for details
// on how to associate this object with a string.
struct AX_EXPORT AXLanguageSpan {
  int start_index;
  int end_index;
  std::string language;
  float probability;
};

// A single AXLanguageInfoStats instance is stored on each AXTree and contains
// statistics on detected languages for all the AXNodes in that tree.
//
// We rely on these tree-level statistics when labelling individual nodes, to
// provide extra signals to increase our confidence in assigning a detected
// language.
//
// The Label step will only assign a detected language to a node if that
// language is one of the most frequent languages on the page.
//
// For example, if a single node has detected_languages (in order of probability
// assigned by cld_3): da-DK, en-AU, fr-FR, but the page statistics overall
// indicate that the page is generally in en-AU and ja-JP, it is more likely to
// be a mis-recognition of Danish than an accurate assignment, so we assign
// en-AU instead of da-DK.
class AX_EXPORT AXLanguageInfoStats {
 public:
  AXLanguageInfoStats();
  ~AXLanguageInfoStats();

  // Adjust our statistics to add provided detected languages.
  void Add(const std::vector<std::string>& languages);

  // Fetch the score for a given language.
  int GetScore(const std::string& lang) const;

  // Check if a given language is within the top results.
  bool CheckLanguageWithinTop(const std::string& lang);

 private:
  // Store a count of the occurrences of a given language.
  std::unordered_map<std::string, unsigned int> lang_counts_;

  // Cache of last calculated top language results.
  // A vector of pairs of (score, language) sorted by descending score.
  std::vector<std::pair<unsigned int, std::string>> top_results_;
  // Boolean recording that we have not mutated the statistics since last
  // calculating top results, setting this to false will cause recalculation
  // when the results are next fetched.
  bool top_results_valid_;

  void InvalidateTopResults();

  void GenerateTopResults();

  DISALLOW_COPY_AND_ASSIGN(AXLanguageInfoStats);
};

// AXLanguageDetectionManager manages all of the context needed for language
// detection within an AXTree.
class AX_EXPORT AXLanguageDetectionManager {
 public:
  AXLanguageDetectionManager();
  ~AXLanguageDetectionManager();

  // Detect language for each node in the subtree rooted at the given node.
  // This is the first pass in detection and labelling.
  // This only detects the language, it does not label it, for that see
  //  LabelLanguageForSubtree.
  void DetectLanguageForSubtree(AXNode* subtree_root);

  // Label language for each node in the subtree rooted at the given node.
  // This is the second pass in detection and labelling.
  // This will label the language, but relies on the earlier detection phase
  // having already completed.
  void LabelLanguageForSubtree(AXNode* subtree_root);

  // Sub-node language detection for a given string attribute.
  // For example, if a node has name: "My name is Fred", then calling
  // GetLanguageAnnotationForStringAttribute(*node, ax::mojom::StringAttribute::
  // kName) would return language detection information about "My name is Fred".
  std::vector<AXLanguageSpan> GetLanguageAnnotationForStringAttribute(
      const AXNode& node,
      ax::mojom::StringAttribute attr);

 private:
  // TODO(chrishall): should this be stored by pointer or value?
  AXLanguageInfoStats lang_info_stats;

  void DetectLanguageForSubtreeInternal(AXNode* subtree_root);
  void LabelLanguageForSubtreeInternal(AXNode* subtree_root);

  // This language identifier is constructed with a default minimum byte length
  // of chrome_lang_id::NNetLanguageIdentifier::kMinNumBytesToConsider and is
  // used for detecting page-level languages.
  chrome_lang_id::NNetLanguageIdentifier language_identifier_;

  // This language identifier is constructed with a minimum byte length of
  // kShortTextIdentifierMinByteLength so it can be used for detecting languages
  // of shorter text (e.g. one character).
  chrome_lang_id::NNetLanguageIdentifier short_text_language_identifier_;

  DISALLOW_COPY_AND_ASSIGN(AXLanguageDetectionManager);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LANGUAGE_DETECTION_H_

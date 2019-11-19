// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_utils.h"

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

base::i18n::BreakIterator::BreakType ICUBreakTypeForBoundaryType(
    AXTextBoundary boundary) {
  switch (boundary) {
    case AXTextBoundary::kCharacter:
      return base::i18n::BreakIterator::BREAK_CHARACTER;
    case AXTextBoundary::kSentenceStart:
      return base::i18n::BreakIterator::BREAK_SENTENCE;
    case AXTextBoundary::kWordStart:
    case AXTextBoundary::kWordStartOrEnd:
      return base::i18n::BreakIterator::BREAK_WORD;
    // These are currently unused since line breaking is done via an array of
    // line break offsets, and object boundary by finding no boundary within the
    // current node.
    case AXTextBoundary::kObject:
    case AXTextBoundary::kLineStart:
    case AXTextBoundary::kParagraphStart:
      return base::i18n::BreakIterator::BREAK_NEWLINE;
    default:
      NOTREACHED() << boundary;
      return base::i18n::BreakIterator::BREAK_NEWLINE;
  }
}

}  // namespace

// line_breaks is a Misnomer. Blink provides the start offsets of each line
// not the line breaks.
// TODO(nektar): Rename line_breaks a11y attribute and variable references.
size_t FindAccessibleTextBoundary(const base::string16& text,
                                  const std::vector<int>& line_breaks,
                                  AXTextBoundary boundary,
                                  size_t start_offset,
                                  AXTextBoundaryDirection direction,
                                  ax::mojom::TextAffinity affinity) {
  size_t text_size = text.size();
  DCHECK_LE(start_offset, text_size);

  base::i18n::BreakIterator::BreakType break_type =
      ICUBreakTypeForBoundaryType(boundary);
  base::i18n::BreakIterator break_iter(text, break_type);
  if (boundary == AXTextBoundary::kCharacter ||
      boundary == AXTextBoundary::kSentenceStart ||
      boundary == AXTextBoundary::kWordStart ||
      boundary == AXTextBoundary::kWordStartOrEnd) {
    if (!break_iter.Init())
      return start_offset;
  }

  if (boundary == AXTextBoundary::kLineStart) {
    if (direction == AXTextBoundaryDirection::kForwards) {
      for (size_t j = 0; j < line_breaks.size(); ++j) {
          size_t line_break = line_breaks[j] >= 0 ? line_breaks[j] : 0;
          if ((affinity == ax::mojom::TextAffinity::kDownstream &&
               line_break > start_offset) ||
              (affinity == ax::mojom::TextAffinity::kUpstream &&
               line_break >= start_offset)) {
            return line_break;
        }
      }
      return text_size;
    } else {
      for (size_t j = line_breaks.size(); j != 0; --j) {
        size_t line_break = line_breaks[j - 1] >= 0 ? line_breaks[j - 1] : 0;
        if ((affinity == ax::mojom::TextAffinity::kDownstream &&
             line_break <= start_offset) ||
            (affinity == ax::mojom::TextAffinity::kUpstream &&
             line_break < start_offset)) {
          return line_break;
        }
      }
      return 0;
    }
  }

  size_t result = start_offset;
  for (;;) {
    size_t pos;
    if (direction == AXTextBoundaryDirection::kForwards) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result == 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case AXTextBoundary::kLineStart:
        NOTREACHED() << boundary;  // This is handled above.
        return result;
      case AXTextBoundary::kCharacter:
        if (break_iter.IsGraphemeBoundary(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next character.
          if (direction == AXTextBoundaryDirection::kBackwards ||
              result != start_offset)
            return result;
        }
        break;
      case AXTextBoundary::kWordStart:
        if (break_iter.IsStartOfWord(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next word.
          if (direction == AXTextBoundaryDirection::kBackwards ||
              result != start_offset)
            return result;
        }
        break;
      case AXTextBoundary::kWordStartOrEnd:
        if (break_iter.IsStartOfWord(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next word.
          if (direction == AXTextBoundaryDirection::kBackwards ||
              result != start_offset)
            return result;
        } else if (break_iter.IsEndOfWord(result)) {
          // If we are searching backward and we are still at the end offset, we
          // need to find the previous word.
          if (direction == AXTextBoundaryDirection::kForwards ||
              result != start_offset)
            return result;
        }
        break;
      case AXTextBoundary::kSentenceStart:
        if (break_iter.IsSentenceBoundary(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next sentence.
          if (direction == AXTextBoundaryDirection::kBackwards ||
              result != start_offset) {
            // ICU sometimes returns sentence boundaries in the whitespace
            // between sentences. For the purposes of accessibility, we want to
            // include all whitespace at the end of a sentence. We move the
            // boundary past the last whitespace offset. This works the same for
            // backwards and forwards searches.
            while (result < text_size &&
                   base::IsUnicodeWhitespace(text[result]))
              result++;
            return result;
          }
        }
        break;
      case AXTextBoundary::kParagraphStart:
        if (text[pos] == '\n')
          return result;
        break;
      default:
        break;
    }

    if (direction == AXTextBoundaryDirection::kForwards) {
      result++;
    } else {
      result--;
    }
  }
}

base::string16 ActionVerbToLocalizedString(
    const ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return base::string16();
    case ax::mojom::DefaultActionVerb::kActivate:
      return l10n_util::GetStringUTF16(IDS_AX_ACTIVATE_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kCheck:
      return l10n_util::GetStringUTF16(IDS_AX_CHECK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClick:
      return l10n_util::GetStringUTF16(IDS_AX_CLICK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return l10n_util::GetStringUTF16(IDS_AX_CLICK_ANCESTOR_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kJump:
      return l10n_util::GetStringUTF16(IDS_AX_JUMP_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kOpen:
      return l10n_util::GetStringUTF16(IDS_AX_OPEN_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kPress:
      return l10n_util::GetStringUTF16(IDS_AX_PRESS_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kSelect:
      return l10n_util::GetStringUTF16(IDS_AX_SELECT_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kUncheck:
      return l10n_util::GetStringUTF16(IDS_AX_UNCHECK_ACTION_VERB);
  }
  NOTREACHED();
  return base::string16();
}

// Some APIs on Linux and Windows need to return non-localized action names.
base::string16 ActionVerbToUnlocalizedString(
    const ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return base::UTF8ToUTF16("none");
    case ax::mojom::DefaultActionVerb::kActivate:
      return base::UTF8ToUTF16("activate");
    case ax::mojom::DefaultActionVerb::kCheck:
      return base::UTF8ToUTF16("check");
    case ax::mojom::DefaultActionVerb::kClick:
      return base::UTF8ToUTF16("click");
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return base::UTF8ToUTF16("click-ancestor");
    case ax::mojom::DefaultActionVerb::kJump:
      return base::UTF8ToUTF16("jump");
    case ax::mojom::DefaultActionVerb::kOpen:
      return base::UTF8ToUTF16("open");
    case ax::mojom::DefaultActionVerb::kPress:
      return base::UTF8ToUTF16("press");
    case ax::mojom::DefaultActionVerb::kSelect:
      return base::UTF8ToUTF16("select");
    case ax::mojom::DefaultActionVerb::kUncheck:
      return base::UTF8ToUTF16("uncheck");
  }
  NOTREACHED();
  return base::string16();
}

std::vector<int> GetWordStartOffsets(const base::string16& text) {
  std::vector<int> word_starts;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return word_starts;
  // iter.Advance() returns false if we've run past end of the text.
  while (iter.Advance()) {
    if (!iter.IsWord())
      continue;
    word_starts.push_back(
        base::checked_cast<int>(iter.prev()) /* start index */);
  }
  return word_starts;
}

std::vector<int> GetWordEndOffsets(const base::string16& text) {
  std::vector<int> word_ends;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return word_ends;
  // iter.Advance() returns false if we've run past end of the text.
  while (iter.Advance()) {
    if (!iter.IsWord())
      continue;
    word_ends.push_back(base::checked_cast<int>(iter.pos()) /* end index */);
  }
  return word_ends;
}

}  // namespace ui

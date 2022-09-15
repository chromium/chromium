// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_private_webkit_constants_mac.h"

namespace ui {

const char* ToString(AXTextStateChangeType type) {
  switch (type) {
    case AXTextStateChangeTypeUnknown:
      return "AXTextStateChangeTypeUnknown";
    case AXTextStateChangeTypeEdit:
      return "AXTextStateChangeTypeEdit";
    case AXTextStateChangeTypeSelectionMove:
      return "AXTextStateChangeTypeSelectionMove";
    case AXTextStateChangeTypeSelectionExtend:
      return "AXTextStateChangeTypeSelectionExtend";
  }

  return "";
}

const char* ToString(AXTextSelectionDirection direction) {
  switch (direction) {
    case AXTextSelectionDirectionUnknown:
      return "AXTextSelectionDirectionUnknown";
    case AXTextSelectionDirectionBeginning:
      return "AXTextSelectionDirectionBeginning";
    case AXTextSelectionDirectionEnd:
      return "AXTextSelectionDirectionEnd";
    case AXTextSelectionDirectionPrevious:
      return "AXTextSelectionDirectionPrevious";
    case AXTextSelectionDirectionNext:
      return "AXTextSelectionDirectionNext";
    case AXTextSelectionDirectionDiscontiguous:
      return "AXTextSelectionDirectionDiscontiguous";
  }

  return "";
}

const char* ToString(AXTextSelectionGranularity granularity) {
  switch (granularity) {
    case AXTextSelectionGranularityUnknown:
      return "AXTextSelectionGranularityUnknown";
    case AXTextSelectionGranularityCharacter:
      return "AXTextSelectionGranularityCharacter";
    case AXTextSelectionGranularityWord:
      return "AXTextSelectionGranularityWord";
    case AXTextSelectionGranularityLine:
      return "AXTextSelectionGranularityLine";
    case AXTextSelectionGranularitySentence:
      return "AXTextSelectionGranularitySentence";
    case AXTextSelectionGranularityParagraph:
      return "AXTextSelectionGranularityParagraph";
    case AXTextSelectionGranularityPage:
      return "AXTextSelectionGranularityPage";
    case AXTextSelectionGranularityDocument:
      return "AXTextSelectionGranularityDocument";
    case AXTextSelectionGranularityAll:
      return "AXTextSelectionGranularityAll";
  }

  return "";
}

const char* ToString(AXTextEditType type) {
  switch (type) {
    case AXTextEditTypeUnknown:
      return "AXTextEditTypeUnknown";
    case AXTextEditTypeDelete:
      return "AXTextEditTypeDelete";
    case AXTextEditTypeInsert:
      return "AXTextEditTypeInsert";
    case AXTextEditTypeTyping:
      return "AXTextEditTypeTyping";
    case AXTextEditTypeDictation:
      return "AXTextEditTypeDictation";
    case AXTextEditTypeCut:
      return "AXTextEditTypeCut";
    case AXTextEditTypePaste:
      return "AXTextEditTypePaste";
    case AXTextEditTypeAttributesChange:
      return "AXTextEditTypeAttributesChange";
  }

  return "";
}

}  // namespace ui

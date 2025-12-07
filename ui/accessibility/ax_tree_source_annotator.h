// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SOURCE_ANNOTATOR_H_
#define UI_ACCESSIBILITY_AX_TREE_SOURCE_ANNOTATOR_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/accessibility/ax_tree_source_observer.h"

namespace ui {

// This is an interface for a class that could be used by any `AXTreeSource` to
// provide automatically generated accessible names, such as automatically
// generated alt text for unlabeled images. A specific annotator may be able to
// work on multiple `AXNodeSource`s, e.g. `AXNode*` and `WebAXObject`.
template <typename AXNodeSource>
class AX_EXPORT AXTreeSourceAnnotator
    : public AXTreeSourceObserver<AXNodeSource, AXTreeData*, AXNodeData> {
 public:
  virtual ~AXTreeSourceAnnotator() = default;

  // Returns the automatically generated accessible name for the given
  // `AXNodeSource`, if any. For example, in the case of an unlabeled image,
  // this would return automatically generated alt text for the image.
  virtual std::string GetAnnotation(
      const AXTreeSource<AXNodeSource, AXTreeData*, AXNodeData>& tree_source,
      const AXNodeSource& node_source) const = 0;

  // Returns a value indicating the status of the automatically generated
  // accessible name, such as whether it is currently being computed, if it has
  // been computed successfully, if the operation is still pending, etc.
  //
  // TODO(nektar): Rename `ImageAnnotationStatus` to `AnnotationStatus`.
  virtual ax::mojom::ImageAnnotationStatus GetAnnotationStatus(
      const AXTreeSource<AXNodeSource, AXTreeData*, AXNodeData>& tree_source,
      const AXNodeSource& node_source) const = 0;

  // Returns true if an accessible name for the given `AXNodeSource` has already
  // been automatically generated.
  virtual bool HasAnnotationInCache(
      const AXTreeSource<AXNodeSource, AXTreeData*, AXNodeData>& tree_source,
      const AXNodeSource& node_source) const = 0;

  // Returns true if an accessible name for the given `AXNodeSource` has already
  // been automatically generated, is in the process of being generated, or has
  // encountered an error.
  virtual bool HasNodeInCache(
      const AXTreeSource<AXNodeSource, AXTreeData*, AXNodeData>& tree_source,
      const AXNodeSource& node_source) const = 0;

  // Returns true if the existing accessible name for a node consists of mostly
  // stopwords, such as "the" and "of". This would be a strong indication that
  // the accessible name is not informative and should be replaced by an
  // automatically generated one.
  virtual bool AccessibleNameHasMostlyStopwords(
      const std::string& accessible_name) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SOURCE_ANNOTATOR_H_

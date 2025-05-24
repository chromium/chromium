// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_OS_COMPOSITOR_TREE_BASE_H_
#define UI_GL_OS_COMPOSITOR_TREE_BASE_H_

#include <concepts>
#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/overlay_layer_id.h"

namespace gl {

// The overlay layer parameters outputted by Viz.
template <typename T>
concept HasParentLayerId = requires(T params) {
  // Required to be unique in a frame and stable across frames. If it is not
  // stable across frames, the incrementality of updates will suffer.
  { params.layer_id } -> std::convertible_to<gfx::OverlayLayerId>;

  // Required to refer to a valid overlay layer in the frame, or the default
  // constructed layer id (which represents the implicit root node).
  { params.parent_layer_id } -> std::convertible_to<gfx::OverlayLayerId>;

#if EXPENSIVE_DCHECKS_ARE_ON()
  // For validation only
  { params.z_order } -> std::convertible_to<int>;
#endif
};

// An object that owns the OS compositor visual(s) and can incrementally update
// itself when called from `UpdateLayer`.
template <typename T>
concept OsCompositorVisualsWrapper = requires(T layer, const T* below_layer) {
  requires std::default_initializable<T>;
  requires std::destructible<T>;
  requires !std::copyable<T>;

  // Unsafely update the internal bookkeeping of `layer` so that it thinks it's
  // above the OS compositor node `below_layer`.
  layer.unsafe_set_below_layer(below_layer);
};

// A generic tree structure that represents a hierarchy of overlay layers. It
// takes in a topologically sorted list of overlay layers and incrementally
// updates an OS compositor tree (via platform-specific implementations of its
// interfaces).
//
// See:
// https://docs.google.com/document/d/18wX75CqPIdFAk0W4Je5q_4FZZzLHtbBJsdp6Y7o80xM/edit?usp=sharing
template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
class OsCompositorTreeBase {
 public:
  enum class UpdateMode {
    // Clear the tree at the start of the update and always rebuild it from
    // scratch. This can potentially catching bugs in the incremental update
    // optimizations.
    kFromScratch,

    // Incrementally update the tree.
    kIncrementalNoPatchSiblingsOptimization,

    // Incrementally update the tree with the "patch siblings" optimization.
    // This optimization tracks sibling information, which allows us to be aware
    // of implicit tree updates, e.g. if a overlay is placed directly above the
    // overlay below us, then newly moved overlay implicitly becomes the one
    // directly below us. In this case, we don't need to update the tree again.
    kIncremental,
  };

  OsCompositorTreeBase() : OsCompositorTreeBase(UpdateMode::kIncremental) {}
  explicit OsCompositorTreeBase(UpdateMode update_mode);
  virtual ~OsCompositorTreeBase();

  OsCompositorTreeBase(const OsCompositorTreeBase&) = delete;
  OsCompositorTreeBase& operator=(const OsCompositorTreeBase&) = delete;

  // Given overlays, builds or updates this overlay tree. `overlays` must be
  // topologically sorted by hierarchy based on the `parent_layer_id` pointers
  // and siblings must be sorted back-to-front in z-order.
  //
  // Returns true if commit succeeded.
  bool UpdateTree(const base::span<const OverlayParams> overlays);

  int num_layers_modified_last_frame_for_testing() const {
    return num_layers_modified_last_frame_;
  }

 protected:
  // Resolve the parameters into platform-specific types and update `layer`
  // incrementally.
  //
  // - If `parent_layer` is present, `layer` should be placed as a child of
  //   `parent_layer`. If `parent_layer` is not present, then `layer`'s parent
  //   should be the implicit root node.
  // - If `below_layer` is present, `layer` should be placed immediately in
  //   front of it (in terms of z-order). If `below_layer` is not present,
  //   then `layer` should be placed behind all of `parent_layer`'s children.
  //
  // Returns true if `layer` needed modification.
  virtual bool UpdateLayer(const OverlayParams& overlay,
                           const Layer* parent_layer,
                           const Layer* below_layer,
                           Layer& layer) = 0;

  // Detach `layer` from its parent and clean up any associated resources.
  virtual void DestroyLayer(std::unique_ptr<Layer> layer) = 0;

  // Finalize and commit a frame, showing its contents on the screen.
  //
  // Returns true on success.
  virtual bool CommitTree() = 0;

 private:
  const UpdateMode update_mode_;

  bool UsingPatchSiblingsOptimization() const {
    return update_mode_ == UpdateMode::kIncremental;
  }

  // The set of overlays currently in the compositor tree.
  base::flat_map<gfx::OverlayLayerId, std::unique_ptr<Layer>> layers_;

  // Return the `Layer` for `overlay_id`, or `nullptr` if there is no overlay
  // with that ID.
  Layer* GetLayer(const std::optional<gfx::OverlayLayerId>& overlay_id) const;

  // Used to speed up the lookup of sibling relationships.
  struct Siblings {
    Siblings();
    ~Siblings();

    // The layer that this node is a child of.
    gfx::OverlayLayerId parent;
    // The layer that this node is immediately in front of, if null then
    // indicates this layer is below all of its siblings.
    std::optional<gfx::OverlayLayerId> below;
    // The layer that this node is immediately behind, if null then indicates
    // this layer is on top of all of its siblings.
    std::optional<gfx::OverlayLayerId> above;
  };

  // Bookkeeping to help speed up lookup of a layer's position in the tree. This
  // is updated incrementally as we walk the overlays passed to `UpdateTree`.
  //
  // Only used when `UsingPatchSiblingsOptimization()`.
  base::flat_map<gfx::OverlayLayerId, Siblings> siblings_;

  // Update `siblings_` such that `overlay_id` is being tracked as above
  // `below_id`.
  void PatchSiblingsTogether(
      const std::optional<gfx::OverlayLayerId>& overlay_id,
      const std::optional<gfx::OverlayLayerId>& below_id);

  // Update `siblings_` to remove an overlay from its siblings, patching its
  // immediate neighbors together
  void RemoveSibling(const gfx::OverlayLayerId& overlay_id);

  // Update `siblings_` so that the layer with `overlay_id` has the parent
  // `new_parent_id` and is above `new_below_id`.
  void MoveSibling(const gfx::OverlayLayerId& overlay_id,
                   const gfx::OverlayLayerId& new_parent_id,
                   const std::optional<gfx::OverlayLayerId>& new_below_id);

  std::string PrintOverlayId(
      const std::optional<gfx::OverlayLayerId>& overlay_id);

  // Count of layers directly modified in the last frame. This does not count
  // layers whose children were removed, but it does count children who are
  // removed from parents.
  int num_layers_modified_last_frame_ = 0;
};

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
OsCompositorTreeBase<OverlayParams, Layer>::OsCompositorTreeBase(
    UpdateMode update_mode)
    : update_mode_(update_mode) {}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
OsCompositorTreeBase<OverlayParams, Layer>::~OsCompositorTreeBase() = default;

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
bool OsCompositorTreeBase<OverlayParams, Layer>::UpdateTree(
    const base::span<const OverlayParams> overlays) {
  TRACE_EVENT("gpu", "OsCompositorTreeBase::UpdateTree", "overlays",
              overlays.size());

  // DVLOG(2) has a lot of output, add a new line to help visually separate
  // frames.
  DVLOG(2) << "";
  DVLOG(2) << __func__;

#if EXPENSIVE_DCHECKS_ARE_ON()
  // For validation only!
  // Pre-walk to collect sibling information to check after updating the frame.
  base::flat_map<gfx::OverlayLayerId, Siblings> expected_siblings;
  base::flat_set<gfx::OverlayLayerId> seen_layer_ids{gfx::OverlayLayerId()};
  for (size_t i = 0u; i < overlays.size(); i++) {
    const auto& overlay = overlays[i];
    const auto overlay_id = overlay.layer_id;

    if (UsingPatchSiblingsOptimization()) {
      auto& siblings = expected_siblings[overlay_id];

      siblings.parent = overlay.parent_layer_id;

      for (int j = i - 1; j >= 0; j--) {
        if (overlays[j].parent_layer_id == overlay.parent_layer_id) {
          CHECK_LT(overlays[j].z_order, overlay.z_order)
              << "Siblings must be sorted. "
              << "overlays[j] = " << overlays[j].layer_id.ToString()
              << ", overlay = " << overlay.layer_id.ToString();
          siblings.below = overlays[j].layer_id;
          break;
        }
      }

      for (size_t j = i + 1; j < overlays.size(); j++) {
        if (overlays[j].parent_layer_id == overlay.parent_layer_id) {
          CHECK_GT(overlays[j].z_order, overlay.z_order)
              << "Siblings must be sorted. "
              << "overlays[j] = " << overlays[j].layer_id.ToString()
              << ", overlay = " << overlay.layer_id.ToString();
          siblings.above = overlays[j].layer_id;
          break;
        }
      }
    }

    // The default layer ID is reserved for the implicit root node in the OS
    // compositor tree.
    CHECK_NE(overlay_id, gfx::OverlayLayerId())
        << "Overlay must have non-default layer ID.";
    // Check the hierarchy is topologically sorted (i.e. we walk a parent layer
    // before walking its children)
    CHECK(seen_layer_ids.contains(overlay.parent_layer_id))
        << "Overlays must be topologically sorted. "
        << "overlay.parent_layer_id = " << overlay.parent_layer_id.ToString();
    // Check the overlays have unique and valid layer IDs.
    CHECK(!seen_layer_ids.contains(overlay_id))
        << "Overlay layer IDs must all be unique. "
        << "overlay_id = " << overlay_id.ToString();
    seen_layer_ids.insert(overlay_id);
  }

  if (!UsingPatchSiblingsOptimization()) {
    CHECK(siblings_.empty());
  }
#endif

  int num_layers_modified = 0;

  if (update_mode_ == UpdateMode::kFromScratch) {
    TRACE_EVENT("gpu", "Cleanup all layers");
    for (auto& layer : layers_) {
      DestroyLayer(std::move(layer.second));
    }
    layers_.clear();
  } else {
    // Cleanup overlays that are unused in this frame.
    TRACE_EVENT("gpu", "Cleanup unused layers");

    base::flat_set<gfx::OverlayLayerId> seen_in_frame;
    seen_in_frame.reserve(overlays.size());
    for (const auto& overlay : overlays) {
      seen_in_frame.insert(overlay.layer_id);
    }

    auto it = layers_.begin();
    while (it != layers_.end()) {
      if (!seen_in_frame.contains(it->first)) {
        const gfx::OverlayLayerId overlay_id = it->first;
        DVLOG(2) << "> Cleanup unused overlay " << PrintOverlayId(overlay_id);

        if (UsingPatchSiblingsOptimization()) {
          TRACE_EVENT("gpu", "patch_siblings_optimization");
          // Remove this overlay from the siblings list, patching its neighbors
          // together to avoid a potential update when updating them.
          RemoveSibling(overlay_id);
        }

        // Removing a tree counts as modifying its parent.
        num_layers_modified++;

        DestroyLayer(std::move(it->second));

        it = layers_.erase(it);
      } else {
        it++;
      }
    }
  }

  for (size_t i = 0u; i < overlays.size(); i++) {
    const auto& overlay = overlays[i];
    const auto overlay_id = overlay.layer_id;

    // All overlay layers must have a non-default ID. If we hit this, it
    // indicates a bug in Viz, e.g. during overlay processing.
    CHECK_NE(overlay_id, gfx::OverlayLayerId());

    auto& layer = layers_[overlay_id];
    if (!layer) {
      layer = std::make_unique<Layer>();
    }

    std::optional<gfx::OverlayLayerId> below_id;
    // Walk the overlays backwards to find the overlay that's "below" this. We
    // need to walk everything since overlay with the same parent aren't
    // necessarily contiguous.
    for (int j = i - 1; j >= 0; j--) {
      if (overlays[j].parent_layer_id == overlay.parent_layer_id) {
        below_id = overlays[j].layer_id;
        break;
      }
    }

    TRACE_EVENT("gpu", "Update overlay layer");
    const Layer* parent_layer = overlay.parent_layer_id != gfx::OverlayLayerId()
                                    ? layers_[overlay.parent_layer_id].get()
                                    : nullptr;
    const Layer* below_layer =
        below_id ? layers_[below_id.value()].get() : nullptr;
    if (UpdateLayer(overlay, parent_layer, below_layer, *layer)) {
      num_layers_modified++;

      DVLOG(2) << "> Updated layer: " << PrintOverlayId(overlay_id)
               << ", parent = " << PrintOverlayId(overlay.parent_layer_id)
               << ", below = " << PrintOverlayId(below_id);

      if (UsingPatchSiblingsOptimization()) {
        TRACE_EVENT("gpu", "patch_siblings_optimization");
        MoveSibling(overlay_id, overlay.parent_layer_id, below_id);
      }
    } else {
      DVLOG(2) << "| Skipped layer: " << PrintOverlayId(overlay_id)
               << ", parent = " << PrintOverlayId(overlay.parent_layer_id)
               << ", below = " << PrintOverlayId(below_id);
    }
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  if (UsingPatchSiblingsOptimization()) {
    for (const auto& [id, siblings] : expected_siblings) {
      CHECK(siblings.parent == siblings_[id].parent ||
            siblings.below == siblings_[id].below ||
            siblings.above == siblings_[id].above)
          << "\nCalculated siblings don't match reality:" << "\n  expected: "
          << base::StringPrintf("parent = %s, below = %s, above = %s",
                                PrintOverlayId(siblings.parent),
                                PrintOverlayId(siblings.below),
                                PrintOverlayId(siblings.above))
          << "\n    actual: "
          << base::StringPrintf("parent = %s, below = %s, above = %s",
                                PrintOverlayId(siblings_[id].parent),
                                PrintOverlayId(siblings_[id].below),
                                PrintOverlayId(siblings_[id].above));
    }
    CHECK_EQ(expected_siblings.size(), siblings_.size());
  }
#endif

  DVLOG(1) << "layers: modified = " << num_layers_modified
           << " / total = " << layers_.size();
  num_layers_modified_last_frame_ = num_layers_modified;

  if (num_layers_modified > 0) {
    TRACE_EVENT("gpu", "Commit overlay layers");
    return CommitTree();
  }

  return true;
}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
Layer* OsCompositorTreeBase<OverlayParams, Layer>::GetLayer(
    const std::optional<gfx::OverlayLayerId>& overlay_id) const {
  if (overlay_id) {
    if (auto it = layers_.find(overlay_id.value()); it != layers_.end()) {
      return it->second.get();
    }
  }
  return nullptr;
}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
OsCompositorTreeBase<OverlayParams, Layer>::Siblings::Siblings() = default;
template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
OsCompositorTreeBase<OverlayParams, Layer>::Siblings::~Siblings() = default;

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
void OsCompositorTreeBase<OverlayParams, Layer>::PatchSiblingsTogether(
    const std::optional<gfx::OverlayLayerId>& overlay_id,
    const std::optional<gfx::OverlayLayerId>& below_id) {
  if (below_id && overlay_id) {
    // Ensure we're not setting a sibling as below itself.
    CHECK_NE(below_id.value(), overlay_id.value());
  }

  if (overlay_id) {
    siblings_[overlay_id.value()].below = below_id;
  }

  if (below_id) {
    siblings_[below_id.value()].above = overlay_id;
  }

  // Log both operations on a single line, for easier reading.
  if (overlay_id && below_id) {
    DVLOG(3) << PrintOverlayId(overlay_id)
             << ".below = " << PrintOverlayId(below_id) << ", "
             << PrintOverlayId(below_id)
             << ".above = " << PrintOverlayId(overlay_id);
  } else if (overlay_id && !below_id) {
    DVLOG(3) << PrintOverlayId(overlay_id)
             << ".below = " << PrintOverlayId(std::nullopt);
  } else if (!overlay_id && below_id) {
    DVLOG(3) << PrintOverlayId(below_id)
             << ".above = " << PrintOverlayId(std::nullopt);
  }
}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
void OsCompositorTreeBase<OverlayParams, Layer>::RemoveSibling(
    const gfx::OverlayLayerId& overlay_id) {
  auto it = siblings_.find(overlay_id);
  if (it != siblings_.end()) {
    DVLOG(3) << "RemoveSibling(" << PrintOverlayId(overlay_id) << ")";

    if (auto* above_layer = GetLayer(it->second.above)) {
      above_layer->unsafe_set_below_layer(GetLayer(it->second.below));
      DVLOG(3) << PrintOverlayId(it->second.above) << ".unsafe_set_below_layer("
               << PrintOverlayId(it->second.below) << ")";
    }
    PatchSiblingsTogether(it->second.above, it->second.below);

    siblings_.erase(it);
  } else {
    // Not in frame, nothing to patch.
  }
}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
void OsCompositorTreeBase<OverlayParams, Layer>::MoveSibling(
    const gfx::OverlayLayerId& overlay_id,
    const gfx::OverlayLayerId& new_parent_id,
    const std::optional<gfx::OverlayLayerId>& new_below_id) {
  auto it = siblings_.find(overlay_id);
  if (it != siblings_.end()) {
    const Siblings& prev_siblings = it->second;
    if (prev_siblings.parent == new_parent_id &&
        prev_siblings.below == new_below_id) {
      DVLOG(3) << "Skipping updating siblings for: "
               << PrintOverlayId(overlay_id)
               << " since parent and below stayed the same";
      return;
    }
  }

  // Remove ourselves from the source of our move, patching our above and
  // below siblings together.
  RemoveSibling(overlay_id);

  DVLOG(3) << "InsertSibling(" << PrintOverlayId(overlay_id) << ", "
           << PrintOverlayId(new_parent_id) << ", "
           << PrintOverlayId(new_below_id) << ")";
  // Update our "above" relationship: find the layer that was previously
  // above what we're above now, and update our relationship to it.
  auto above_it = std::ranges::find_if(
      siblings_, [&parent_id = new_parent_id, &new_below_id](const auto& kv) {
        // The parent needs to match, since multiple overlays can have
        // their "below" point to nullopt, meaning they are below all
        // their siblings.
        return kv.second.parent == parent_id && kv.second.below == new_below_id;
      });
  if (above_it != siblings_.end()) {
    const gfx::OverlayLayerId& new_above_id = above_it->first;
    auto* above_layer = GetLayer(new_above_id);
    CHECK(above_layer);
    above_layer->unsafe_set_below_layer(GetLayer(overlay_id));
    DVLOG(3) << PrintOverlayId(new_above_id) << ".unsafe_set_below_layer("
             << PrintOverlayId(overlay_id) << ")";

    PatchSiblingsTogether(new_above_id, overlay_id);
  } else {
    // Already at the bottom of the layer, no need to update our "above"
    // relationship.
  }

  // Update our "below" relationship. Our below layer should already be set.
  PatchSiblingsTogether(overlay_id, new_below_id);

  // Note our "parent" relationship even though the overlay IDs should be
  // unique because the nullopt value can be used by multiple layers for
  // their "below" and "above".
  siblings_[overlay_id].parent = new_parent_id;
  DVLOG(3) << PrintOverlayId(overlay_id)
           << ".parent = " << PrintOverlayId(new_parent_id);
}

template <HasParentLayerId OverlayParams, OsCompositorVisualsWrapper Layer>
std::string OsCompositorTreeBase<OverlayParams, Layer>::PrintOverlayId(
    const std::optional<gfx::OverlayLayerId>& overlay_id) {
  if (overlay_id) {
    return base::StringPrintf("%s", overlay_id->ToString());
  } else {
    return "(null)";
  }
}

}  // namespace gl

#endif  // UI_GL_OS_COMPOSITOR_TREE_BASE_H_

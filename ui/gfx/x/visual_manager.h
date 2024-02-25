// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_VISUAL_MANAGER_H_
#define UI_GFX_X_VISUAL_MANAGER_H_

#include <unordered_map>

#include "base/component_export.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Selects a visual with a preference for alpha support on compositing window
// managers.
class COMPONENT_EXPORT(X11) VisualManager : public EventObserver {
 public:
  explicit VisualManager(Connection* connection);

  VisualManager(const VisualManager&) = delete;
  VisualManager& operator=(const VisualManager&) = delete;

  ~VisualManager() override;

  // Picks the best argb or opaque visual given |want_argb_visual|.
  void ChooseVisualForWindow(bool want_argb_visual,
                             x11::VisualId* visual_id,
                             uint8_t* depth,
                             x11::ColorMap* colormap,
                             bool* visual_has_alpha);

  bool GetVisualInfo(x11::VisualId visual_id,
                     uint8_t* depth,
                     x11::ColorMap* colormap,
                     bool* visual_has_alpha);

  // Are all of the system requirements met for using transparent visuals?
  bool ArgbVisualAvailable() const;

 private:
  class XVisualData {
   public:
    XVisualData(x11::Connection* connection,
                uint8_t depth,
                const x11::VisualType* info);
    ~XVisualData();

    x11::ColorMap GetColormap(x11::Connection* connection);

    const uint8_t depth;
    const raw_ptr<const x11::VisualType> info;

   private:
    x11::ColorMap colormap_{};
  };

  // EventObserver:
  void OnEvent(const x11::Event& event) override;

  const raw_ptr<x11::Connection> connection_;

  Atom compositor_atom_ = x11::Atom::None;
  Window compositor_owner_ = x11::Window::None;

  std::unordered_map<x11::VisualId, std::unique_ptr<XVisualData>> visuals_;

  x11::VisualId opaque_visual_id_{};
  x11::VisualId transparent_visual_id_{};
};

}  // namespace x11

#endif  // UI_GFX_X_VISUAL_MANAGER_H_

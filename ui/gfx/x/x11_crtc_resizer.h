// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_CRTC_RESIZER_H_
#define UI_GFX_X_X11_CRTC_RESIZER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/randr.h"

namespace x11 {

// Helper class for DesktopResizerX11 which handles much of the logic
// for arranging and resizing a set of active CRTCs.
class COMPONENT_EXPORT(X11) X11CrtcResizer {
 public:
  X11CrtcResizer(x11::RandR::GetScreenResourcesCurrentReply* resources,
                 x11::Connection* connection);
  X11CrtcResizer(const X11CrtcResizer&) = delete;
  X11CrtcResizer& operator=(const X11CrtcResizer&) = delete;
  ~X11CrtcResizer();

  // Queries the server for all active CRTCs and stores them in |active_crtcs_|.
  void FetchActiveCrtcs();

  // Searches |active_crts_| to find the one for the given output. If none is
  // found, this returns kDisabledCrtc. Since the information on all CRTCs is
  // already fetched, this method avoids a server round-trip from using
  // RRGetOutputInfo.
  x11::RandR::Crtc GetCrtcForOutput(x11::RandR::Output output) const;

  // Disables a CRTC. A disabled CRTC no longer has a mode selected (allowing
  // the CRD mode to be removed). It also no longer occupies space in the root
  // window, which may allow the root window to be resized. This does not
  // modify |active_crtcs_|, so the stored information can be used to enable
  // the CRTC again.
  void DisableCrtc(x11::RandR::Crtc crtc);

  // This operates only on |active_crtcs_| without making any X server calls.
  // It sets the new mode and width/height for the given CRTC. And it changes
  // any xy-offsets as needed, to avoid overlaps between CRTCs.
  // This method calls NormalizeCrtcs() so you don't need to call it manually.
  void UpdateActiveCrtcs(x11::RandR::Crtc crtc,
                         x11::RandR::Mode mode,
                         const gfx::Size& new_size);

  // This operates only on |active_crtcs_| without making any X server calls.
  // It sets the new mode, offsets and width/height for the given CRTC. Unlike
  // UpdateActiveCrtcs, this method does not change any xy-offsets to avoid
  // overlaps.
  void UpdateActiveCrtc(x11::RandR::Crtc crtc,
                        x11::RandR::Mode mode,
                        const gfx::Rect& new_rect);

  // Adds a new CRTC to |active_crtcs_|. This method does not make any X server
  // calls, and it does not change xy-offsets of any existing CRTCs.
  void AddActiveCrtc(x11::RandR::Crtc crtc,
                     x11::RandR::Mode mode,
                     const std::vector<x11::RandR::Output>& outputs,
                     const gfx::Rect& new_rect);

  // Removes a CRTC from |active_crtcs_|. This method does not make any X server
  // calls.
  void RemoveActiveCrtc(x11::RandR::Crtc crtc);

  // Disables any CRTCs whose offsets are being changed. The caller is
  // responsible for disabling the CRTC being resized, so there is no need to
  // do it here.
  void DisableChangedCrtcs();

  // Computes the bounding-box of the CRTCs after active CRTCs have been
  // modified. If the bounding-box is not aligned at the origin, all the CRTC
  // offsets are shifted the same amount so that the new bounding-box's top-left
  // corner is at (0, 0).
  void NormalizeCrtcs();

  // This should be called after a new layout is computed and normalized, just
  // before applying the changes to the X server. This moves any visible
  // windows, to try to keep them in the same relative positions to any moved
  // CRTCs.
  void MoveApplicationWindows();

  // Returns the bounding box of |active_crtcs_| from their current xy-offsets
  // and sizes.
  // NormalizeCrtcs() needs to be called first if (Update|Add|Remove)ActiveCrtc
  // has been called (not including UpdateActiveCrtcs which takes a DesktopSize
  // instead of DesktopRect).
  gfx::Size GetBoundingBox() const;

  // Applies any changed CRTCs back to the X server. This will re-enable any
  // outputs/CRTCs that were disabled.
  void ApplyActiveCrtcs();

  // Updates the root window using the bounding box of the CRTCs, then
  // re-activate all CRTCs.
  void UpdateRootWindow(x11::Window root);

  // Initializes the active CRTCs from a list of fake X11 replies. As the
  // replies don't include the CRTC IDs, these will be created sequentially
  // as 1, 2, ...
  void SetCrtcsForTest(std::vector<x11::RandR::GetCrtcInfoReply> crtcs);

  // Returns the stored CRTCs as a list of rectangles.
  std::vector<gfx::Rect> GetCrtcsForTest() const;

 private:
  // Information for an active CRTC, from RRGetCrtcInfo response. When
  // modifying a CRTC, the information here can reconstruct the original
  // properties that should not be changed.
  struct CrtcInfo {
    CrtcInfo();
    CrtcInfo(x11::RandR::Crtc crtc,
             int16_t x,
             int16_t y,
             uint16_t width,
             uint16_t height,
             x11::RandR::Mode mode,
             x11::RandR::Rotation rotation,
             const std::vector<x11::RandR::Output>& outputs);
    CrtcInfo(const CrtcInfo&);
    CrtcInfo(CrtcInfo&&);
    CrtcInfo& operator=(const CrtcInfo&);
    CrtcInfo& operator=(CrtcInfo&&);
    ~CrtcInfo();

    // Returns whether {x, y} are different from the original values.
    bool OffsetsChanged() const;

    x11::RandR::Crtc crtc;
    int16_t old_x;
    int16_t x;
    int16_t old_y;
    int16_t y;
    uint16_t width;
    uint16_t height;
    x11::RandR::Mode mode;
    x11::RandR::Rotation rotation;
    std::vector<x11::RandR::Output> outputs;
  };

  // Adds a new active CRTC from a reply to RRGetCrtcInfo.
  void AddCrtcFromReply(x11::RandR::Crtc crtc,
                        const x11::RandR::GetCrtcInfoReply& reply);

  // Adjusts CRTC offsets to accommodate the new size of the CRTC. This should
  // position the CRTCs such that they do not overlap after the CRTC is given
  // its new size. The implementation may set negative xy-coordinates, or it
  // may leave a gap between all CRTCs and the top (or left) desktop edge.
  // NormalizeCrtcs() will be called afterwards to fix both these issues.
  // The implementation may also re-order |active_crtcs_|, for example, a
  // stacking-algorithm may want to sort the list first.
  // |crtc_to_resize| points to the CRTC being resized.
  // |new_size| is the new size of |crtc_to_resize|. This method is responsible
  // for setting the new width/height values of the CRTC.
  void RelayoutCrtcs(x11::RandR::Crtc crtc_to_resize,
                     const gfx::Size& new_size);

  // Returns true if the CRTCs appear to be roughly laid out vertically.
  bool LayoutIsVertical() const;

  // Stacks the CRTCs vertically without gaps. The CRTC being resized should
  // have the old width/height, so that right-alignment can be detected and
  // preserved. If they are not all right-aligned, this will position the
  // monitors against the left edge of the desktop.
  // On return, the CRTC being resized will have the new width/height.
  void PackVertically(const gfx::Size& new_size, x11::RandR::Crtc resized_crtc);

  // Behaves similarly, but stacks the CRTCs horizontally.
  void PackHorizontally(const gfx::Size& new_size,
                        x11::RandR::Crtc resized_crtc);

  // Transposes all the CRTCs by swapping x and y coordinates. This allows
  // the vertical layout code to be re-used for horizontal layout.
  void Transpose();

  // Moves an application window. This tries to account for any re-parenting
  // done by a window-manager.
  // |window| is the top-level window to move (a child of the root window),
  // which is possibly a window-manager frame.
  // |attributes| is the response from GetWindowAttributes(window...). It is
  // used here to avoid fetching the attributes again inside this method,
  // as the caller already has them.
  // |top_left| is the requested top-left corner of the new window position.
  void MoveWindow(x11::Window window,
                  const x11::GetWindowAttributesReply& attributes,
                  gfx::Point top_left);

  // Helper method to locate an application window which was re-parented by the
  // window-manager. It looks at |window| and its children recursively, in
  // stacking order, and returns a viewable window which has the "WM_STATE"
  // property. If not found, this returns x11::Window::None.
  x11::Window FindAppWindow(x11::Window window,
                            const x11::GetWindowAttributesReply& attributes);

  x11::RandR::GetScreenResourcesCurrentReply resources_;
  raw_ptr<x11::Connection> connection_;
  raw_ptr<x11::RandR> randr_;

  // Information on all CRTCs that are currently enabled (including the CRTC
  // being resized). This is only used during a resize operation, while the X
  // server is grabbed. Some of these xy-positions may be adjusted. At the end,
  // the root window size will be set to the bounding rectangle of all these
  // CRTCs. If a CRTC needs to be disabled temporarily, the list entry will be
  // preserved so that the CRTC can be re-enabled with the original and new
  // properties.
  std::vector<CrtcInfo> active_crtcs_;

  // Stores the CRTCs being updated. This is needed so that ApplyActiveCrtcs()
  // will set the CRTCs' new sizes in the X server, even if their xy-offsets are
  // unchanged.
  base::flat_set<x11::RandR::Crtc> updated_crtcs_;

  // Bounding-box size computed by NormalizeCrtcs().
  gfx::Size bounding_box_size_;

  x11::Atom wm_state_atom_;
};

}  // namespace x11

#endif  // UI_GFX_X_X11_CRTC_RESIZER_H_

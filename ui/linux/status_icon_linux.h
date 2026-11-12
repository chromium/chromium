// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_STATUS_ICON_LINUX_H_
#define UI_LINUX_STATUS_ICON_LINUX_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}

namespace ui {
class MenuModel;
}  // namespace ui

namespace ui {

// Since linux_ui cannot have dependencies on any chrome browser components
// we cannot inherit from StatusIcon. So we implement the necessary methods
// and let a wrapper class implement the StatusIcon interface and defer the
// callbacks to a delegate. For the same reason, do not use StatusIconMenuModel.
class COMPONENT_EXPORT(LINUX_UI) StatusIconLinux {
 public:
  class Delegate {
   public:
    virtual void OnClick() = 0;
    virtual bool HasClickAction() = 0;

    virtual const gfx::ImageSkia& GetImage() const = 0;
    virtual const gfx::VectorIcon* GetIcon() const = 0;
    virtual const std::u16string& GetToolTip() const = 0;
    virtual ui::MenuModel* GetMenuModel() const = 0;

    // This should be called at most once by the implementation.
    virtual void OnImplInitializationFailed() = 0;

    // Called at most once by an implementation that can't show the icon yet,
    // but keeps waiting until it can (e.g. while no StatusNotifierWatcher is
    // running). The delegate may show a fallback icon until
    // OnImplInitialized() is called. Until then, the implementation doesn't
    // call OnImplInitializationFailed().
    virtual void OnImplInitializationDelayed() = 0;

    // Called by the implementation each time it has registered the icon.
    virtual void OnImplInitialized() = 0;

   protected:
    virtual ~Delegate();
  };

  StatusIconLinux();
  virtual ~StatusIconLinux();

  virtual void SetImage(const gfx::ImageSkia& image) = 0;
  virtual void SetIcon(const gfx::VectorIcon& icon) = 0;
  virtual void SetToolTip(const std::u16string& tool_tip) = 0;

  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. The
  // subclass should destroy the existing native context menu on this call.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* model) = 0;

  // Update all the enabled/checked states and the dynamic labels. Some status
  // icon implementations do not refresh the native menu before showing so we
  // need to manually refresh it when the menu model changes.
  virtual void RefreshPlatformContextMenu();

  virtual void OnSetDelegate();

  void SetDelegate(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 protected:
  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace ui

#endif  // UI_LINUX_STATUS_ICON_LINUX_H_

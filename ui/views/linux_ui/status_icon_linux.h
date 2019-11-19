// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_STATUS_ICON_LINUX_H_
#define UI_VIEWS_LINUX_UI_STATUS_ICON_LINUX_H_

#include "base/strings/string16.h"
#include "ui/views/views_export.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {

// Since liblinux_ui cannot have dependencies on any chrome browser components
// we cannot inherit from StatusIcon. So we implement the necessary methods
// and let a wrapper class implement the StatusIcon interface and defer the
// callbacks to a delegate. For the same reason, do not use StatusIconMenuModel.
class VIEWS_EXPORT StatusIconLinux {
 public:
  class Delegate {
   public:
    virtual void OnClick() = 0;
    virtual bool HasClickAction() = 0;

    virtual const gfx::ImageSkia& GetImage() const = 0;
    virtual const base::string16& GetToolTip() const = 0;
    virtual ui::MenuModel* GetMenuModel() const = 0;

    // This should be called at most once by the implementation.
    virtual void OnImplInitializationFailed() = 0;

   protected:
    virtual ~Delegate();
  };

  StatusIconLinux();
  virtual ~StatusIconLinux();

  virtual void SetIcon(const gfx::ImageSkia& image) = 0;
  virtual void SetToolTip(const base::string16& tool_tip) = 0;

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
  Delegate* delegate_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_STATUS_ICON_LINUX_H_

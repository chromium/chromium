// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/shell.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "weblayer/public/tab.h"

#if defined(USE_AURA)
#include "ui/wm/core/wm_state.h"
#endif

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace weblayer {

namespace {

// Maintain the UI controls and web view for web shell
class ShellWindowDelegateView : public views::WidgetDelegateView,
                                public views::TextfieldController,
                                public views::ButtonListener {
 public:
  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  ShellWindowDelegateView(Shell* shell) : shell_(shell) {}

  ~ShellWindowDelegateView() override {}

  // Update the state of UI controls
  void SetAddressBarURL(const GURL& url) {
    url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
  }

  void AttachTab(Tab* tab, const gfx::Size& size) {
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    // If there was a previous WebView in this Shell it should be removed and
    // deleted.
    if (web_view_) {
      contents_view_->RemoveChildView(web_view_);
      delete web_view_;
    }
    auto web_view = std::make_unique<views::WebView>(nullptr);
    tab->AttachToView(web_view.get());
    web_view->SetPreferredSize(size);
    web_view_ = contents_view_->AddChildView(std::move(web_view));
    Layout();

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);
  }

  void SetWindowTitle(const base::string16& title) { title_ = title; }

  void EnableUIControl(UIControl control, bool is_enabled) {
    if (control == BACK_BUTTON) {
      back_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
    } else if (control == FORWARD_BUTTON) {
      forward_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                           : views::Button::STATE_DISABLED);
    } else if (control == STOP_BUTTON) {
      stop_button_->SetState(is_enabled ? views::Button::STATE_NORMAL
                                        : views::Button::STATE_DISABLED);
      if (!is_enabled)
        UpdateLoadProgress();
    }
  }

  void UpdateLoadProgress(double progress = 0.) {
    std::string stop_text("Stop");
    if (stop_button_->state() == views::Button::STATE_NORMAL)
      stop_text = base::StringPrintf("Stop (%.0f%%)", progress * 100);
    stop_button_->SetText(base::ASCIIToUTF16(stop_text));
  }

 private:
  // Initialize the UI control contained in shell window
  void InitShellWindow() {
    SetBackground(views::CreateStandardPanelBackground());

    auto contents_view = std::make_unique<views::View>();
    auto toolbar_view = std::make_unique<views::View>();

    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());

    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddPaddingColumn(0, 2);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, 2);

    // Add toolbar buttons and URL text field
    layout->AddPaddingRow(0, 2);
    layout->StartRow(0, 0);
    views::GridLayout* toolbar_layout =
        toolbar_view->SetLayoutManager(std::make_unique<views::GridLayout>());

    views::ColumnSet* toolbar_column_set = toolbar_layout->AddColumnSet(0);
    // Back button
    auto back_button =
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Back"));
    gfx::Size back_button_size = back_button->GetPreferredSize();
    toolbar_column_set->AddColumn(
        views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
        views::GridLayout::FIXED, back_button_size.width(),
        back_button_size.width() / 2);
    // Forward button
    auto forward_button =
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Forward"));
    gfx::Size forward_button_size = forward_button->GetPreferredSize();
    toolbar_column_set->AddColumn(
        views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
        views::GridLayout::FIXED, forward_button_size.width(),
        forward_button_size.width() / 2);
    // Refresh button
    auto refresh_button =
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Refresh"));
    gfx::Size refresh_button_size = refresh_button->GetPreferredSize();
    toolbar_column_set->AddColumn(
        views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
        views::GridLayout::FIXED, refresh_button_size.width(),
        refresh_button_size.width() / 2);
    // Stop button
    auto stop_button =
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Stop (100%)"));
    int stop_button_width = stop_button->GetPreferredSize().width();
    toolbar_column_set->AddColumn(
        views::GridLayout::FILL, views::GridLayout::CENTER, 0,
        views::GridLayout::FIXED, stop_button_width, stop_button_width / 2);
    toolbar_column_set->AddPaddingColumn(0, 2);
    // URL entry
    auto url_entry = std::make_unique<views::Textfield>();
    url_entry->SetAccessibleName(base::ASCIIToUTF16("Enter URL"));
    url_entry->set_controller(this);
    url_entry->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_URL);
    toolbar_column_set->AddColumn(views::GridLayout::FILL,
                                  views::GridLayout::FILL, 1,
                                  views::GridLayout::USE_PREF, 0, 0);
    toolbar_column_set->AddPaddingColumn(0, 2);

    // Fill up the first row
    toolbar_layout->StartRow(0, 0);
    back_button_ = toolbar_layout->AddView(std::move(back_button));
    forward_button_ = toolbar_layout->AddView(std::move(forward_button));
    refresh_button_ = toolbar_layout->AddView(std::move(refresh_button));
    stop_button_ = toolbar_layout->AddView(std::move(stop_button));
    url_entry_ = toolbar_layout->AddView(std::move(url_entry));

    toolbar_view_ = layout->AddView(std::move(toolbar_view));

    layout->AddPaddingRow(0, 5);

    // Add WebBrowser view as the second row
    {
      layout->StartRow(1, 0);
      contents_view_ = layout->AddView(std::move(contents_view));
    }

    layout->AddPaddingRow(0, 5);

    InitAccelerators();
  }

  void InitAccelerators() {
    static const ui::KeyboardCode keys[] = {ui::VKEY_F5, ui::VKEY_BROWSER_BACK,
                                            ui::VKEY_BROWSER_FORWARD};
    for (size_t i = 0; i < base::size(keys); ++i) {
      GetFocusManager()->RegisterAccelerator(
          ui::Accelerator(keys[i], ui::EF_NONE),
          ui::AcceleratorManager::kNormalPriority, this);
    }
  }

  // Overridden from TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {}

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() == ui::ET_KEY_PRESSED && sender == url_entry_ &&
        key_event.key_code() == ui::VKEY_RETURN) {
      std::string text = base::UTF16ToUTF8(url_entry_->GetText());
      GURL url(text);
      if (!url.has_scheme()) {
        url = GURL(std::string("http://") + std::string(text));
        url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
      }
      shell_->LoadURL(url);
      return true;
    }
    return false;
  }

  // Overridden from ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == back_button_)
      shell_->GoBackOrForward(-1);
    else if (sender == forward_button_)
      shell_->GoBackOrForward(1);
    else if (sender == refresh_button_)
      shell_->Reload();
    else if (sender == stop_button_)
      shell_->Stop();
  }

  // Overridden from WidgetDelegateView
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  base::string16 GetWindowTitle() const override { return title_; }
  void WindowClosing() override {
    if (shell_) {
      delete shell_;
      shell_ = nullptr;
    }
  }

  // Overridden from View
  gfx::Size GetMinimumSize() const override {
    // We want to be able to make the window smaller than its initial
    // (preferred) size.
    return gfx::Size();
  }
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      InitShellWindow();
    }
  }

  // Overridden from AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    switch (accelerator.key_code()) {
      case ui::VKEY_F5:
        shell_->Reload();
        return true;
      case ui::VKEY_BROWSER_BACK:
        shell_->GoBackOrForward(-1);
        return true;
      case ui::VKEY_BROWSER_FORWARD:
        shell_->GoBackOrForward(1);
        return true;
      default:
        return views::WidgetDelegateView::AcceleratorPressed(accelerator);
    }
  }

 private:
  // Hold a reference of Shell for deleting it when the window is closing
  Shell* shell_;

  // Window title
  base::string16 title_;

  // Toolbar view contains forward/backward/reload button and URL entry
  View* toolbar_view_ = nullptr;
  views::Button* back_button_ = nullptr;
  views::Button* forward_button_ = nullptr;
  views::Button* refresh_button_ = nullptr;
  views::MdTextButton* stop_button_ = nullptr;
  views::Textfield* url_entry_ = nullptr;

  // Contents view contains the WebBrowser view
  View* contents_view_ = nullptr;
  views::WebView* web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowDelegateView);
};

}  // namespace

#if defined(USE_AURA)
// static
wm::WMState* Shell::wm_state_ = nullptr;
#endif
// static
views::ViewsDelegate* Shell::views_delegate_ = nullptr;

// static
void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
#if defined(OS_WIN)
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  wm_state_ = new wm::WMState;
  views::InstallDesktopScreenIfNecessary();
  views_delegate_ = new views::DesktopTestViewsDelegate();
}

void Shell::PlatformExit() {
  delete views_delegate_;
  views_delegate_ = nullptr;
  // delete platform_;
  // platform_ = nullptr;
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  delete wm_state_;
  wm_state_ = nullptr;
#endif
}

void Shell::PlatformCleanUp() {}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  if (control == BACK_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::BACK_BUTTON,
                                   is_enabled);
  } else if (control == FORWARD_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::FORWARD_BUTTON,
                                   is_enabled);
  } else if (control == STOP_BUTTON) {
    delegate_view->EnableUIControl(ShellWindowDelegateView::STOP_BUTTON,
                                   is_enabled);
  }
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->SetAddressBarURL(url);
}

void Shell::PlatformSetLoadProgress(double progress) {
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->UpdateLoadProgress(progress);
}

void Shell::PlatformCreateWindow(int width, int height) {
  window_widget_ = new views::Widget;
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(0, 0, width, height);
  params.delegate = new ShellWindowDelegateView(this);
  params.wm_class_class = "chromium-web_shell";
  params.wm_class_name = params.wm_class_class;
  window_widget_->Init(std::move(params));

  content_size_ = gfx::Size(width, height);

  // |window_widget_| is made visible in PlatformSetContents(), so that the
  // platform-window size does not need to change due to layout again.
  window_ = window_widget_->GetNativeWindow();
}

void Shell::PlatformSetContents() {
  views::WidgetDelegate* widget_delegate = window_widget_->widget_delegate();
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(widget_delegate);
  delegate_view->AttachTab(tab_.get(), content_size_);
  window_->GetHost()->Show();
  window_widget_->Show();
}

void Shell::PlatformResizeSubViews() {}

void Shell::Close() {
  window_widget_->CloseNow();
}

void Shell::PlatformSetTitle(const base::string16& title) {
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->SetWindowTitle(title);
  window_widget_->UpdateWindowTitle();
}

}  // namespace weblayer

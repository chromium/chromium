// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace weblayer {

namespace {

// Maintain the UI controls and web view for web shell
class ShellWindowDelegateView : public views::WidgetDelegateView,
                                public views::TextfieldController {
 public:
  METADATA_HEADER(ShellWindowDelegateView);

  enum UIControl { BACK_BUTTON, FORWARD_BUTTON, STOP_BUTTON };

  explicit ShellWindowDelegateView(Shell* shell) : shell_(shell) {
    SetHasWindowSizeControls(true);
    InitShellWindow();
  }

  ShellWindowDelegateView(const ShellWindowDelegateView&) = delete;
  ShellWindowDelegateView& operator=(const ShellWindowDelegateView&) = delete;

  ~ShellWindowDelegateView() override = default;

  // Update the state of UI controls
  void SetAddressBarURL(const GURL& url) {
    url_entry_->SetText(base::ASCIIToUTF16(url.spec()));
  }

  void AttachTab(Tab* tab, const gfx::Size& size) {
    contents_view_->SetUseDefaultFillLayout(true);
    // If there was a previous WebView in this Shell it should be removed and
    // deleted.
    if (web_view_)
      contents_view_->RemoveChildViewT(web_view_.get());

    views::Builder<views::View>(contents_view_.get())
        .AddChild(views::Builder<views::WebView>()
                      .CopyAddressTo(&web_view_)
                      .SetPreferredSize(size))
        .BuildChildren();
    tab->AttachToView(web_view_);
    web_view_->SizeToPreferredSize();

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);
  }

  void SetWindowTitle(const std::u16string& title) { title_ = title; }

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
    if (stop_button_->GetState() == views::Button::STATE_NORMAL)
      stop_text = base::StringPrintf("Stop (%.0f%%)", progress * 100);
    stop_button_->SetText(base::ASCIIToUTF16(stop_text));
  }

 private:
  // Initialize the UI control contained in shell window
  void InitShellWindow() {
    auto toolbar_button_rule = [](const views::View* view,
                                  const views::SizeBounds& size_bounds) {
      gfx::Size preferred_size = view->GetPreferredSize();
      if (size_bounds != views::SizeBounds() &&
          size_bounds.width().is_bounded()) {
        preferred_size.set_width(std::max(
            std::min(size_bounds.width().value(), preferred_size.width()),
            preferred_size.width() / 2));
      }
      return preferred_size;
    };

    auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));

    views::Builder<views::WidgetDelegateView>(this)
        .SetBackground(
            views::CreateThemedSolidBackground(ui::kColorWindowBackground))
        .AddChildren(
            views::Builder<views::FlexLayoutView>()
                .CopyAddressTo(&toolbar_view_)
                .SetOrientation(views::LayoutOrientation::kHorizontal)
                // Top/Left/Right padding = 2, Bottom padding = 5
                .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(2, 2, 5, 2))
                .AddChildren(
                    views::Builder<views::MdTextButton>()
                        .CopyAddressTo(&back_button_)
                        .SetText(u"Back")
                        .SetCallback(base::BindRepeating(
                            &Shell::GoBackOrForward,
                            base::Unretained(shell_.get()), -1))
                        .SetProperty(
                            views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                base::BindRepeating(toolbar_button_rule))),
                    views::Builder<views::MdTextButton>()
                        .CopyAddressTo(&forward_button_)
                        .SetText(u"Forward")
                        .SetCallback(base::BindRepeating(
                            &Shell::GoBackOrForward,
                            base::Unretained(shell_.get()), 1))
                        .SetProperty(
                            views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                base::BindRepeating(toolbar_button_rule))),
                    views::Builder<views::MdTextButton>()
                        .CopyAddressTo(&refresh_button_)
                        .SetText(u"Refresh")
                        .SetCallback(base::BindRepeating(
                            &Shell::Reload, base::Unretained(shell_.get())))
                        .SetProperty(
                            views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                base::BindRepeating(toolbar_button_rule))),
                    views::Builder<views::MdTextButton>()
                        .CopyAddressTo(&stop_button_)
                        .SetText(u"Stop (100%)")
                        .SetCallback(base::BindRepeating(
                            &Shell::Stop, base::Unretained(shell_.get())))
                        .SetProperty(
                            views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                base::BindRepeating(toolbar_button_rule))),
                    views::Builder<views::Textfield>()
                        .CopyAddressTo(&url_entry_)
                        .SetAccessibleName(u"Enter URL")
                        .SetController(this)
                        .SetTextInputType(
                            ui::TextInputType::TEXT_INPUT_TYPE_URL)
                        .SetProperty(
                            views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                views::MinimumFlexSizeRule::kScaleToMinimum,
                                views::MaximumFlexSizeRule::kUnbounded))
                        // Left padding  = 2, Right padding = 2
                        .SetProperty(views::kMarginsKey,
                                     gfx::Insets::TLBR(0, 2, 0, 2))),
            views::Builder<views::View>()
                .CopyAddressTo(&contents_view_)
                .SetUseDefaultFillLayout(true)
                .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 2, 0, 2)),
            views::Builder<views::View>().SetProperty(
                views::kMarginsKey, gfx::Insets::TLBR(0, 0, 5, 0)))
        .BuildChildren();
    box_layout->SetFlexForView(contents_view_, 1);
  }

  void InitAccelerators() {
    // This function must be called when part of the widget hierarchy.
    DCHECK(GetWidget());
    static const ui::KeyboardCode keys[] = {ui::VKEY_F5, ui::VKEY_BROWSER_BACK,
                                            ui::VKEY_BROWSER_FORWARD};
    for (size_t i = 0; i < std::size(keys); ++i) {
      GetFocusManager()->RegisterAccelerator(
          ui::Accelerator(keys[i], ui::EF_NONE),
          ui::AcceleratorManager::kNormalPriority, this);
    }
  }

  // Overridden from TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {}

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

  // Overridden from WidgetDelegateView
  std::u16string GetWindowTitle() const override { return title_; }

  // Overridden from View
  gfx::Size GetMinimumSize() const override {
    // We want to be able to make the window smaller than its initial
    // (preferred) size.
    return gfx::Size();
  }
  void AddedToWidget() override { InitAccelerators(); }

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
  std::unique_ptr<Shell> shell_;

  // Window title
  std::u16string title_;

  // Toolbar view contains forward/backward/reload button and URL entry
  raw_ptr<views::View> toolbar_view_ = nullptr;
  raw_ptr<views::Button> back_button_ = nullptr;
  raw_ptr<views::Button> forward_button_ = nullptr;
  raw_ptr<views::Button> refresh_button_ = nullptr;
  raw_ptr<views::MdTextButton> stop_button_ = nullptr;
  raw_ptr<views::Textfield> url_entry_ = nullptr;

  // Contents view contains the WebBrowser view
  raw_ptr<views::View> contents_view_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
};

BEGIN_METADATA(ShellWindowDelegateView, views::WidgetDelegateView)
END_METADATA

}  // namespace

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
// static
wm::WMState* Shell::wm_state_ = nullptr;
display::Screen* Shell::screen_ = nullptr;
#endif
// static
views::ViewsDelegate* Shell::views_delegate_ = nullptr;

// static
void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
#if BUILDFLAG(IS_WIN)
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
  wm_state_ = new wm::WMState;
  CHECK(!display::Screen::GetScreen());
  screen_ = views::CreateDesktopScreen().release();
#endif
  views_delegate_ = new views::DesktopTestViewsDelegate();
}

void Shell::PlatformExit() {
  delete views_delegate_;
  views_delegate_ = nullptr;
  // delete platform_;
  // platform_ = nullptr;
#if defined(USE_AURA)
  delete screen_;
  screen_ = nullptr;
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
  delegate_view->AttachTab(tab(), content_size_);
  window_->GetHost()->Show();
  window_widget_->Show();
}

void Shell::PlatformResizeSubViews() {}

void Shell::Close() {
  window_widget_->CloseNow();
}

void Shell::PlatformSetTitle(const std::u16string& title) {
  ShellWindowDelegateView* delegate_view =
      static_cast<ShellWindowDelegateView*>(window_widget_->widget_delegate());
  delegate_view->SetWindowTitle(title);
  window_widget_->UpdateWindowTitle();
}

}  // namespace weblayer

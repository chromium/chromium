// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/browser_main_parts.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/examples/browser/browser_context.h"
#include "ui/webui/examples/browser/devtools/devtools_frontend.h"
#include "ui/webui/examples/browser/devtools/devtools_manager_delegate.h"
#include "ui/webui/examples/browser/devtools/devtools_server.h"
#include "ui/webui/examples/browser/ui/aura/aura_context.h"
#include "ui/webui/examples/browser/ui/aura/content_window.h"
#include "ui/webui/examples/browser/webui_controller_factory.h"
#include "ui/webui/examples/grit/webui_examples_resources.h"

namespace webui_examples {

namespace {

class WebContentsViewDelegate : public content::WebContentsViewDelegate {
 public:
  using CreateContentWindowFunc =
      base::RepeatingCallback<content::WebContents*(const GURL&)>;

  WebContentsViewDelegate(content::WebContents* web_contents,
                          CreateContentWindowFunc create_window_func)
      : web_contents_(web_contents),
        create_window_func_(std::move(create_window_func)) {}
  WebContentsViewDelegate(const WebContentsViewDelegate&) = delete;
  WebContentsViewDelegate& operator=(const WebContentsViewDelegate&) = delete;
  ~WebContentsViewDelegate() override = default;

 private:
  // content::WebContentsViewDelegate:
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override {
    DevToolsFrontend* frontend = DevToolsFrontend::CreateAndGet(web_contents_);
    frontend->SetDevtoolsWebContents(
        create_window_func_.Run(frontend->frontend_url()));
  }

  const raw_ptr<content::WebContents> web_contents_;
  CreateContentWindowFunc create_window_func_;
};

}  // namespace

BrowserMainParts::BrowserMainParts() = default;

BrowserMainParts::~BrowserMainParts() = default;

std::unique_ptr<content::WebContentsViewDelegate>
BrowserMainParts::CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<WebContentsViewDelegate>(
      web_contents,
      base::BindRepeating(
          [](BrowserMainParts* browser_main_parts, const GURL& url) {
            return browser_main_parts->CreateAndShowContentWindow(
                url, l10n_util::GetStringUTF16(IDS_DEVTOOLS_WINDOW_TITLE));
          },
          base::Unretained(this)));
}

std::unique_ptr<content::DevToolsManagerDelegate>
BrowserMainParts::CreateDevToolsManagerDelegate() {
  return std::make_unique<DevToolsManagerDelegate>(
      browser_context_.get(),
      base::BindRepeating(
          [](BrowserMainParts* browser_main_parts,
             content::BrowserContext* browser_context, const GURL& url) {
            return browser_main_parts->CreateAndShowContentWindow(
                url, l10n_util::GetStringUTF16(IDS_DEVTOOLS_WINDOW_TITLE));
          },
          base::Unretained(this)));
}

int BrowserMainParts::PreMainMessageLoopRun() {
  std::ignore = temp_dir_.CreateUniqueTempDir();

  browser_context_ = std::make_unique<BrowserContext>(temp_dir_.GetPath());

  devtools::StartHttpHandler(browser_context_.get());

  web_ui_controller_factory_ = std::make_unique<WebUIControllerFactory>();
  content::WebUIControllerFactory::RegisterFactory(
      web_ui_controller_factory_.get());

  aura_context_ = std::make_unique<AuraContext>();

  CreateAndShowContentWindow(
      GURL("chrome://main/"),
      l10n_util::GetStringUTF16(IDS_WEBUI_EXAMPLES_WINDOW_TITLE));

  return 0;
}

void BrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_run_loop_ = run_loop->QuitClosure();
}

void BrowserMainParts::PostMainMessageLoopRun() {
  devtools::StopHttpHandler();
  browser_context_.reset();
}

content::WebContents* BrowserMainParts::CreateAndShowContentWindow(
    GURL url,
    const std::u16string& title) {
  auto content_window = std::make_unique<ContentWindow>(aura_context_.get(),
                                                        browser_context_.get());
  ContentWindow* content_window_ptr = content_window.get();
  content_window_ptr->SetTitle(title);
  content_window_ptr->NavigateToURL(url);
  content_window_ptr->Show();
  content_window_ptr->SetCloseCallback(
      base::BindOnce(&BrowserMainParts::OnWindowClosed,
                     weak_factory_.GetWeakPtr(), std::move(content_window)));
  ++content_windows_outstanding_;
  return content_window_ptr->web_contents();
}

void BrowserMainParts::OnWindowClosed(
    std::unique_ptr<ContentWindow> content_window) {
  --content_windows_outstanding_;
  auto task_runner = content::GetUIThreadTaskRunner({});
  // We are dispatching a callback that originates from the content_window.
  // Deleting soon instead of now eliminates the chance of a crash in case the
  // content_window or associated objects have more work to do after this
  // callback.
  task_runner->DeleteSoon(FROM_HERE, std::move(content_window));
  if (content_windows_outstanding_ == 0) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserMainParts::QuitMessageLoop,
                                         weak_factory_.GetWeakPtr()));
  }
}

void BrowserMainParts::QuitMessageLoop() {
  aura_context_.reset();
  web_ui_controller_factory_.reset();
  quit_run_loop_.Run();
}

}  // namespace webui_examples

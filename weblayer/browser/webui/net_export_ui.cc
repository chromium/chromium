// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webui/net_export_ui.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/net_log/net_export_ui_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/grit/weblayer_resources.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/share/android/intent_helper.h"
#endif

namespace weblayer {

namespace {

class NetExportMessageHandler
    : public content::WebUIMessageHandler,
      public net_log::NetExportFileWriter::StateObserver {
 public:
  NetExportMessageHandler()
      : file_writer_(SystemNetworkContextManager::GetInstance()
                         ->GetNetExportFileWriter()) {
    file_writer_->Initialize();
  }

  NetExportMessageHandler(const NetExportMessageHandler&) = delete;
  NetExportMessageHandler& operator=(const NetExportMessageHandler&) = delete;

  ~NetExportMessageHandler() override { file_writer_->StopNetLog(); }

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        net_log::kEnableNotifyUIWithStateHandler,
        base::BindRepeating(&NetExportMessageHandler::OnEnableNotifyUIWithState,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        net_log::kStartNetLogHandler,
        base::BindRepeating(&NetExportMessageHandler::OnStartNetLog,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        net_log::kStopNetLogHandler,
        base::BindRepeating(&NetExportMessageHandler::OnStopNetLog,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        net_log::kSendNetLogHandler,
        base::BindRepeating(&NetExportMessageHandler::OnSendNetLog,
                            base::Unretained(this)));
  }

  // Messages
  void OnEnableNotifyUIWithState(const base::Value::List& list) {
    AllowJavascript();
    if (!state_observation_manager_.IsObserving()) {
      state_observation_manager_.Observe(file_writer_.get());
    }
    NotifyUIWithState(file_writer_->GetState());
  }

  void OnStartNetLog(const base::Value::List& params) {
    // Determine the capture mode.
    if (!params.empty() && params[0].is_string()) {
      capture_mode_ = net_log::NetExportFileWriter::CaptureModeFromString(
          params[0].GetString());
    }

    // Determine the max file size.
    if (params.size() > 1 && params[1].is_int() && params[1].GetInt() > 0)
      max_log_file_size_ = params[1].GetInt();

    StartNetLog(base::FilePath());
  }

  void OnStopNetLog(const base::Value::List& list) {
    file_writer_->StopNetLog();
  }

  void OnSendNetLog(const base::Value::List& list) {
    file_writer_->GetFilePathToCompletedLog(
        base::BindOnce(&NetExportMessageHandler::SendEmail));
  }

  // net_log::NetExportFileWriter::StateObserver implementation.
  void OnNewState(const base::Value::Dict& state) override {
    NotifyUIWithState(state);
  }

 private:
  // Send NetLog data via email.
  static void SendEmail(const base::FilePath& file_to_send) {
#if BUILDFLAG(IS_ANDROID)
    if (file_to_send.empty())
      return;
    std::string email;
    std::string subject = "WebLayer net_internals_log";
    std::string title = "Issue number: ";
    std::string body =
        "Please add some informative text about the network issues.";
    base::FilePath::StringType file_to_attach(file_to_send.value());
    browser_ui::SendEmail(base::ASCIIToUTF16(email),
                          base::ASCIIToUTF16(subject), base::ASCIIToUTF16(body),
                          base::ASCIIToUTF16(title),
                          base::ASCIIToUTF16(file_to_attach));
#endif
  }

  void StartNetLog(const base::FilePath& path) {
    file_writer_->StartNetLog(
        path, capture_mode_, max_log_file_size_,
        base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
        std::string(),
        web_ui()
            ->GetWebContents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetNetworkContext());
  }

  // Fires net-log-info-changed event to update the JavaScript UI in the
  // renderer.
  void NotifyUIWithState(const base::Value::Dict& state) {
    FireWebUIListener(net_log::kNetLogInfoChangedEvent, state);
  }

  // Cached pointer to SystemNetworkContextManager's NetExportFileWriter.
  raw_ptr<net_log::NetExportFileWriter> file_writer_;

  base::ScopedObservation<net_log::NetExportFileWriter,
                          net_log::NetExportFileWriter::StateObserver>
      state_observation_manager_{this};

  // The capture mode and file size bound that the user chose in the UI when
  // logging started is cached here and is read after a file path is chosen in
  // the save dialog. Their values are only valid while the save dialog is open
  // on the desktop UI.
  net::NetLogCaptureMode capture_mode_ = net::NetLogCaptureMode::kDefault;
  uint64_t max_log_file_size_ = net_log::NetExportFileWriter::kNoLimit;
};

}  // namespace

const char kChromeUINetExportHost[] = "net-export";

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetExportMessageHandler>());

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUINetExportHost);
  source->UseStringsJs();
  source->AddResourcePath(net_log::kNetExportUICSS, IDR_NET_LOG_NET_EXPORT_CSS);
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

NetExportUI::~NetExportUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NetExportUI)

}  // namespace weblayer

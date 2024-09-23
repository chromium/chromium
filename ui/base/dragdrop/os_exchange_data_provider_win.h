// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_

#include <objidl.h>
#include <shlobj.h>
#include <stddef.h>
#include <wrl/client.h>
#include <utility>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

class DataObjectImpl : public DownloadFileObserver,
                       public IDataObject,
                       public IDataObjectAsyncCapability {
 public:
  DataObjectImpl();

  // Accessors.
  void set_in_drag_loop(bool in_drag_loop) { in_drag_loop_ = in_drag_loop; }

  // Number of known formats.
  size_t size() const { return contents_.size(); }

  // DownloadFileObserver implementation:
  void OnDownloadCompleted(const base::FilePath& file_path) override;
  void OnDownloadAborted() override;

  // IDataObject implementation:
  HRESULT __stdcall GetData(FORMATETC* format_etc, STGMEDIUM* medium) override;
  HRESULT __stdcall GetDataHere(FORMATETC* format_etc,
                                STGMEDIUM* medium) override;
  HRESULT __stdcall QueryGetData(FORMATETC* format_etc) override;
  HRESULT __stdcall GetCanonicalFormatEtc(FORMATETC* format_etc,
                                          FORMATETC* result) override;
  HRESULT __stdcall SetData(FORMATETC* format_etc,
                            STGMEDIUM* medium,
                            BOOL should_release) override;
  HRESULT __stdcall EnumFormatEtc(DWORD direction,
                                  IEnumFORMATETC** enumerator) override;
  HRESULT __stdcall DAdvise(FORMATETC* format_etc,
                            DWORD advf,
                            IAdviseSink* sink,
                            DWORD* connection) override;
  HRESULT __stdcall DUnadvise(DWORD connection) override;
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** enumerator) override;

  // IDataObjectAsyncCapability implementation:
  HRESULT __stdcall EndOperation(HRESULT result,
                                 IBindCtx* reserved,
                                 DWORD effects) override;
  HRESULT __stdcall GetAsyncMode(BOOL* is_op_async) override;
  HRESULT __stdcall InOperation(BOOL* in_async_op) override;
  HRESULT __stdcall SetAsyncMode(BOOL do_op_async) override;
  HRESULT __stdcall StartOperation(IBindCtx* reserved) override;

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object) override;
  ULONG __stdcall AddRef() override;
  ULONG __stdcall Release() override;

 private:
  // FormatEtcEnumerator only likes us for our StoredDataMap typedef.
  friend class FormatEtcEnumerator;
  friend class OSExchangeDataProviderWin;

  ~DataObjectImpl() override;

  void StopDownloads();

  // Removes from contents_ the first data that matches |format|.
  void RemoveData(const FORMATETC& format);

  // Our internal representation of stored data & type info.
  struct StoredDataInfo {
   public:
    FORMATETC format_etc;
    STGMEDIUM medium;
    std::unique_ptr<DownloadFileProvider> downloader;

    ~StoredDataInfo();
    StoredDataInfo(const StoredDataInfo&) = delete;
    StoredDataInfo& operator=(const StoredDataInfo&) = delete;

    // Takes ownership of and nullifies `medium` to approximate moving from
    // STGMEDIUM.
    static std::unique_ptr<StoredDataInfo> TakeStorageMedium(
        const FORMATETC& format_etc,
        STGMEDIUM& medium);

   private:
    // STGMEDIUM is just a POD, it does not guarantee `medium` is no longer be
    // used after calling this constructor while the ownership of `medium` is
    // passed.
    StoredDataInfo(const FORMATETC& format_etc, const STGMEDIUM& medium);
  };

  typedef std::vector<std::unique_ptr<StoredDataInfo>> StoredData;
  StoredData contents_;

  Microsoft::WRL::ComPtr<IDataObject> source_object_;

  bool is_aborting_;
  bool in_drag_loop_;
  bool in_async_mode_;
  bool async_operation_started_;
};

class COMPONENT_EXPORT(UI_BASE) OSExchangeDataProviderWin
    : public OSExchangeDataProvider {
 public:
  // Returns true if source has plain text that is a valid url.
  static bool HasPlainTextURL(IDataObject* source);

  // Returns true if source has plain text that is a valid URL and sets url to
  // that url.
  static bool GetPlainTextURL(IDataObject* source, GURL* url);

  static DataObjectImpl* GetDataObjectImpl(const OSExchangeData& data);
  static IDataObject* GetIDataObject(const OSExchangeData& data);

  explicit OSExchangeDataProviderWin(IDataObject* source);
  OSExchangeDataProviderWin();

  OSExchangeDataProviderWin(const OSExchangeDataProviderWin&) = delete;
  OSExchangeDataProviderWin& operator=(const OSExchangeDataProviderWin&) =
      delete;

  ~OSExchangeDataProviderWin() override;

  IDataObject* data_object() const { return data_.get(); }
  IDataObjectAsyncCapability* async_operation() const { return data_.get(); }

  // OSExchangeDataProvider methods.
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;
  void MarkRendererTaintedFromOrigin(const url::Origin& origin) override;
  bool IsRendererTainted() const override;
  std::optional<url::Origin> GetRendererTaintedOrigin() const override;
  void MarkAsFromPrivileged() override;
  bool IsFromPrivileged() const override;
  void SetString(const std::u16string& data) override;
  void SetURL(const GURL& url, const std::u16string& title) override;
  void SetFilename(const base::FilePath& path) override;
  void SetFilenames(const std::vector<FileInfo>& filenames) override;
  // Test only method for adding virtual file content to the data store. The
  // first value in the pair is the file display name, the second is a string
  // providing the file content.
  void SetVirtualFileContentsForTesting(
      const std::vector<std::pair<base::FilePath, std::string>>&
          filenames_and_contents,
      DWORD tymed) override;
  void SetPickledData(const ClipboardFormatType& format,
                      const base::Pickle& data) override;
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents) override;
  void SetHtml(const std::u16string& html, const GURL& base_url) override;

  std::optional<std::u16string> GetString() const override;
  std::optional<UrlInfo> GetURLAndTitle(
      FilenameToURLPolicy policy) const override;
  std::optional<std::vector<GURL>> GetURLs(
      FilenameToURLPolicy policy) const override;
  std::optional<std::vector<FileInfo>> GetFilenames() const override;
  bool HasVirtualFilenames() const override;
  std::optional<std::vector<FileInfo>> GetVirtualFilenames() const override;
  void GetVirtualFilesAsTempFiles(
      base::OnceCallback<
          void(const std::vector<std::pair<base::FilePath, base::FilePath>>&)>
          callback) const override;
  std::optional<base::Pickle> GetPickledData(
      const ClipboardFormatType& format) const override;
  std::optional<FileContentsInfo> GetFileContents() const override;
  std::optional<HtmlInfo> GetHtml() const override;
  bool HasString() const override;
  bool HasURL(FilenameToURLPolicy policy) const override;
  bool HasFile() const override;
  bool HasFileContents() const override;
  bool HasHtml() const override;
  bool HasCustomFormat(const ClipboardFormatType& format) const override;
  void SetDownloadFileInfo(DownloadFileInfo* download_info) override;
  void SetDragImage(const gfx::ImageSkia& image_skia,
                    const gfx::Vector2d& cursor_offset) override;
  gfx::ImageSkia GetDragImage() const override;
  gfx::Vector2d GetDragImageOffset() const override;

  void SetSource(std::unique_ptr<DataTransferEndpoint> data_source) override;
  DataTransferEndpoint* GetSource() const override;

 private:
  void SetVirtualFileContentAtIndexForTesting(base::span<const uint8_t> data,
                                              DWORD tymed,
                                              LONG index);

  scoped_refptr<DataObjectImpl> data_;
  Microsoft::WRL::ComPtr<IDataObject> source_object_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_

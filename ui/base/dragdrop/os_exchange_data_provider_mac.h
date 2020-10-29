// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_

#include <memory>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"

@class NSArray;
@class NSData;
@class NSDraggingItem;
@class NSPasteboard;
@class NSString;

namespace ui {

// OSExchangeDataProvider implementation for Mac.
class COMPONENT_EXPORT(UI_BASE) OSExchangeDataProviderMac
    : public OSExchangeDataProvider {
 public:
  ~OSExchangeDataProviderMac() override;

  // Creates a stand-alone OSExchangeDataProviderMac.
  static std::unique_ptr<OSExchangeDataProviderMac> CreateProvider();

  // Creates an OSExchangeDataProviderMac object wrapping the given NSPasteboard
  // object.
  static std::unique_ptr<OSExchangeDataProviderMac>
  CreateProviderWrappingPasteboard(NSPasteboard* pasteboard);

  // Overridden from OSExchangeDataProvider:
  void MarkOriginatedFromRenderer() override;
  bool DidOriginateFromRenderer() const override;
  void SetString(const base::string16& data) override;
  void SetURL(const GURL& url, const base::string16& title) override;
  void SetFilename(const base::FilePath& path) override;
  void SetFilenames(const std::vector<FileInfo>& filenames) override;
  void SetPickledData(const ClipboardFormatType& format,
                      const base::Pickle& data) override;
  bool GetString(base::string16* data) const override;
  bool GetURLAndTitle(FilenameToURLPolicy policy,
                      GURL* url,
                      base::string16* title) const override;
  bool GetFilename(base::FilePath* path) const override;
  bool GetFilenames(std::vector<FileInfo>* filenames) const override;
  bool GetPickledData(const ClipboardFormatType& format,
                      base::Pickle* data) const override;
  bool HasString() const override;
  bool HasURL(FilenameToURLPolicy policy) const override;
  bool HasFile() const override;
  bool HasCustomFormat(const ClipboardFormatType& format) const override;
  void SetDragImage(const gfx::ImageSkia& image,
                    const gfx::Vector2d& cursor_offset) override;
  gfx::ImageSkia GetDragImage() const override;
  gfx::Vector2d GetDragImageOffset() const override;

  // Gets the underlying pasteboard.
  virtual NSPasteboard* GetPasteboard() const = 0;

  // Returns an NSDraggingItem useful for initiating a drag. (Currently
  // OSExchangeDataProviderMac can only have one item.)
  NSDraggingItem* GetDraggingItem() const;

  // Returns an array of pasteboard types that can be supported by
  // OSExchangeData.
  static NSArray* SupportedPasteboardTypes();

  void SetSource(std::unique_ptr<DataTransferEndpoint> data_source) override;
  DataTransferEndpoint* GetSource() const override;

 protected:
  OSExchangeDataProviderMac();
  OSExchangeDataProviderMac(const OSExchangeDataProviderMac&);
  OSExchangeDataProviderMac& operator=(const OSExchangeDataProviderMac&);

 private:
  // Drag image and offset data.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d cursor_offset_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_

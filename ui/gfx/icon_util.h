// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ICON_UTIL_H_
#define UI_GFX_ICON_UTIL_H_

#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace base {
class FilePath;
}

namespace gfx {
class ImageFamily;
class Size;
}
class SkBitmap;

///////////////////////////////////////////////////////////////////////////////
//
// The IconUtil class contains helper functions for manipulating Windows icons.
// The class interface contains methods for converting an HICON handle into an
// SkBitmap object and vice versa. The class can also create a .ico file given
// a PNG image contained in an SkBitmap object. The following code snippet
// shows an example usage of IconUtil::CreateHICONFromSkBitmap():
//
//   SkBitmap bitmap;
//
//   // Fill |bitmap| with valid data
//   bitmap.setConfig(...);
//   bitmap.allocPixels();
//
//   ...
//
//   // Convert the bitmap into a Windows HICON
//   base::win::ScopedHICON icon(IconUtil::CreateHICONFromSkBitmap(bitmap));
//   if (!icon.is_valid()) {
//     // Handle error
//     ...
//   }
//
//   // Use the icon with a WM_SETICON message
//   ::SendMessage(hwnd, WM_SETICON, static_cast<WPARAM>(ICON_BIG),
//                 reinterpret_cast<LPARAM>(icon.get()));
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT IconUtil {
 public:
  // ATOMIC_WRITE ensures that a partially written icon won't be created even if
  // Chrome crashes part way through, but ATOMIC_WRITE is more expensive than
  // NORMAL_WRITE. See CreateIconFileFromImageFamily. ATOMIC_WRITE is the
  // default for historical reasons.
  enum WriteType { ATOMIC_WRITE, NORMAL_WRITE };
  // The size of the large icon entries in .ico files on Windows Vista+.
  enum { kLargeIconSize = 256 };
  // The size of icons in the medium icons view on Windows Vista+. This is the
  // maximum size Windows will display an icon that does not have a 256x256
  // image, even at the large or extra large icons views.
  enum { kMediumIconSize = 48 };

  // The dimensions for icon images in Windows icon files. All sizes are square;
  // that is, the value 48 means a 48x48 pixel image. Sizes are listed in
  // ascending order.
  static const int kIconDimensions[];

  // The number of elements in kIconDimensions.
  static const size_t kNumIconDimensions;
  // The number of elements in kIconDimensions <= kMediumIconSize.
  static const size_t kNumIconDimensionsUpToMediumSize;

  // Given an SkBitmap object, the function converts the bitmap to a Windows
  // icon and returns the corresponding HICON handle. If the function cannot
  // convert the bitmap, NULL is returned.
  //
  // The client is responsible for destroying the icon when it is no longer
  // needed by calling ::DestroyIcon().
  static base::win::ScopedHICON CreateHICONFromSkBitmap(const SkBitmap& bitmap);

  // Given a valid HICON handle representing an icon, this function converts
  // the icon into an SkBitmap object containing an ARGB bitmap using the
  // dimensions specified in |s|. |s| must specify valid dimensions (both
  // width() an height() must be greater than zero). If the function cannot
  // convert the icon to a bitmap (most probably due to an invalid parameter),
  // the returned SkBitmap's isNull() method will return true.
  static SkBitmap CreateSkBitmapFromHICON(HICON icon, const gfx::Size& s);

  // Loads an icon resource  as a SkBitmap for the specified |size| from a
  // loaded .dll or .exe |module|. Supports loading smaller icon sizes as well
  // as the Vista+ 256x256 PNG icon size. If the icon could not be loaded or
  // found, returns a NULL scoped_ptr.
  static std::unique_ptr<gfx::ImageFamily> CreateImageFamilyFromIconResource(
      HMODULE module,
      int resource_id);

  // Given a valid HICON handle representing an icon, this function converts
  // the icon into an SkBitmap object containing an ARGB bitmap using the
  // dimensions of HICON. If the function cannot convert the icon to a bitmap
  // (most probably due to an invalid parameter), the returned SkBitmap's
  // isNull() method will return true.
  static SkBitmap CreateSkBitmapFromHICON(HICON icon);

  // Creates Windows .ico file at |icon_path|. The icon file is created with
  // multiple BMP representations at varying predefined dimensions (by resizing
  // an appropriately sized image from |image_family|) because Windows uses
  // different image sizes when loading icons, depending on where the icon is
  // drawn (ALT+TAB window, desktop shortcut, Quick Launch, etc.).
  //
  // If |image_family| contains an image larger than 48x48, the resulting icon
  // will contain all sizes up to 256x256. The 256x256 image will be stored in
  // PNG format inside the .ico file. If not, the resulting icon will contain
  // all sizes up to 48x48.
  //
  // The function returns true on success and false otherwise. Returns false if
  // |image_family| is empty.
  static bool CreateIconFileFromImageFamily(
      const gfx::ImageFamily& image_family,
      const base::FilePath& icon_path,
      WriteType write_type = ATOMIC_WRITE);

  // Creates a cursor of the specified size from the SkBitmap passed in.
  // Returns the cursor on success or NULL on failure.
  static base::win::ScopedHICON CreateCursorFromSkBitmap(
      const SkBitmap& bitmap,
      const gfx::Point& hotspot);

  // Given a valid HICON handle representing an icon, this function retrieves
  // the hot spot of the icon.
  static gfx::Point GetHotSpotFromHICON(HICON icon);

 private:
  // The icon format is published in the MSDN but there is no definition of
  // the icon file structures in any of the Windows header files so we need to
  // define these structure within the class. We must make sure we use 2 byte
  // packing so that the structures are laid out properly within the file.
  // See: http://msdn.microsoft.com/en-us/library/ms997538.aspx
#pragma pack(push)
#pragma pack(2)

  // ICONDIRENTRY contains meta data for an individual icon image within a
  // .ico file.
  struct ICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
  };

  // ICONDIR Contains information about all the icon images contained within a
  // single .ico file.
  struct ICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
    ICONDIRENTRY idEntries[1];
  };

  // GRPICONDIRENTRY contains meta data for an individual icon image within a
  // RT_GROUP_ICON resource in an .exe or .dll.
  struct GRPICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD nID;
  };

  // GRPICONDIR Contains information about all the icon images contained within
  // a RT_GROUP_ICON resource in an .exe or .dll.
  struct GRPICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
    GRPICONDIRENTRY idEntries[1];
  };

  // Contains the actual icon image.
  struct ICONIMAGE {
    BITMAPINFOHEADER icHeader;
    RGBQUAD icColors[1];
    BYTE icXOR[1];
    BYTE icAND[1];
  };
#pragma pack(pop)

  friend class IconUtilTest;

  // Returns true if any pixel in the given pixels buffer has an non-zero alpha.
  static bool PixelsHaveAlpha(const uint32_t* pixels, size_t num_pixels);

  // A helper function that initializes a BITMAPV5HEADER structure with a set
  // of values.
  static void InitializeBitmapHeader(BITMAPV5HEADER* header, int width,
                                     int height);

  // Given a single SkBitmap object and pointers to the corresponding icon
  // structures within the icon data buffer, this function sets the image
  // information (dimensions, color depth, etc.) in the icon structures and
  // also copies the underlying icon image into the appropriate location.
  // The width and height of |bitmap| must be < 256.
  // (Note that the 256x256 icon is treated specially, as a PNG, and should not
  // use this method.)
  //
  // The function will set the data pointed to by |image_byte_count| with the
  // number of image bytes written to the buffer. Note that the number of bytes
  // includes only the image data written into the memory pointed to by
  // |icon_image|.
  static void SetSingleIconImageInformation(const SkBitmap& bitmap,
                                            size_t index,
                                            ICONDIR* icon_dir,
                                            ICONIMAGE* icon_image,
                                            DWORD image_offset,
                                            size_t* image_byte_count);

  // Copies the bits of an SkBitmap object into a buffer holding the bits of
  // the corresponding image for an icon within the .ico file.
  static void CopySkBitmapBitsIntoIconBuffer(const SkBitmap& bitmap,
                                             unsigned char* buffer,
                                             size_t buffer_size);

  // Given a set of bitmaps with varying dimensions, this function computes
  // the amount of memory needed in order to store the bitmaps as image icons
  // in a .ico file.
  static size_t ComputeIconFileBufferSize(const std::vector<SkBitmap>& set);

  // A helper function for computing various size components of a given bitmap.
  // The different sizes can be used within the various .ico file structures.
  //
  // |xor_mask_size| - the size, in bytes, of the XOR mask in the ICONIMAGE
  //                   structure.
  // |and_mask_size| - the size, in bytes, of the AND mask in the ICONIMAGE
  //                   structure.
  // |bytes_in_resource| - the total number of bytes set in the ICONIMAGE
  //                       structure. This value is equal to the sum of the
  //                       bytes in the AND mask and the XOR mask plus the size
  //                       of the BITMAPINFOHEADER structure. Note that since
  //                       only 32bpp are handled by the IconUtil class, the
  //                       icColors field in the ICONIMAGE structure is ignored
  //                       and is not accounted for when computing the
  //                       different size components.
  static void ComputeBitmapSizeComponents(const SkBitmap& bitmap,
                                          size_t* xor_mask_size,
                                          DWORD* bytes_in_resource);

  // A helper function of CreateSkBitmapFromHICON.
  static SkBitmap CreateSkBitmapFromHICONHelper(HICON icon,
                                                const gfx::Size& s);

  // Prevent clients from instantiating objects of that class by declaring the
  // ctor/dtor as private.
  DISALLOW_IMPLICIT_CONSTRUCTORS(IconUtil);
};

#endif  // UI_GFX_ICON_UTIL_H_

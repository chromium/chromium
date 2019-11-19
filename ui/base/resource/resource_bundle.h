// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;

namespace base {
class File;
class Lock;
class RefCountedMemory;
}

namespace ui {

class DataPack;
class ResourceHandle;

// ResourceBundle is a central facility to load images and other resources,
// such as theme graphics. Every resource is loaded only once.
class UI_BASE_EXPORT ResourceBundle {
 public:
  // Legacy font size deltas. Consider these to be magic numbers. New code
  // should declare their own size delta constant using an identifier that
  // imparts some semantic meaning.
  static const int kSmallFontDelta = -1;
  static const int kMediumFontDelta = 3;
  static const int kLargeFontDelta = 8;

  // The constant added during the compression to the front of Brotli-compressed
  // resources in Chromium. Compression occurs at tools/grit/grit/node/base.py.
  static constexpr uint8_t kBrotliConst[] = {0x1e, 0x9b};
  static const size_t kBrotliHeaderSize = 8;

  // Legacy font style mappings. TODO(tapted): Phase these out in favour of
  // client code providing their own constant with the desired font size delta.
  enum FontStyle {
    SmallFont,
    BaseFont,
    BoldFont,
    MediumFont,
    MediumBoldFont,
    LargeFont,
  };

  enum LoadResources {
    LOAD_COMMON_RESOURCES,
    DO_NOT_LOAD_COMMON_RESOURCES
  };

  // Delegate class that allows interception of pack file loading and resource
  // requests. The methods of this class may be called on multiple threads.
  class Delegate {
   public:
    // Called before a resource pack file is loaded. Return the full path for
    // the pack file to continue loading or an empty value to cancel loading.
    // |pack_path| will contain the complete default path for the pack file if
    // known or just the pack file name otherwise.
    virtual base::FilePath GetPathForResourcePack(
        const base::FilePath& pack_path,
        ScaleFactor scale_factor) = 0;

    // Called before a locale pack file is loaded. Return the full path for
    // the pack file to continue loading or an empty value to cancel loading.
    // |pack_path| will contain the complete default path for the pack file if
    // known or just the pack file name otherwise.
    virtual base::FilePath GetPathForLocalePack(
        const base::FilePath& pack_path,
        const std::string& locale) = 0;

    // Return an image resource or an empty value to attempt retrieval of the
    // default resource.
    virtual gfx::Image GetImageNamed(int resource_id) = 0;

    // Return an image resource or an empty value to attempt retrieval of the
    // default resource.
    virtual gfx::Image GetNativeImageNamed(int resource_id) = 0;

    // Return a ref counted memory resource or NULL to attempt retrieval of the
    // default resource.
    virtual base::RefCountedMemory* LoadDataResourceBytes(
        int resource_id,
        ScaleFactor scale_factor) = 0;

    // Retrieve a raw data resource. Return true if a resource was provided or
    // false to attempt retrieval of the default resource.
    virtual bool GetRawDataResource(int resource_id,
                                    ScaleFactor scale_factor,
                                    base::StringPiece* value) = 0;

    // Retrieve a localized string. Return true if a string was provided or
    // false to attempt retrieval of the default string.
    virtual bool GetLocalizedString(int message_id, base::string16* value) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Initialize the ResourceBundle for this process. Does not take ownership of
  // the |delegate| value. Returns the language selected.
  // NOTE: Mac ignores this and always loads up resources for the language
  // defined by the Cocoa UI (i.e., NSBundle does the language work).
  //
  // TODO(sergeyu): This method also loads common resources (i.e. chrome.pak).
  // There is no way to specify which resource files are loaded, i.e. names of
  // the files are hardcoded in ResourceBundle. Fix it to allow to specify which
  // files are loaded (e.g. add a new method in Delegate).
  // |load_resources| controls whether or not LoadCommonResources is called.
  static std::string InitSharedInstanceWithLocale(
      const std::string& pref_locale,
      Delegate* delegate,
      LoadResources load_resources);

  // Initialize the ResourceBundle using the given file region. If |region| is
  // MemoryMappedFile::Region::kWholeFile, the entire |pak_file| is used.
  // This allows the use of this function in a sandbox without local file
  // access (as on Android).
  static void InitSharedInstanceWithPakFileRegion(
      base::File pak_file,
      const base::MemoryMappedFile::Region& region);

  // Initialize the ResourceBundle using given data pack path for testing.
  static void InitSharedInstanceWithPakPath(const base::FilePath& path);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Returns true after the global resource loader instance has been created.
  static bool HasSharedInstance();

  // Initialize the ResourceBundle using data pack from given buffer.
  // Return the global resource loader instance.
  static ResourceBundle& GetSharedInstance();

  // Loads a secondary locale data pack using the given file region.
  void LoadSecondaryLocaleDataWithPakFileRegion(
      base::File pak_file,
      const base::MemoryMappedFile::Region& region);

  // Check if the .pak for the given locale exists.
  static bool LocaleDataPakExists(const std::string& locale);

  // Registers additional data pack files with this ResourceBundle.  When
  // looking for a DataResource, we will search these files after searching the
  // main module. |path| should be the complete path to the pack file if known
  // or just the pack file name otherwise (the delegate may optionally override
  // this value). |scale_factor| is the scale of images in this resource pak
  // relative to the images in the 1x resource pak. This method is not thread
  // safe! You should call it immediately after calling InitSharedInstance.
  void AddDataPackFromPath(const base::FilePath& path,
                           ScaleFactor scale_factor);

  // Same as above but using an already open file.
  void AddDataPackFromFile(base::File file, ScaleFactor scale_factor);

  // Same as above but using only a region (offset + size) of the file.
  void AddDataPackFromFileRegion(base::File file,
                                 const base::MemoryMappedFile::Region& region,
                                 ScaleFactor scale_factor);

  // Same as above but using contents of the given buffer.
  void AddDataPackFromBuffer(base::StringPiece buffer,
                             ScaleFactor scale_factor);

  // Same as AddDataPackFromPath but does not log an error if the pack fails to
  // load.
  void AddOptionalDataPackFromPath(const base::FilePath& path,
                                   ScaleFactor scale_factor);

  // Changes the locale for an already-initialized ResourceBundle, returning the
  // name of the newly-loaded locale.  Future calls to get strings will return
  // the strings for this new locale.  This has no effect on existing or future
  // image resources.  |locale_resources_data_| is protected by a lock for the
  // duration of the swap, as GetLocalizedString() may be concurrently invoked
  // on another thread.
  std::string ReloadLocaleResources(const std::string& pref_locale);

  // Gets image with the specified resource_id from the current module data.
  // Returns a pointer to a shared instance of gfx::ImageSkia. This shared
  // instance is owned by the resource bundle and should not be freed.
  // TODO(pkotwicz): Make method return const gfx::ImageSkia*
  //
  // NOTE: GetNativeImageNamed is preferred for cross-platform gfx::Image use.
  gfx::ImageSkia* GetImageSkiaNamed(int resource_id);

  // Gets an image resource from the current module data. This will load the
  // image in Skia format by default. The ResourceBundle owns this.
  gfx::Image& GetImageNamed(int resource_id);

  // Similar to GetImageNamed, but rather than loading the image in Skia format,
  // it will load in the native platform type. This can avoid conversion from
  // one image type to another. ResourceBundle owns the result.
  //
  // Note that if the same resource has already been loaded in GetImageNamed(),
  // gfx::Image will perform a conversion, rather than using the native image
  // loading code of ResourceBundle.
  gfx::Image& GetNativeImageNamed(int resource_id);

  // Loads the raw bytes of a scale independent data resource.
  base::RefCountedMemory* LoadDataResourceBytes(int resource_id) const;

  // Whether the |resource_id| is gzipped in this bundle. False is also returned
  // if the resource is not found.
  bool IsGzipped(int resource_id) const;

  // Whether the |resource_id| is brotli compressed in this bundle. False is
  // also returned if the resource is not found.
  bool IsBrotli(int resource_id) const;

  // Loads the raw bytes of a data resource nearest the scale factor
  // |scale_factor| into |bytes|. If the resource is compressed, decompress
  // before returning. Use ResourceHandle::SCALE_FACTOR_NONE for scale
  // independent image resources (such as wallpaper). Returns nullptr if we fail
  // to read the resource.
  base::RefCountedMemory* LoadDataResourceBytesForScale(
      int resource_id,
      ScaleFactor scale_factor) const;

  // Return the contents of a scale independent resource in a
  // StringPiece given the resource id.
  base::StringPiece GetRawDataResource(int resource_id) const;

  // Return the contents of a resource in a StringPiece given the resource id
  // nearest the scale factor |scale_factor|.
  // Use ResourceHandle::SCALE_FACTOR_NONE for scale independent image resources
  // (such as wallpaper).
  base::StringPiece GetRawDataResourceForScale(int resource_id,
                                               ScaleFactor scale_factor) const;

  // Return the contents of a scale independent resource, decompressed
  // into a newly allocated string given the resource id. Todo: Look into
  // introducing an Async version of this function in the future.
  // Bug: https://bugs.chromium.org/p/chromium/issues/detail?id=973417
  std::string LoadDataResourceString(int resource_id) const;

  // Return the contents of a scale dependent resource, decompressed into
  // a newly allocated string given the resource id.
  std::string LoadDataResourceStringForScale(int resource_id,
                                             ScaleFactor scaling_factor) const;

  // Return the contents of a localized resource, decompressed into a
  // newly allocated string given the resource id.
  std::string LoadLocalizedResourceString(int resource_id) const;

  // Get a localized string given a message id.  Returns an empty string if the
  // resource_id is not found.
  base::string16 GetLocalizedString(int resource_id);

  // Get a localized resource (for example, localized image logo) given a
  // resource id.
  base::RefCountedMemory* LoadLocalizedResourceBytes(int resource_id);

  // Returns a font list derived from the platform-specific "Base" font list.
  // The result is always cached and exists for the lifetime of the process.
  const gfx::FontList& GetFontListWithDelta(
      int size_delta,
      gfx::Font::FontStyle style = gfx::Font::NORMAL,
      gfx::Font::Weight weight = gfx::Font::Weight::NORMAL);

  // Returns a font list derived from the user-specified typeface. The
  // result is always cached and exists for the lifetime of the process.
  // If typeface is empty, we default to the platform-specific "Base" font
  // list.
  const gfx::FontList& GetFontListWithTypefaceAndDelta(
      const std::string& typeface,
      int size_delta,
      gfx::Font::FontStyle style = gfx::Font::NORMAL,
      gfx::Font::Weight weight = gfx::Font::Weight::NORMAL);

  // Returns the primary font from the FontList given by GetFontListWithDelta().
  const gfx::Font& GetFontWithDelta(
      int size_delta,
      gfx::Font::FontStyle style = gfx::Font::NORMAL,
      gfx::Font::Weight weight = gfx::Font::Weight::NORMAL);

  // Deprecated. Returns fonts using hard-coded size deltas implied by |style|.
  const gfx::FontList& GetFontList(FontStyle style);
  const gfx::Font& GetFont(FontStyle style);

  // Resets and reloads the cached fonts.  This is useful when the fonts of the
  // system have changed, for example, when the locale has changed.
  void ReloadFonts();

  // Overrides the path to the pak file from which the locale resources will be
  // loaded. Pass an empty path to undo.
  void OverrideLocalePakForTest(const base::FilePath& pak_path);

  // Overrides a localized string resource with the given string. If no delegate
  // is present, the |string| will be returned when getting the localized string
  // |resource_id|. If |ReloadLocaleResources| is called, all overrides are
  // cleared. This is intended to be used in conjunction with field trials and
  // the variations service to experiment with different UI strings. This method
  // is not thread safe!
  void OverrideLocaleStringResource(int resource_id,
                                    const base::string16& string);

  // Returns the full pathname of the locale file to load, which may be a
  // compressed locale file ending in .gz. Returns an empty string if no locale
  // data files are found.
  // Used on Android to load the local file in the browser process and pass it
  // to the sandboxed renderer process.
  static base::FilePath GetLocaleFilePath(const std::string& app_locale);

  // Returns the maximum scale factor currently loaded.
  // Returns SCALE_FACTOR_100P if no resource is loaded.
  ScaleFactor GetMaxScaleFactor() const;

  // Returns true if |scale_factor| is supported by this platform.
  static bool IsScaleFactorSupported(ScaleFactor scale_factor);

  // Checks whether overriding locale strings is supported. This will fail with
  // a DCHECK if the first string resource has already been queried.
  void CheckCanOverrideStringResources();

  // Sets whether this ResourceBundle should mangle localized strings or not.
  void set_mangle_localized_strings_for_test(bool mangle) {
    mangle_localized_strings_ = mangle;
  }

#if DCHECK_IS_ON()
  // Gets whether overriding locale strings is supported.
  bool get_can_override_locale_string_resources_for_test() {
    return can_override_locale_string_resources_;
  }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetPathForLocalePack);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetImageNamed);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetNativeImageNamed);

  friend class ResourceBundleMacImageTest;
  friend class ResourceBundleImageTest;
  friend class ResourceBundleTest;
  friend class ChromeBrowserMainMacBrowserTest;

  class ResourceBundleImageSource;
  friend class ResourceBundleImageSource;

  struct FontKey;

  using IdToStringMap = std::unordered_map<int, base::string16>;

  // Ctor/dtor are private, since we're a singleton.
  explicit ResourceBundle(Delegate* delegate);
  ~ResourceBundle();

  // Shared initialization.
  static void InitSharedInstance(Delegate* delegate);

  // Free skia_images_.
  void FreeImages();

  // Load the main resources.
  void LoadCommonResources();

  // Loads the resource paks chrome_{100,200}_percent.pak.
  void LoadChromeResources();

  // Implementation for the public methods which add a DataPack from a path. If
  // |optional| is false, an error is logged on failure to load.
  void AddDataPackFromPathInternal(const base::FilePath& path,
                                   ScaleFactor scale_factor,
                                   bool optional);

  // Inserts |data_pack| to |data_pack_| and updates |max_scale_factor_|
  // accordingly.
  void AddDataPack(std::unique_ptr<DataPack> data_pack);

  // Try to load the locale specific strings from an external data module.
  // Returns the locale that is loaded.
  std::string LoadLocaleResources(const std::string& pref_locale);

  // Load test resources in given paths. If either path is empty an empty
  // resource pack is loaded.
  void LoadTestResources(const base::FilePath& path,
                         const base::FilePath& locale_path);

  // Unload the locale specific strings and prepares to load new ones. See
  // comments for ReloadLocaleResources().
  void UnloadLocaleResources();

  // Initializes the font description of default gfx::FontList.
  void InitDefaultFontList();

  // Fills the |bitmap| given the data file to look in and the |resource_id|.
  // Returns false if the resource does not exist.
  //
  // If the call succeeds, |fell_back_to_1x| indicates whether Chrome's custom
  // csCl PNG chunk is present (added by GRIT if it falls back to a 100% image).
  bool LoadBitmap(const ResourceHandle& data_handle,
                  int resource_id,
                  SkBitmap* bitmap,
                  bool* fell_back_to_1x) const;

  // Fills the |bitmap| given the |resource_id| and |scale_factor|.
  // Returns false if the resource does not exist. This may fall back to
  // the data pack with SCALE_FACTOR_NONE, and when this happens,
  // |scale_factor| will be set to SCALE_FACTOR_NONE.
  bool LoadBitmap(int resource_id,
                  ScaleFactor* scale_factor,
                  SkBitmap* bitmap,
                  bool* fell_back_to_1x) const;

  // Returns true if missing scaled resources should be visually indicated when
  // drawing the fallback (e.g., by tinting the image).
  static bool ShouldHighlightMissingScaledResources();

  // Returns true if the data in |buf| is a PNG that has the special marker
  // added by GRIT that indicates that the image is actually 1x data.
  static bool PNGContainsFallbackMarker(const unsigned char* buf, size_t size);

  // A wrapper for PNGCodec::Decode that returns information about custom
  // chunks. For security reasons we can't alter PNGCodec to return this
  // information. Our PNG files are preprocessed by GRIT, and any special chunks
  // should occur immediately after the IHDR chunk.
  static bool DecodePNG(const unsigned char* buf,
                        size_t size,
                        SkBitmap* bitmap,
                        bool* fell_back_to_1x);

  // Returns an empty image for when a resource cannot be loaded. This is a
  // bright red bitmap.
  gfx::Image& GetEmptyImage();

  const base::FilePath& GetOverriddenPakPath();

  // If mangling of localized strings is enabled, mangles |str| to make it
  // longer and to add begin and end markers so that any truncation of it is
  // visible and returns the mangled string. If not, returns |str|.
  base::string16 MaybeMangleLocalizedString(const base::string16& str);

  // An internal implementation of |GetLocalizedString()| without setting the
  // flag of whether overriding locale strings is supported to false. We don't
  // update this flag only in |InitDefaultFontList()| which is called earlier
  // than the overriding. This is okay, because the font list doesn't need to be
  // overridden by variations.
  base::string16 GetLocalizedStringImpl(int resource_id);

  // This pointer is guaranteed to outlive the ResourceBundle instance and may
  // be NULL.
  Delegate* delegate_;

  // Protects |locale_resources_data_|.
  std::unique_ptr<base::Lock> locale_resources_data_lock_;

  // Handles for data sources.
  std::unique_ptr<ResourceHandle> locale_resources_data_;
  std::unique_ptr<ResourceHandle> secondary_locale_resources_data_;
  std::vector<std::unique_ptr<ResourceHandle>> data_packs_;

  // The maximum scale factor currently loaded.
  ScaleFactor max_scale_factor_;

  // Cached images. The ResourceBundle caches all retrieved images and keeps
  // ownership of the pointers.
  using ImageMap = std::map<int, gfx::Image>;
  ImageMap images_;

  gfx::Image empty_image_;

  // The various font lists used, as a map from a signed size delta from the
  // platform base font size, plus style, to the FontList. Cached to avoid
  // repeated GDI creation/destruction and font derivation.
  // Must be accessed only from UI thread.
  std::map<FontKey, gfx::FontList> font_cache_;

  base::FilePath overridden_pak_path_;

  IdToStringMap overridden_locale_strings_;

#if DCHECK_IS_ON()
  bool can_override_locale_string_resources_ = true;
#endif

  bool is_test_resources_ = false;
  bool mangle_localized_strings_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ResourceBundle);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_

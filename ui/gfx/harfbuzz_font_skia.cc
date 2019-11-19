// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/harfbuzz_font_skia.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

class TypefaceData;

// Maps from code points to glyph indices in a font.
using GlyphCache = std::map<uint32_t, uint16_t>;

// Wraps a custom user data attached to a hb_font object. Font data provider for
// HarfBuzz using Skia. Copied from Blink.
// TODO(ckocagil): Eliminate the duplication. http://crbug.com/368375
struct FontData {
  explicit FontData(GlyphCache* glyph_cache) : glyph_cache_(glyph_cache) {}

  SkFont font_;
  GlyphCache* glyph_cache_;
};

// Deletes the object at the given pointer after casting it to the given type.
template<typename Type>
void DeleteByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete typed_data;
}

template<typename Type>
void DeleteArrayByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete[] typed_data;
}

// Outputs the |width| and |extents| of the glyph with index |codepoint| in
// |paint|'s font.
void GetGlyphWidthAndExtents(const SkFont& font,
                             hb_codepoint_t codepoint,
                             hb_position_t* width,
                             hb_glyph_extents_t* extents) {
  DCHECK_LE(codepoint, std::numeric_limits<uint16_t>::max());

  SkScalar sk_width;
  SkRect sk_bounds;
  uint16_t glyph = static_cast<uint16_t>(codepoint);

  font.getWidths(&glyph, 1, &sk_width, &sk_bounds);
  if (width)
    *width = SkiaScalarToHarfBuzzUnits(sk_width);
  if (extents) {
    // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be
    // y-grows-up.
    extents->x_bearing = SkiaScalarToHarfBuzzUnits(sk_bounds.fLeft);
    extents->y_bearing = SkiaScalarToHarfBuzzUnits(-sk_bounds.fTop);
    extents->width = SkiaScalarToHarfBuzzUnits(sk_bounds.width());
    extents->height = SkiaScalarToHarfBuzzUnits(-sk_bounds.height());
  }
}

// Writes the |glyph| index for the given |unicode| code point. Returns whether
// the glyph exists, i.e. it is not a missing glyph.
hb_bool_t GetGlyph(hb_font_t* font,
                   void* data,
                   hb_codepoint_t unicode,
                   hb_codepoint_t variation_selector,
                   hb_codepoint_t* glyph,
                   void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  GlyphCache* cache = font_data->glyph_cache_;

  GlyphCache::iterator iter = cache->find(unicode);
  if (iter == cache->end()) {
    auto result = cache->insert(
        std::make_pair(unicode, font_data->font_.unicharToGlyph(unicode)));
    DCHECK(result.second);
    iter = result.first;
  }

  *glyph = iter->second;
  return !!*glyph;
}

hb_bool_t GetNominalGlyph(hb_font_t* font,
                          void* data,
                          hb_codepoint_t unicode,
                          hb_codepoint_t* glyph,
                          void* user_data) {
  return GetGlyph(font, data, unicode, 0, glyph, user_data);
}

// Returns the horizontal advance value of the |glyph|.
hb_position_t GetGlyphHorizontalAdvance(hb_font_t* font,
                                        void* data,
                                        hb_codepoint_t glyph,
                                        void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  hb_position_t advance = 0;

  GetGlyphWidthAndExtents(font_data->font_, glyph, &advance, 0);
  return advance;
}

hb_bool_t GetGlyphHorizontalOrigin(hb_font_t* font,
                                   void* data,
                                   hb_codepoint_t glyph,
                                   hb_position_t* x,
                                   hb_position_t* y,
                                   void* user_data) {
  // Just return true, like the HarfBuzz-FreeType implementation.
  return true;
}

hb_position_t GetGlyphKerning(FontData* font_data,
                              hb_codepoint_t first_glyph,
                              hb_codepoint_t second_glyph) {
  SkTypeface* typeface = font_data->font_.getTypeface();
  const uint16_t glyphs[2] = { static_cast<uint16_t>(first_glyph),
                               static_cast<uint16_t>(second_glyph) };
  int32_t kerning_adjustments[1] = { 0 };

  if (!typeface->getKerningPairAdjustments(glyphs, 2, kerning_adjustments))
    return 0;

  SkScalar upm = SkIntToScalar(typeface->getUnitsPerEm());
  SkScalar size = font_data->font_.getSize();
  return SkiaScalarToHarfBuzzUnits(SkIntToScalar(kerning_adjustments[0]) *
                                   size / upm);
}

hb_position_t GetGlyphHorizontalKerning(hb_font_t* font,
                                        void* data,
                                        hb_codepoint_t left_glyph,
                                        hb_codepoint_t right_glyph,
                                        void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  return GetGlyphKerning(font_data, left_glyph, right_glyph);
}

hb_position_t GetGlyphVerticalKerning(hb_font_t* font,
                                      void* data,
                                      hb_codepoint_t top_glyph,
                                      hb_codepoint_t bottom_glyph,
                                      void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  return GetGlyphKerning(font_data, top_glyph, bottom_glyph);
}

// Writes the |extents| of |glyph|.
hb_bool_t GetGlyphExtents(hb_font_t* font,
                          void* data,
                          hb_codepoint_t glyph,
                          hb_glyph_extents_t* extents,
                          void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);

  GetGlyphWidthAndExtents(font_data->font_, glyph, 0, extents);
  return true;
}

class FontFuncs {
 public:
  FontFuncs() : font_funcs_(hb_font_funcs_create()) {
    hb_font_funcs_set_variation_glyph_func(font_funcs_, GetGlyph, 0, 0);
    hb_font_funcs_set_nominal_glyph_func(font_funcs_, GetNominalGlyph, 0, 0);
    hb_font_funcs_set_glyph_h_advance_func(
        font_funcs_, GetGlyphHorizontalAdvance, 0, 0);
    hb_font_funcs_set_glyph_h_kerning_func(
        font_funcs_, GetGlyphHorizontalKerning, 0, 0);
    hb_font_funcs_set_glyph_h_origin_func(
        font_funcs_, GetGlyphHorizontalOrigin, 0, 0);
    hb_font_funcs_set_glyph_v_kerning_func(
        font_funcs_, GetGlyphVerticalKerning, 0, 0);
    hb_font_funcs_set_glyph_extents_func(
        font_funcs_, GetGlyphExtents, 0, 0);
    hb_font_funcs_make_immutable(font_funcs_);
  }

  ~FontFuncs() {
    hb_font_funcs_destroy(font_funcs_);
  }

  hb_font_funcs_t* get() { return font_funcs_; }

 private:
  hb_font_funcs_t* font_funcs_;

  DISALLOW_COPY_AND_ASSIGN(FontFuncs);
};

base::LazyInstance<FontFuncs>::Leaky g_font_funcs = LAZY_INSTANCE_INITIALIZER;

// Returns the raw data of the font table |tag|.
hb_blob_t* GetFontTable(hb_face_t* face, hb_tag_t tag, void* user_data) {
  SkTypeface* typeface = reinterpret_cast<SkTypeface*>(user_data);

  const size_t table_size = typeface->getTableSize(tag);
  if (!table_size)
    return 0;

  std::unique_ptr<char[]> buffer(new char[table_size]);
  if (!buffer)
    return 0;
  size_t actual_size = typeface->getTableData(tag, 0, table_size, buffer.get());
  if (table_size != actual_size)
    return 0;

  char* buffer_raw = buffer.release();
  return hb_blob_create(buffer_raw, static_cast<uint32_t>(table_size),
                        HB_MEMORY_MODE_WRITABLE, buffer_raw,
                        DeleteArrayByType<char>);
}

// For a given skia typeface, maps to its harfbuzz face and its glyphs cache.
class TypefaceData {
 public:
  explicit TypefaceData(sk_sp<SkTypeface> skia_face) : sk_typeface_(skia_face) {
    face_ = hb_face_create_for_tables(GetFontTable, skia_face.get(), nullptr);
    DCHECK(face_);
  }

  TypefaceData(TypefaceData&& data) {
    face_ = data.face_;
    glyphs_ = std::move(data.glyphs_);
    data.face_ = nullptr;
  }

  ~TypefaceData() { hb_face_destroy(face_); }

  hb_face_t* face() { return face_; }
  GlyphCache* glyphs() { return &glyphs_; }

 private:
  TypefaceData() = delete;

  GlyphCache glyphs_;
  hb_face_t* face_ = nullptr;

  // The skia typeface must outlive |face_| since it's being used by harfbuzz.
  sk_sp<SkTypeface> sk_typeface_;

  DISALLOW_COPY_AND_ASSIGN(TypefaceData);
};

}  // namespace

// Creates a HarfBuzz font from the given Skia face and text size.
hb_font_t* CreateHarfBuzzFont(sk_sp<SkTypeface> skia_face,
                              SkScalar text_size,
                              const FontRenderParams& params,
                              bool subpixel_rendering_suppressed) {
  // A cache from Skia font to harfbuzz typeface information.
  using TypefaceCache = base::MRUCache<SkFontID, TypefaceData>;

  constexpr int kTypefaceCacheSize = 64;
  static base::NoDestructor<TypefaceCache> face_caches(kTypefaceCacheSize);

  TypefaceCache* typeface_cache = face_caches.get();
  TypefaceCache::iterator typeface_data =
      typeface_cache->Get(skia_face->uniqueID());
  if (typeface_data == typeface_cache->end()) {
    TypefaceData new_typeface_data(skia_face);
    typeface_data = typeface_cache->Put(skia_face->uniqueID(),
                                        std::move(new_typeface_data));
  }

  DCHECK(typeface_data->second.face());
  hb_font_t* harfbuzz_font = hb_font_create(typeface_data->second.face());

  const int scale = SkiaScalarToHarfBuzzUnits(text_size);
  hb_font_set_scale(harfbuzz_font, scale, scale);
  FontData* hb_font_data = new FontData(typeface_data->second.glyphs());
  hb_font_data->font_.setTypeface(std::move(skia_face));
  hb_font_data->font_.setSize(text_size);
  // TODO(ckocagil): Do we need to update these params later?
  internal::ApplyRenderParams(params, subpixel_rendering_suppressed,
                              &hb_font_data->font_);
  hb_font_set_funcs(harfbuzz_font, g_font_funcs.Get().get(), hb_font_data,
                    DeleteByType<FontData>);
  hb_font_make_immutable(harfbuzz_font);
  return harfbuzz_font;
}

}  // namespace gfx

// Compile with Skia's actual HarfBuzz shaper and PDF subsetter, substituting
// HarfRust compatibility headers for the second executable.
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkDocument.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPaint.h"
#include "include/core/SkStream.h"
#include "include/docs/SkPDFDocument.h"
#include "include/ports/SkFontMgr_empty.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "src/pdf/SkPDFGlyphUse.h"
#include "src/pdf/SkPDFSubsetFont.h"
#include "src/ports/SkTypeface_proxy.h"

// Keep Skia headers in the prebuilt libraries' release configuration, but
// always enable the consumer checks.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

class TableTypeface final : public SkTypeface_proxy {
public:
    explicit TableTypeface(sk_sp<SkTypeface> face)
        : SkTypeface_proxy(face, face->fontStyle(), face->isFixedPitch()) {}
    mutable unsigned tableReads = 0;
    mutable unsigned tagReads = 0;
    mutable unsigned charMaps = 0;
    bool allowPdfStream = false;

protected:
    std::unique_ptr<SkStreamAsset> onOpenStream(int* index) const override {
        return allowPdfStream ? SkTypeface_proxy::onOpenStream(index) : nullptr;
    }
    std::unique_ptr<SkStreamAsset> onOpenExistingStream(int*) const override { return nullptr; }
    int onGetTableTags(SkSpan<SkFontTableTag> tags) const override {
        ++tagReads;
        return SkTypeface_proxy::onGetTableTags(tags);
    }
    size_t onGetTableData(SkFontTableTag tag, size_t offset, size_t size, void* data) const override {
        ++tableReads;
        return SkTypeface_proxy::onGetTableData(tag, offset, size, data);
    }
    void onCharsToGlyphs(SkSpan<const SkUnichar> chars, SkSpan<SkGlyphID> glyphs) const override {
        ++charMaps;
        SkTypeface_proxy::onCharsToGlyphs(chars, glyphs);
    }
};

class RecordingHandler final : public SkShaper::RunHandler {
public:
    explicit RecordingHandler(const char* text) : blob(text, {0, 0}) {}
    SkTextBlobBuilderRunHandler blob;

    void beginLine() override { blob.beginLine(); }
    void runInfo(const RunInfo& info) override { blob.runInfo(info); }
    void commitRunInfo() override { blob.commitRunInfo(); }
    Buffer runBuffer(const RunInfo& info) override {
        buffer = blob.runBuffer(info);
        assert(buffer.glyphs && buffer.positions && buffer.clusters);
        return buffer;
    }
    void commitRunBuffer(const RunInfo& info) override {
        std::printf("run %u %u %zu %.6f %.6f\n", info.fBidiLevel, info.fScript,
                    info.glyphCount, info.fAdvance.x(), info.fAdvance.y());
        for (size_t i = 0; i < info.glyphCount; ++i) {
            std::printf("glyph %u %u %.6f %.6f\n", buffer.glyphs[i], buffer.clusters[i],
                        buffer.positions[i].x(), buffer.positions[i].y());
        }
        blob.commitRunBuffer(info);
    }
    void commitLine() override { blob.commitLine(); }

private:
    Buffer buffer{};
};

static void save(const std::string& path, const SkData& data) {
    SkFILEWStream file(path.c_str());
    assert(file.isValid() && file.write(data.data(), data.size()));
}

int main(int argc, char** argv) {
    assert(argc == 5);
    const char* fontPath = argv[1];
    const std::string output = argv[2];
    const bool callback = std::strcmp(argv[3], "callback") == 0;
    const bool variable = std::strcmp(argv[4], "variable") == 0;
    auto manager = SkFontMgr_New_Custom_Empty();
    assert(manager);
    auto face = manager->makeFromFile(fontPath);
    assert(face && face->countGlyphs());
    if (variable) {
        SkFontArguments::VariationPosition::Coordinate axes[] = {
            {SkSetFourByteTag('w', 'd', 't', 'h'), 150},
            {SkSetFourByteTag('w', 'g', 'h', 't'), 800},
        };
        SkFontArguments args;
        args.setVariationDesignPosition({axes, 2});
        face = face->makeClone(args);
        assert(face);
    }
    sk_sp<TableTypeface> proxy;
    if (callback) {
        proxy = sk_make_sp<TableTypeface>(face);
        face = proxy;
    }
    auto shaper = SkShapers::HB::ShapeThenWrap(SkUnicodes::ICU::Make(), manager);
    assert(shaper && SkPDFCanSubsetTableBasedFonts());
    const char* text = "office caf\xC3\xA9 a\xCC\x81 ac";
    for (float size : {12.f, 36.f}) {
        for (float scale : {1.f, 1.75f}) {
            SkFont font(face, size);
            font.setScaleX(scale);
            RecordingHandler handler(text);
            shaper->shape(text, std::strlen(text), font, true, 180.f, &handler);
            auto blob = handler.blob.makeBlob();
            assert(blob);
            if (size == 36.f && scale == 1.f) {
                // PDF font-descriptor creation needs the complete font stream.
                // Its subsetter still takes table callbacks because existing
                // streams remain unavailable. Shaping and direct subset tests
                // run with both stream methods disabled.
                if (proxy) proxy->allowPdfStream = true;
                SkFILEWStream file((output + ".pdf").c_str());
                assert(file.isValid());
                SkPDF::Metadata metadata;
                metadata.allowNoJpegs = true;  // This document contains only text.
                auto document = SkPDF::MakeDocument(&file, metadata);
                assert(document);
                auto canvas = document->beginPage(300, 300);
                SkPaint paint;
                canvas->drawTextBlob(blob, 20, 60, paint);
                document->endPage();
                document->close();
                if (proxy) proxy->allowPdfStream = false;
            }
        }
    }
    SkFont font(face, 36);
    SkGlyphID selected[2];
    assert(font.textToGlyphs("ac", 2, SkTextEncoding::kUTF8, {selected, 2}) == 2);
    assert(selected[0] && selected[1]);
    for (bool zero : {false, true}) {
        SkPDFGlyphUse usage(1, face->countGlyphs() - 1);
        usage.set(selected[0]);
        usage.set(selected[1]);
        if (zero) usage.set(0);
        auto subset = SkPDFSubsetFont(*face, usage);
        assert(subset && subset->size());
        save(output + (zero ? ".zero.ttf" : ".ttf"), *subset);
        auto subsetFace = manager->makeFromData(subset);
        assert(subsetFace);
        SkFont subsetFont(subsetFace, 36);
        SkGlyphID retained[2];
        assert(subsetFont.textToGlyphs("ac", 2, SkTextEncoding::kUTF8, {retained, 2}) == 2);
        assert(retained[0] == selected[0] && retained[1] == selected[1]);
        assert(subsetFace->countGlyphs() < face->countGlyphs());
    }
    if (proxy) {
        assert(proxy->tableReads && proxy->tagReads && proxy->charMaps);
    }
    SkShapers::HB::PurgeCaches();
}

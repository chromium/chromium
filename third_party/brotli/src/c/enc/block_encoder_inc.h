#if !defined(BROTLI_BASE64_LUT_)
#define BROTLI_BASE64_LUT_
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint8_t kIsBase64[256];
#endif

#define HistogramType FN(Histogram)

static void FN(BuildAndStoreEntropyCodes)(
    MemoryManager* m, BlockEncoder* self, const HistogramType* histograms,
    const size_t histograms_size, const size_t alphabet_size, HuffmanTree* tree,
    const uint8_t* is_base64_histogram, size_t* storage_ix, uint8_t* storage) {
  const size_t table_size = histograms_size * self->histogram_length_;
  self->depths_ = BROTLI_ALLOC(m, uint8_t, table_size);
  self->bits_ = BROTLI_ALLOC(m, uint16_t, table_size);
  if (BROTLI_IS_OOM(m)) return;

  {
    size_t i;
    for (i = 0; i < histograms_size; ++i) {
      size_t ix = i * self->histogram_length_;
      if (self->histogram_length_ == 256 && is_base64_histogram && i < 256 &&
          is_base64_histogram[i]) {
        size_t k;
        BROTLI_BOOL all_base64 = BROTLI_TRUE;
        for (k = 0; k < 256; ++k) {
          if (histograms[i].data_[k] > 0 && !kIsBase64[k]) {
            all_base64 = BROTLI_FALSE;
            break;
          }
        }
        if (all_base64) {
          memset(&self->depths_[ix], 0, 256);
          for (k = 0; k < 256; ++k) {
            if (kIsBase64[k]) {
              self->depths_[ix + k] = 6;
            }
          }
          BrotliConvertBitDepthsToSymbols(&self->depths_[ix], 256,
                                          &self->bits_[ix]);
          BrotliStoreHuffmanTree(&self->depths_[ix], 256, tree, storage_ix,
                                 storage);
        } else {
          BuildAndStoreHuffmanTree(
              &histograms[i].data_[0], self->histogram_length_, alphabet_size,
              tree, &self->depths_[ix], &self->bits_[ix], storage_ix, storage);
        }
      } else {
        BuildAndStoreHuffmanTree(
            &histograms[i].data_[0], self->histogram_length_, alphabet_size,
            tree, &self->depths_[ix], &self->bits_[ix], storage_ix, storage);
      }
    }
  }
}

#undef HistogramType

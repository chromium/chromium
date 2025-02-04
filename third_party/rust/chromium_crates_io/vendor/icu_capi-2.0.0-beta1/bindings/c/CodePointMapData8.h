#ifndef CodePointMapData8_H
#define CodePointMapData8_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "CodePointRangeIterator.d.h"
#include "CodePointSetData.d.h"
#include "DataError.d.h"
#include "DataProvider.d.h"

#include "CodePointMapData8.d.h"






uint8_t icu4x_CodePointMapData8_get_mv1(const CodePointMapData8* self, char32_t cp);

uint32_t icu4x_CodePointMapData8_general_category_to_mask_mv1(uint8_t gc);

CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_mv1(const CodePointMapData8* self, uint8_t value);

CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_complemented_mv1(const CodePointMapData8* self, uint8_t value);

CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_mask_mv1(const CodePointMapData8* self, uint32_t mask);

CodePointSetData* icu4x_CodePointMapData8_get_set_for_value_mv1(const CodePointMapData8* self, uint8_t value);

typedef struct icu4x_CodePointMapData8_load_general_category_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_general_category_mv1_result;
icu4x_CodePointMapData8_load_general_category_mv1_result icu4x_CodePointMapData8_load_general_category_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_bidi_class_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_bidi_class_mv1_result;
icu4x_CodePointMapData8_load_bidi_class_mv1_result icu4x_CodePointMapData8_load_bidi_class_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_east_asian_width_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_east_asian_width_mv1_result;
icu4x_CodePointMapData8_load_east_asian_width_mv1_result icu4x_CodePointMapData8_load_east_asian_width_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result;
icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result icu4x_CodePointMapData8_load_hangul_syllable_type_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result;
icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result icu4x_CodePointMapData8_load_indic_syllabic_category_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_line_break_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_line_break_mv1_result;
icu4x_CodePointMapData8_load_line_break_mv1_result icu4x_CodePointMapData8_load_line_break_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result;
icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_word_break_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_word_break_mv1_result;
icu4x_CodePointMapData8_load_word_break_mv1_result icu4x_CodePointMapData8_load_word_break_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_sentence_break_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_sentence_break_mv1_result;
icu4x_CodePointMapData8_load_sentence_break_mv1_result icu4x_CodePointMapData8_load_sentence_break_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_joining_type_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_joining_type_mv1_result;
icu4x_CodePointMapData8_load_joining_type_mv1_result icu4x_CodePointMapData8_load_joining_type_mv1(const DataProvider* provider);

typedef struct icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result {union {CodePointMapData8* ok; DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result;
icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result icu4x_CodePointMapData8_load_canonical_combining_class_mv1(const DataProvider* provider);


void icu4x_CodePointMapData8_destroy_mv1(CodePointMapData8* self);





#endif // CodePointMapData8_H

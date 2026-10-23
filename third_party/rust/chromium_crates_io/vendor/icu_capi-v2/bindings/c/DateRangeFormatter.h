#ifndef DateRangeFormatter_H
#define DateRangeFormatter_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataProvider.d.h"
#include "Date.d.h"
#include "DateTimeAlignment.d.h"
#include "DateTimeFormatterLoadError.d.h"
#include "DateTimeLength.d.h"
#include "IsoDate.d.h"
#include "Locale.d.h"
#include "YearStyle.d.h"

#include "DateRangeFormatter.d.h"






typedef struct icu4x_DateRangeFormatter_create_d_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_d_mv1_result;
icu4x_DateRangeFormatter_create_d_mv1_result icu4x_DateRangeFormatter_create_d_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_d_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_d_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_d_with_provider_mv1_result icu4x_DateRangeFormatter_create_d_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_md_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_md_mv1_result;
icu4x_DateRangeFormatter_create_md_mv1_result icu4x_DateRangeFormatter_create_md_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_md_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_md_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_md_with_provider_mv1_result icu4x_DateRangeFormatter_create_md_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_ymd_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymd_mv1_result;
icu4x_DateRangeFormatter_create_ymd_mv1_result icu4x_DateRangeFormatter_create_ymd_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_ymd_with_provider_mv1_result icu4x_DateRangeFormatter_create_ymd_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_de_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_de_mv1_result;
icu4x_DateRangeFormatter_create_de_mv1_result icu4x_DateRangeFormatter_create_de_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_de_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_de_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_de_with_provider_mv1_result icu4x_DateRangeFormatter_create_de_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_mde_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_mde_mv1_result;
icu4x_DateRangeFormatter_create_mde_mv1_result icu4x_DateRangeFormatter_create_mde_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_mde_with_provider_mv1_result icu4x_DateRangeFormatter_create_mde_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_ymde_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymde_mv1_result;
icu4x_DateRangeFormatter_create_ymde_mv1_result icu4x_DateRangeFormatter_create_ymde_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_ymde_with_provider_mv1_result icu4x_DateRangeFormatter_create_ymde_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_e_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_e_mv1_result;
icu4x_DateRangeFormatter_create_e_mv1_result icu4x_DateRangeFormatter_create_e_mv1(const Locale* locale, DateTimeLength_option length);

typedef struct icu4x_DateRangeFormatter_create_e_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_e_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_e_with_provider_mv1_result icu4x_DateRangeFormatter_create_e_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length);

typedef struct icu4x_DateRangeFormatter_create_m_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_m_mv1_result;
icu4x_DateRangeFormatter_create_m_mv1_result icu4x_DateRangeFormatter_create_m_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_m_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_m_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_m_with_provider_mv1_result icu4x_DateRangeFormatter_create_m_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment);

typedef struct icu4x_DateRangeFormatter_create_ym_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ym_mv1_result;
icu4x_DateRangeFormatter_create_ym_mv1_result icu4x_DateRangeFormatter_create_ym_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_ym_with_provider_mv1_result icu4x_DateRangeFormatter_create_ym_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_y_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_y_mv1_result;
icu4x_DateRangeFormatter_create_y_mv1_result icu4x_DateRangeFormatter_create_y_mv1(const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

typedef struct icu4x_DateRangeFormatter_create_y_with_provider_mv1_result {union {DateRangeFormatter* ok; DateTimeFormatterLoadError err;}; bool is_ok;} icu4x_DateRangeFormatter_create_y_with_provider_mv1_result;
icu4x_DateRangeFormatter_create_y_with_provider_mv1_result icu4x_DateRangeFormatter_create_y_with_provider_mv1(const DataProvider* provider, const Locale* locale, DateTimeLength_option length, DateTimeAlignment_option alignment, YearStyle_option year_style);

void icu4x_DateRangeFormatter_format_iso_mv1(const DateRangeFormatter* self, const IsoDate* start_iso_date, const IsoDate* end_iso_date, DiplomatWrite* write);

void icu4x_DateRangeFormatter_format_same_calendar_mv1(const DateRangeFormatter* self, const Date* start_date, const Date* end_date, DiplomatWrite* write);

void icu4x_DateRangeFormatter_destroy_mv1(DateRangeFormatter* self);





#endif // DateRangeFormatter_H

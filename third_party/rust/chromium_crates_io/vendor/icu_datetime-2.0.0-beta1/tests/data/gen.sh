#!/bin/sh
cargo run -p icu4x-datagen -- \
--markers \
GregorianDateLengthsV1Marker \
GregorianDateSymbolsV1Marker \
TimeLengthsV1Marker \
TimeSymbolsV1Marker \
DecimalSymbolsV2Marker \
TimeZoneFormatsV1Marker \
MetazoneSpecificNamesShortV1Marker \
--locales en \
--format blob \
--out $(dirname $0)/blob.postcard \
--overwrite
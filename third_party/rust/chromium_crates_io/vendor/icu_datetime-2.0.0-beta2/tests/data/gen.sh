#!/bin/sh
cargo run -p icu4x-datagen -- \
--markers \
GregorianDateLengthsV1 \
GregorianDateSymbolsV1 \
TimeLengthsV1 \
TimeSymbolsV1 \
DecimalSymbolsV2Marker \
TimeZoneFormatsV1 \
MetazoneSpecificNamesShortV1 \
--locales en \
--format blob \
--out $(dirname $0)/blob.postcard \
--overwrite
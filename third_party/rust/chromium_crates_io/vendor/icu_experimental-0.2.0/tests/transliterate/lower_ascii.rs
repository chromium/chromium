// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::any::TypeId;

use icu::casemap::CaseMapper;
use icu_experimental::transliterate::{
    provider::TransliteratorRulesV1Marker, CustomTransliterator, RuleCollection,
    RuleCollectionProvider, Transliterator,
};
use icu_locale::LanguageIdentifier;
use icu_provider::prelude::*;

struct TransliteratorMultiSourceProvider<'a>(
    RuleCollectionProvider<'a, icu_properties::provider::Baked, icu_normalizer::provider::Baked>,
);

impl<'a, M> DataProvider<M> for TransliteratorMultiSourceProvider<'a>
where
    M: DataMarker,
    RuleCollectionProvider<'a, icu_properties::provider::Baked, icu_normalizer::provider::Baked>:
        DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        println!("{:?} {req:?}", M::INFO);
        if TypeId::of::<M>() == TypeId::of::<TransliteratorRulesV1Marker>() {
            let mut silent_req = req;
            silent_req.metadata.silent = true;
            if let Some(response) = DataProvider::<TransliteratorRulesV1Marker>::load(
                &icu_experimental::provider::Baked,
                silent_req,
            )
            .allow_identifier_not_found()?
            {
                return Ok(DataResponse {
                    metadata: response.metadata,
                    payload: response.payload.dynamic_cast()?,
                });
            }
        }
        self.0.load(req)
    }
}

#[derive(Debug)]
struct LowercaseTransliterator(CaseMapper);

impl CustomTransliterator for LowercaseTransliterator {
    fn transliterate(&self, input: &str, range: std::ops::Range<usize>) -> String {
        self.0
            .lowercase_to_string(&input[range], &LanguageIdentifier::default())
    }
}

#[test]
fn test_lower_ascii() {
    let mut collection = RuleCollection::default();
    // Register Latin-ASCII so that the alias mapping gets added
    collection.register_source(
        &"und-t-und-latn-d0-ascii".parse().unwrap(),
        "<error>".to_string(),
        ["Latin-ASCII"],
        false,
        true,
    );
    // Register Lower so that the alias mapping gets added
    collection.register_source(
        &"und-t-und-x0-lower".parse().unwrap(),
        "<error>".to_string(),
        ["Any-XLower"],
        false,
        true,
    );
    // Now register our new transliterator
    collection.register_source(
        &"und-t-und-x0-lowascii".parse().unwrap(),
        // "::NFD; ::[:Nonspacing Mark:] Remove; ::Any-XLower; ::NFC; ::Latin-ASCII;".to_string(),
        "::NFD; ::[:Nonspacing Mark:] Remove; ::Any-XLower; ::NFC; ::Latin-ASCII;".to_string(),
        [],
        false,
        true,
    );
    let provider = TransliteratorMultiSourceProvider(collection.as_provider());
    let t = Transliterator::try_new_with_override_unstable(
        &provider,
        &provider,
        &"und-t-und-x0-lowascii".parse().unwrap(),
        |locale| {
            if locale.normalizing_eq("und-t-und-x0-lower") {
                Some(Ok(Box::new(LowercaseTransliterator(CaseMapper::new()))))
            } else {
                None
            }
        },
    )
    .unwrap();
    let r = t.transliterate("ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ".to_string());
    assert_eq!(r, "internationalization");
}

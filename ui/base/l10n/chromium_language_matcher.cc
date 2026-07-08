// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/chromium_language_matcher.h"

#include <iterator>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/tags.h"
#include "base/no_destructor.h"
#include "ui/base/buildflags.h"

namespace ui_l10n {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagMatcher;

constexpr auto kAcceptLanguageList = std::to_array<LanguageTag>({
    GetKnownLanguageTag("af"),  // Afrikaans
    GetKnownLanguageTag("ak"),  // Twi
    GetKnownLanguageTag("am"),  // Amharic
    GetKnownLanguageTag("an"),  // Aragonese
    GetKnownLanguageTag("ar"),  // Arabic
#if BUILDFLAG(ENABLE_PSEUDOLOCALES)
    GetKnownLanguageTag("ar-XB"),           // RTL Pseudolocale
#endif                                      // BUILDFLAG(ENABLE_PSEUDOLOCALES)
    GetKnownLanguageTag("as"),              // Assamese
    GetKnownLanguageTag("ast"),             // Asturian
    GetKnownLanguageTag("ay"),              // Aymara
    GetKnownLanguageTag("az"),              // Azerbaijani
    GetKnownLanguageTag("be"),              // Belarusian
    GetKnownLanguageTag("bg"),              // Bulgarian
    GetKnownLanguageTag("bho"),             // Bhojpuri
    GetKnownLanguageTag("bm"),              // Bambara
    GetKnownLanguageTag("bn"),              // Bengali
    GetKnownLanguageTag("br"),              // Breton
    GetKnownLanguageTag("bs"),              // Bosnian
    GetKnownLanguageTag("ca"),              // Catalan
    GetKnownLanguageTag("ceb"),             // Cebuano
    GetKnownLanguageTag("chr"),             // Cherokee
    GetKnownLanguageTag("ckb"),             // Kurdish (Arabic),  Sorani
    GetKnownLanguageTag("co"),              // Corsican
    GetKnownLanguageTag("cs"),              // Czech
    GetKnownLanguageTag("cy"),              // Welsh
    GetKnownLanguageTag("da"),              // Danish
    GetKnownLanguageTag("de"),              // German
    GetKnownLanguageTag("de-AT"),           // German (Austria)
    GetKnownLanguageTag("de-CH"),           // German (Switzerland)
    GetKnownLanguageTag("de-DE"),           // German (Germany)
    GetKnownLanguageTag("de-LI"),           // German (Liechtenstein)
    GetKnownLanguageTag("doi"),             // Dogri
    GetKnownLanguageTag("dv"),              // Dhivehi
    GetKnownLanguageTag("ee"),              // Ewe
    GetKnownLanguageTag("el"),              // Greek
    GetKnownLanguageTag("en"),              // English
    GetKnownLanguageTag("en-AU"),           // English (Australia)
    GetKnownLanguageTag("en-CA"),           // English (Canada)
    GetKnownLanguageTag("en-GB"),           // English (UK)
    GetKnownLanguageTag("en-GB-oxendict"),  // English (UK, OED
                                            // spelling)
    GetKnownLanguageTag("en-IE"),           // English (Ireland)
    GetKnownLanguageTag("en-IN"),           // English (India)
    GetKnownLanguageTag("en-NZ"),           // English (New Zealand)
    GetKnownLanguageTag("en-US"),           // English (US)
#if BUILDFLAG(ENABLE_PSEUDOLOCALES)
    GetKnownLanguageTag("en-XA"),     // Long strings Pseudolocale
#endif                                // BUILDFLAG(ENABLE_PSEUDOLOCALES)
    GetKnownLanguageTag("en-ZA"),     // English (South Africa)
    GetKnownLanguageTag("eo"),        // Esperanto
    GetKnownLanguageTag("es"),        // Spanish
    GetKnownLanguageTag("es-419"),    // Spanish (Latin America)
    GetKnownLanguageTag("es-AR"),     // Spanish (Argentina)
    GetKnownLanguageTag("es-CL"),     // Spanish (Chile)
    GetKnownLanguageTag("es-CO"),     // Spanish (Colombia)
    GetKnownLanguageTag("es-CR"),     // Spanish (Costa Rica)
    GetKnownLanguageTag("es-ES"),     // Spanish (Spain)
    GetKnownLanguageTag("es-HN"),     // Spanish (Honduras)
    GetKnownLanguageTag("es-MX"),     // Spanish (Mexico)
    GetKnownLanguageTag("es-PE"),     // Spanish (Peru)
    GetKnownLanguageTag("es-US"),     // Spanish (US)
    GetKnownLanguageTag("es-UY"),     // Spanish (Uruguay)
    GetKnownLanguageTag("es-VE"),     // Spanish (Venezuela)
    GetKnownLanguageTag("et"),        // Estonian
    GetKnownLanguageTag("eu"),        // Basque
    GetKnownLanguageTag("fa"),        // Persian
    GetKnownLanguageTag("fi"),        // Finnish
    GetKnownLanguageTag("fil"),       // Filipino
    GetKnownLanguageTag("fo"),        // Faroese
    GetKnownLanguageTag("fr"),        // French
    GetKnownLanguageTag("fr-CA"),     // French (Canada)
    GetKnownLanguageTag("fr-CH"),     // French (Switzerland)
    GetKnownLanguageTag("fr-FR"),     // French (France)
    GetKnownLanguageTag("fy"),        // Frisian
    GetKnownLanguageTag("ga"),        // Irish
    GetKnownLanguageTag("gd"),        // Scots Gaelic
    GetKnownLanguageTag("gl"),        // Galician
    GetKnownLanguageTag("gn"),        // Guarani
    GetKnownLanguageTag("gu"),        // Gujarati
    GetKnownLanguageTag("ha"),        // Hausa
    GetKnownLanguageTag("haw"),       // Hawaiian
    GetKnownLanguageTag("he"),        // Hebrew
    GetKnownLanguageTag("hi"),        // Hindi
    GetKnownLanguageTag("hmn"),       // Hmong
    GetKnownLanguageTag("hr"),        // Croatian
    GetKnownLanguageTag("ht"),        // Haitian Creole
    GetKnownLanguageTag("hu"),        // Hungarian
    GetKnownLanguageTag("hy"),        // Armenian
    GetKnownLanguageTag("ia"),        // Interlingua
    GetKnownLanguageTag("id"),        // Indonesian
    GetKnownLanguageTag("ig"),        // Igbo
    GetKnownLanguageTag("ilo"),       // Ilocano
    GetKnownLanguageTag("is"),        // Icelandic
    GetKnownLanguageTag("it"),        // Italian
    GetKnownLanguageTag("it-CH"),     // Italian (Switzerland)
    GetKnownLanguageTag("it-IT"),     // Italian (Italy)
    GetKnownLanguageTag("ja"),        // Japanese
    GetKnownLanguageTag("jv"),        // Javanese
    GetKnownLanguageTag("ka"),        // Georgian
    GetKnownLanguageTag("kk"),        // Kazakh
    GetKnownLanguageTag("km"),        // Cambodian
    GetKnownLanguageTag("kn"),        // Kannada
    GetKnownLanguageTag("ko"),        // Korean
    GetKnownLanguageTag("kok"),       // Konkani
    GetKnownLanguageTag("kri"),       // Krio
    GetKnownLanguageTag("ku"),        // Kurdish
    GetKnownLanguageTag("ky"),        // Kyrgyz
    GetKnownLanguageTag("la"),        // Latin
    GetKnownLanguageTag("lb"),        // Luxembourgish
    GetKnownLanguageTag("lg"),        // Luganda
    GetKnownLanguageTag("ln"),        // Lingala
    GetKnownLanguageTag("lo"),        // Laothian
    GetKnownLanguageTag("lt"),        // Lithuanian
    GetKnownLanguageTag("lus"),       // Mizo
    GetKnownLanguageTag("lv"),        // Latvian
    GetKnownLanguageTag("mai"),       // Maithili
    GetKnownLanguageTag("mg"),        // Malagasy
    GetKnownLanguageTag("mi"),        // Maori
    GetKnownLanguageTag("mk"),        // Macedonian
    GetKnownLanguageTag("ml"),        // Malayalam
    GetKnownLanguageTag("mn"),        // Mongolian
    GetKnownLanguageTag("mni-Mtei"),  // Manipuri (Meitei Mayek)
    GetKnownLanguageTag("mr"),        // Marathi
    GetKnownLanguageTag("ms"),        // Malay
    GetKnownLanguageTag("mt"),        // Maltese
    GetKnownLanguageTag("my"),        // Burmese
    GetKnownLanguageTag("nb"),        // Norwegian (Bokmal)
    GetKnownLanguageTag("ne"),        // Nepali
    GetKnownLanguageTag("nl"),        // Dutch
    GetKnownLanguageTag("nn"),        // Norwegian (Nynorsk)
    GetKnownLanguageTag("no"),        // Norwegian
    GetKnownLanguageTag("nso"),       // Sepedi
    GetKnownLanguageTag("ny"),        // Nyanja
    GetKnownLanguageTag("oc"),        // Occitan
    GetKnownLanguageTag("om"),        // Oromo
    GetKnownLanguageTag("or"),        // Odia (Oriya)
    GetKnownLanguageTag("pa"),        // Punjabi
    GetKnownLanguageTag("pl"),        // Polish
    GetKnownLanguageTag("ps"),        // Pashto
    GetKnownLanguageTag("pt"),        // Portuguese
    GetKnownLanguageTag("pt-BR"),     // Portuguese (Brazil)
    GetKnownLanguageTag("pt-PT"),     // Portuguese (Portugal)
    GetKnownLanguageTag("qu"),        // Quechua
    GetKnownLanguageTag("rm"),        // Romansh
    GetKnownLanguageTag("ro"),        // Romanian
    GetKnownLanguageTag("ro-MD"),     // Moldavian
    GetKnownLanguageTag("ru"),        // Russian
    GetKnownLanguageTag("rw"),        // Kinyarwanda
    GetKnownLanguageTag("sa"),        // Sanskrit
    GetKnownLanguageTag("sd"),        // Sindhi
    GetKnownLanguageTag("sh"),        // Serbo-Croatian
    GetKnownLanguageTag("si"),        // Sinhalese
    GetKnownLanguageTag("sk"),        // Slovak
    GetKnownLanguageTag("sl"),        // Slovenian
    GetKnownLanguageTag("sm"),        // Samoan
    GetKnownLanguageTag("sn"),        // Shona
    GetKnownLanguageTag("so"),        // Somali
    GetKnownLanguageTag("sq"),        // Albanian
    GetKnownLanguageTag("sr"),        // Serbian
    GetKnownLanguageTag("st"),        // Sesotho
    GetKnownLanguageTag("su"),        // Sundanese
    GetKnownLanguageTag("sv"),        // Swedish
    GetKnownLanguageTag("sw"),        // Swahili
    GetKnownLanguageTag("ta"),        // Tamil
    GetKnownLanguageTag("te"),        // Telugu
    GetKnownLanguageTag("tg"),        // Tajik
    GetKnownLanguageTag("th"),        // Thai
    GetKnownLanguageTag("ti"),        // Tigrinya
    GetKnownLanguageTag("tk"),        // Turkmen
    GetKnownLanguageTag("tn"),        // Tswana
    GetKnownLanguageTag("to"),        // Tonga
    GetKnownLanguageTag("tr"),        // Turkish
    GetKnownLanguageTag("ts"),        // Tsonga
    GetKnownLanguageTag("tt"),        // Tatar
    GetKnownLanguageTag("ug"),        // Uyghur
    GetKnownLanguageTag("uk"),        // Ukrainian
    GetKnownLanguageTag("ur"),        // Urdu
    GetKnownLanguageTag("uz"),        // Uzbek
    GetKnownLanguageTag("vi"),        // Vietnamese
    GetKnownLanguageTag("wa"),        // Walloon
    GetKnownLanguageTag("wo"),        // Wolof
    GetKnownLanguageTag("xh"),        // Xhosa
    GetKnownLanguageTag("yi"),        // Yiddish
    GetKnownLanguageTag("yo"),        // Yoruba
    GetKnownLanguageTag("zh"),        // Chinese
    GetKnownLanguageTag("zh-CN"),     // Chinese (China)
    GetKnownLanguageTag("zh-HK"),     // Chinese (Hong Kong)
    GetKnownLanguageTag("zh-TW"),     // Chinese (Taiwan)
    GetKnownLanguageTag("zu"),        // Zulu
});

}  // namespace

const std::vector<LanguageTag>& GetAcceptLanguageTags() {
  static base::NoDestructor<std::vector<LanguageTag>> tags([] {
    std::vector<LanguageTag> tags;
    tags.reserve(std::size(kAcceptLanguageList));
    for (const LanguageTag& tag : kAcceptLanguageList) {
      tags.push_back(tag);
    }
    return tags;
  }());
  return *tags;
}

const LanguageTagMatcher& GetAcceptLanguageMatcher() {
  static base::NoDestructor<LanguageTagMatcher> matcher(
      LanguageTagMatcher::Create(GetAcceptLanguageTags()));
  return *matcher;
}

}  // namespace ui_l10n

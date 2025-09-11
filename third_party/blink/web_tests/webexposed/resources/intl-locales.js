// Languages with 50 million or more total speakers:
// https://en.wikipedia.org/wiki/List_of_languages_by_total_number_of_speakers#Ethnologue_(2025)
//
// This doesn't seem to correlate perfectly with locales set on user devices,
// but it does cover many very common locales.
//
// The choice of country (one or multiple), timezone and currency is a best
// effort to pick combinations that would be very common among users and
// websites serving those users.
const bigLocales = {
  // 1. English
  "en": {
    timeZone: "UTC",
    currency: null,
  },
  "en-GB": {
    timeZone: "Europe/London",
    currency: "GBP",
  },
  "en-US": {
    timeZone: "America/New_York",
    currency: "USD",
  },
  // 2. Mandarin Chinese
  "zh-CN": {
    timeZone: "Asia/Shanghai",
    currency: "CNY",
  },
  "zh-TW": {
    timeZone: "Asia/Taipei",
    currency: "TWD",
  },
  // 3. Hindi
  "hi": {
    timeZone: "Asia/Calcutta",
    currency: "INR",
  },
  // 4. Spanish
  "es": {
    timeZone: "UTC",
    currency: null,
  },
  "es-ES": {
    timeZone: "Europe/Madrid",
    currency: "EUR",
  },
  "es-MX": {
    timeZone: "America/Mexico_City",
    currency: "MXN",
  },
  // es-419 is Spanish Latin America and the Caribbean. Using the same timezone
  // as above to make differences easier to spot.
  "es-419": {
    timeZone: "America/Mexico_City",
    currency: null,
  },
  // 5. Modern Standard Arabic
  "ar": {
    timeZone: "UTC",
    currency: null,
  },
  // 6. French
  "fr": {
    timeZone: "Europe/Paris",
    currency: "EUR",
  },
  // 7. Bengali
  "bn": {
    timeZone: "Asia/Dhaka",
    currency: "BDT",
  },
  // 8. Portuguese
  "pt-PT": {
    timeZone: "Europe/Lisbon",
    currency: "EUR",
  },
  "pt-BR": {
    timeZone: "America/Sao_Paulo",
    currency: "BRL",
  },
  // 9. Russian
  "ru": {
    timeZone: "Europe/Moscow",
    currency: "RUB",
  },
  // 10. Indonesian
  "id": {
    timeZone: "Asia/Jakarta",
    currency: "IDR",
  },
  // 11. Urdu
  "ur": {
    timeZone: "Asia/Karachi",
    currency: "PKR",
  },
  // 12. Standard German
  "de": {
    timeZone: "Europe/Berlin",
    currency: "EUR",
  },
  // 13. Japanese
  "ja": {
    timeZone: "Asia/Tokyo",
    currency: "JPY",
  },
  // 14. Nigerian Pidgin
  // Omitted, does not seem to be a common locale.
  // 15. Egyptian Arabic
  "ar-EG": {
    timeZone: "Africa/Cairo",
    currency: "EGP",
  },
  // 16. Marathi
  "mr": {
    timeZone: "Asia/Kolkata",
    currency: "INR",
  },
  // 17. Vietnamese
  "vi": {
    timeZone: "Asia/Saigon",
    currency: "VND",
  },
  // 18. Telugu
  "te": {
    timeZone: "Asia/Kolkata",
    currency: "INR",
  },
  // 19. Hausa
  // Omitted, does not seem to be a common locale.
  // 20. Turkish
  "tr": {
    timeZone: "Europe/Istanbul",
    currency: "TRY",
  },
  // 21. Western Punjabi
  "pa": {
    timeZone: "Asia/Karachi",
    currency: "PKR",
  },
  // 22. Swahili
  "sw": {
    timeZone: "Africa/Dar_es_Salaam",
    currency: "TZS",
  },
  // 23. Tagalog
  "tl": {
    timeZone: "Asia/Manila",
    currency: "PHP",
  },
  "fil": {
    timeZone: "Asia/Manila",
    currency: "PHP",
  },
  // 24. Tamil
  "ta": {
    timeZone: "Asia/Kolkata",
    currency: "INR",
  },
  // 25. Yue Chinese
  "zh-HK": {
    timeZone: "Asia/Hong_Kong",
    currency: "HKD",
  },
  // 26. Wu Chinese
  // Omitted, does not seem to be a common locale.
  // 27. Iranian Persian
  "fa": {
    timeZone: "Asia/Tehran",
    currency: "IRR",
  },
  // 28. Korean
  "ko": {
    timeZone: "Asia/Seoul",
    currency: "KRW",
  },
  // 29. Thai
  "th": {
    timeZone: "Asia/Bangkok",
    currency: "THB",
  },
  // 30. Javanese
  "jv": {
    timeZone: "Asia/Jakarta",
    currency: "IDR",
  },
  // 31. Italian
  "it": {
    timeZone: "Europe/Rome",
    currency: "EUR",
  },
  // 32. Gujarati
  "gu": {
    timeZone: "Asia/Kolkata",
    currency: "INR",
  },
  // 33. Levantine Arabic
  "ar-SY": {
    timeZone: "Asia/Damascus",
    currency: "SYP",
  },
  // 34. Amharic
  "am": {
    timeZone: "Africa/Addis_Ababa",
    currency: "ETB",
  },
  // 35. Kannada
  "kn": {
    timeZone: "Asia/Kolkata",
    currency: "INR",
  },
  // 36. Bhojpuri
  // Omitted, does not seem to be a common locale.
  // 37. Sudanese Arabic
  "ar-SD": {
    timeZone: "Africa/Khartoum",
    currency: "SDG",
  },
};

// Additional locales where changes might be risky.
const extraLocales = {
  // Dutch and Polish  are in the top 15 of several of the lists on this page:
  // https://en.wikipedia.org/wiki/Languages_used_on_the_Internet
  // Malay isn't included in the Ethnologue
  "nl": {
    timeZone: "Europe/Amsterdam",
    currency: "EUR",
  },
  "pl": {
    timeZone: "Europe/Warsaw",
    currency: "PLN",
  },
  // Swedish is sometimes used as a fallback to get ISO 8601 date formatting:
  // https://stackoverflow.com/q/25050034
  "sv-SE": {
    timeZone: "Europe/Stockholm",
    currency: "SEK",
  },
};

export const locales = Object.assign({}, bigLocales, extraLocales);

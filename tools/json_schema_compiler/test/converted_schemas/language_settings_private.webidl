// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum MoveType { "TOP", "UP", "DOWN", "UNKNOWN" };

dictionary Language {
  // The unique code identifying the language.
  required DOMString code;

  // The name of the language, in the current UI language.
  required DOMString displayName;

  // The name of the language as it is in its own language.
  required DOMString nativeDisplayName;

  // Whether the UI can be displayed in this language. Defaults to false.
  boolean supportsUI;

  // Whether this language can be used for spell checking. Defaults to false.
  boolean supportsSpellcheck;

  // Whether this language has translations for the current target language.
  // Defaults to false.
  boolean supportsTranslate;

  // Whether this language is prohibited as a UI locale (not in the list of
  // the 'AllowedLanguages' policy). Defaults to false.
  boolean isProhibitedLanguage;
};

dictionary SpellcheckDictionaryStatus {
  // The language code of the dictionary that the status describes.
  required DOMString languageCode;

  // Whether the dictionary is ready (has been loaded from disk or
  // successfully downloaded).
  required boolean isReady;

  // Whether the dictionary is being downloaded. Defaults to false.
  boolean isDownloading;

  // Whether the dictionary download failed. Defaults to false.
  boolean downloadFailed;
};

dictionary InputMethod {
  // The ID of the input method descriptor.
  required DOMString id;

  // The human-readable name of the input method.
  required DOMString displayName;

  // The language codes this input method supports.
  required sequence<DOMString> languageCodes;

  // The search terms for the input method.
  required sequence<DOMString> tags;

  // True if the input method is enabled.
  boolean enabled;

  // True if the input method extension has an options page.
  boolean hasOptionsPage;

  // True if the input method is not allowed by policy.
  boolean isProhibitedByPolicy;
};

dictionary InputMethodLists {
  // The list of component extension input methods.
  required sequence<InputMethod> componentExtensionImes;

  // The list of third-party extension input methods.
  required sequence<InputMethod> thirdPartyExtensionImes;
};

// Called when the pref for the dictionaries used for spell checking changes
// or the status of one of the spell check dictionaries changes.
callback OnSpellcheckDictionariesChangedListener =
    undefined (sequence<SpellcheckDictionaryStatus> statuses);

interface OnSpellcheckDictionariesChangedEvent : ExtensionEvent {
  static undefined addListener(
      OnSpellcheckDictionariesChangedListener listener);
  static undefined removeListener(
      OnSpellcheckDictionariesChangedListener listener);
  static boolean hasListener(OnSpellcheckDictionariesChangedListener listener);
};

// Called when words are added to and/or removed from the custom spell check
// dictionary.
callback OnCustomDictionaryChangedListener = undefined (
    sequence<DOMString> wordsAdded, sequence<DOMString> wordsRemoved);

interface OnCustomDictionaryChangedEvent : ExtensionEvent {
  static undefined addListener(OnCustomDictionaryChangedListener listener);
  static undefined removeListener(OnCustomDictionaryChangedListener listener);
  static boolean hasListener(OnCustomDictionaryChangedListener listener);
};

// Called when an input method is added.
callback OnInputMethodAddedListener = undefined (DOMString inputMethodId);

interface OnInputMethodAddedEvent : ExtensionEvent {
  static undefined addListener(OnInputMethodAddedListener listener);
  static undefined removeListener(OnInputMethodAddedListener listener);
  static boolean hasListener(OnInputMethodAddedListener listener);
};

// Called when an input method is removed.
callback OnInputMethodRemovedListener = undefined (DOMString inputMethodId);

interface OnInputMethodRemovedEvent : ExtensionEvent {
  static undefined addListener(OnInputMethodRemovedListener listener);
  static undefined removeListener(OnInputMethodRemovedListener listener);
  static boolean hasListener(OnInputMethodRemovedListener listener);
};

// Use the <code>chrome.languageSettingsPrivate</code> API to get or change
// language and input method settings.
interface LanguageSettingsPrivate {
  // Gets languages available for translate, spell checking, input and locale.
  // |PromiseValue|: languages
  static Promise<sequence<Language>> getLanguageList();

  // Enables a language, adding it to the Accept-Language list (used to decide
  // which languages to translate, generate the Accept-Language header, etc.).
  static undefined enableLanguage(DOMString languageCode);

  // Disables a language, removing it from the Accept-Language list.
  static undefined disableLanguage(DOMString languageCode);

  // Enables or disables translation for a given language.
  static undefined setEnableTranslationForLanguage(DOMString languageCode,
                                                   boolean enable);

  // Moves a language inside the language list.
  static undefined moveLanguage(DOMString languageCode, MoveType moveType);

  // Gets languages that should always be automatically translated.
  // |PromiseValue|: languageCodes
  static Promise<sequence<DOMString>> getAlwaysTranslateLanguages();

  // Sets whether a given language should always be automatically translated.
  static undefined setLanguageAlwaysTranslateState(DOMString languageCode,
                                                   boolean alwaysTranslate);

  // Gets languages that should never be offered to translate.
  // |PromiseValue|: languageCodes
  static Promise<sequence<DOMString>> getNeverTranslateLanguages();

  // Gets the current status of the chosen spell check dictionaries.
  // |PromiseValue|: status
  static Promise<sequence<SpellcheckDictionaryStatus>>
  getSpellcheckDictionaryStatuses();

  // Gets the custom spell check words, in sorted order.
  // |PromiseValue|: words
  static Promise<sequence<DOMString>> getSpellcheckWords();

  // Adds a word to the custom dictionary.
  static undefined addSpellcheckWord(DOMString word);

  // Removes a word from the custom dictionary.
  static undefined removeSpellcheckWord(DOMString word);

  // Gets the translate target language (in most cases, the display locale).
  // |PromiseValue|: languageCode
  static Promise<DOMString> getTranslateTargetLanguage();

  // Sets the translate target language given a language code.
  static undefined setTranslateTargetLanguage(DOMString languageCode);

  // Gets all supported input methods, including third-party IMEs.
  // Chrome OS only.
  // |PromiseValue|: lists
  static Promise<InputMethodLists> getInputMethodLists();

  // Adds the input method to the current user's list of enabled input methods,
  // enabling the input method for the current user. Chrome OS only.
  static undefined addInputMethod(DOMString inputMethodId);

  // Removes the input method from the current user's list of enabled input
  // methods, disabling the input method for the current user. Chrome OS only.
  static undefined removeInputMethod(DOMString inputMethodId);

  // Tries to download the dictionary after a failed download.
  static undefined retryDownloadDictionary(DOMString languageCode);

  // Called when the pref for the dictionaries used for spell checking changes
  // or the status of one of the spell check dictionaries changes.
  static attribute OnSpellcheckDictionariesChangedEvent
      onSpellcheckDictionariesChanged;

  // Called when words are added to and/or removed from the custom spell check
  // dictionary.
  static attribute OnCustomDictionaryChangedEvent onCustomDictionaryChanged;

  // Called when an input method is added.
  static attribute OnInputMethodAddedEvent onInputMethodAdded;

  // Called when an input method is removed.
  static attribute OnInputMethodRemovedEvent onInputMethodRemoved;
};

partial interface Browser {
  static attribute LanguageSettingsPrivate languageSettingsPrivate;
};

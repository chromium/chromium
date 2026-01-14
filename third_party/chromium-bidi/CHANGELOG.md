# Changelog

All notable changes to this project will be documented in this file.

## [12.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v12.0.1...chromium-bidi-v12.1.0) (2026-01-14)


### Features

* implement `emulation.setTouchOverride` ([#3952](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3952)) ([0a69424](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0a69424946e11b03891a38585b33a6fdae9cc0a2))

## [12.0.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v12.0.0...chromium-bidi-v12.0.1) (2025-12-15)


### Bug Fixes

* reliably detect default user context ([#3947](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3947)) ([4184afc](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4184afc241f75bf79d19eb5f38bd568bd4e8e9ec))

## [12.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v11.0.1...chromium-bidi-v12.0.0) (2025-12-12)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 145.0.7563.0 ([#3933](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3933))

### Features

* **chrome:** update the pinned browser version to 145.0.7563.0 ([#3933](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3933)) ([dfd6104](https://github.com/GoogleChromeLabs/chromium-bidi/commit/dfd6104b14ca47efce0e9f1f75e4c0445fa6a3d5))
* emulation.setScreenSettingsOverride ([#3943](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3943)) ([c3ffde0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c3ffde028b11d26f535380a4df26b3b73d27d250))


### Bug Fixes

* ensure `network.beforeRequestSent` is emitted before `network.authRequired` ([#3941](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3941)) ([5072f49](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5072f49367bd94678aa12d744cf0df88818b03f0))
* extra headers can have duplicates ([#3937](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3937)) ([aabd783](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aabd7831255a90b01afcc59c40992cc0c3e7cdbe))
* round cookie expiry field ([#3938](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3938)) ([66c4695](https://github.com/GoogleChromeLabs/chromium-bidi/commit/66c46958fa9f8ebe15d119a42b7a8dc9427f18dd))


### Reverts

* "fix: extra headers can have duplicates" ([#3939](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3939)) ([4a9b011](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4a9b011956055a5e4d098912a361c1699d4c39d4))

## [11.0.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v11.0.0...chromium-bidi-v11.0.1) (2025-11-14)


### Bug Fixes

* correctly report data sizes ([#3836](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3836)) ([154abaa](https://github.com/GoogleChromeLabs/chromium-bidi/commit/154abaaec7e35dc86bcc9dcfd9384d58a65cca08))

## [11.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.6.1...chromium-bidi-v11.0.0) (2025-11-07)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 144.0.7505.0 ([#3870](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3870))

### Features

* **chrome:** update the pinned browser version to 144.0.7505.0 ([#3870](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3870)) ([178b335](https://github.com/GoogleChromeLabs/chromium-bidi/commit/178b3356efbf50cdd9e27faeb333fa936d901968))
* respect `emulation.setLocaleOverride` in `navigator.language` and `Accept-Language` ([#3884](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3884)) ([bc755f4](https://github.com/GoogleChromeLabs/chromium-bidi/commit/bc755f47029bb93758a25e3f66f3d414a0792d07))


### Bug Fixes

* emulations per context vs per user context ([#3885](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3885)) ([8af52e0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8af52e09e91488cb66150459c8309aa10fa2fb51))
* update `viewport` and `screenOrientation` in parallel ([#3892](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3892)) ([b0ed10d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b0ed10d556d52c8a73ddb404675f4d7f7b1afab0))

## [10.6.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.6.0...chromium-bidi-v10.6.1) (2025-10-22)


### Bug Fixes

* do not merge headers with the same names ([#3864](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3864)) ([ae6cec6](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ae6cec6cb4b503a25cb78dfcb591d262200f1a56))

## [10.6.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.5.1...chromium-bidi-v10.6.0) (2025-10-20)


### Features

* provide proper network `destination` on navigation ([#3859](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3859)) ([4fadce5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4fadce503eb208267f5c47af298af84b6aa5e25e))

## [10.5.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.5.0...chromium-bidi-v10.5.1) (2025-10-17)


### Bug Fixes

* data collection do not influence redirect network events ([#3854](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3854)) ([c7297af](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c7297afd692b248904d1f9a672225431be535060))

## [10.5.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.4.0...chromium-bidi-v10.5.0) (2025-10-17)


### Features

* get request body size from Content-Length ([#3851](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3851)) ([69c0c0f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/69c0c0febec022d60e5c5f6d46b10acf4e2ccb8c))

## [10.4.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.3.1...chromium-bidi-v10.4.0) (2025-10-15)


### Features

* allow for disabling durable messages ([#3848](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3848)) ([e6b28ac](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e6b28ac25990bb997074d4505415d1a8fc9e281a))

## [10.3.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.3.0...chromium-bidi-v10.3.1) (2025-10-15)


### Bug Fixes

* don't crash on worker's requests ([#3845](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3845)) ([c0edf96](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c0edf9685d001d52e7e70d2fe063528d8f5e1c22))

## [10.3.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.2.0...chromium-bidi-v10.3.0) (2025-10-14)


### Features

* `network.getData` survives navigations ([#3826](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3826)) ([047ca85](https://github.com/GoogleChromeLabs/chromium-bidi/commit/047ca85a6b20c8c5b34656ce52a50a1b842aa917))
* validate headers set in `network.setExtraHeaders` ([#3832](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3832)) ([2f2238c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2f2238ce54fb3dbb51da36b6a2da1b2ab803c3b5))


### Bug Fixes

* merge `network.setExtraHeaders` for browsing context, user context and global ([#3840](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3840)) ([fbe7018](https://github.com/GoogleChromeLabs/chromium-bidi/commit/fbe7018e32ed84174cd93c96390a9fb5701a7d72))

## [10.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.1.0...chromium-bidi-v10.2.0) (2025-10-10)


### Features

* add `embeddedOrigin` in `permissions.setPermission` ([#3799](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3799)) ([fa1ea6f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/fa1ea6f4a3a0df4f86be81c68081ae6e13396d59))

## [10.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v10.0.0...chromium-bidi-v10.1.0) (2025-10-09)


### Features

* implement `emulation.setNetworkConditions:offline` ([#3819](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3819)) ([cc702ca](https://github.com/GoogleChromeLabs/chromium-bidi/commit/cc702caf1e249e4e6141fc9fa75d374027529569))

## [10.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v9.2.1...chromium-bidi-v10.0.0) (2025-10-07)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 143.0.7447.0 ([#3802](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3802))

### Features

* **chrome:** update the pinned browser version to 143.0.7447.0 ([#3802](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3802)) ([d40b75f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d40b75f0ae7e0b9ce7e58a72ac849f898b88d2f0))
* speculation module implementation  ([#3731](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3731)) ([66d3585](https://github.com/GoogleChromeLabs/chromium-bidi/commit/66d3585e0062f21adcabcc85ceb686ee515f687f))
* support request data collection ([#3809](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3809)) ([ac9792f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ac9792f643e601ee749b6ef35b4e03302b5d1b3c))
* support request post data collection ([#3815](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3815)) ([6123a55](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6123a55bb766967420e1dbfd08ad54735a64ffc0))

## [9.2.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v9.2.0...chromium-bidi-v9.2.1) (2025-10-02)


### Bug Fixes

* don't report interception for DataUrl and servedFromCache ([#3797](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3797)) ([de963da](https://github.com/GoogleChromeLabs/chromium-bidi/commit/de963da9ffc5cf30362b152f992d1aedb953e3aa))

## [9.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v9.1.0...chromium-bidi-v9.2.0) (2025-09-29)


### Features

* respect `network.addDataCollector: maxEncodedDataSize` param ([#3782](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3782)) ([7471cee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7471cee0a91330ff4d8799ca833f4d565a5626dd))

## [9.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v9.0.0...chromium-bidi-v9.1.0) (2025-09-23)


### Features

* `emulation.setUserAgentOverride` ([#3661](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3661)) ([3df5567](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3df5567018e94fbec855719539468e81a57d4b58))
* implement `browser.setDownloadBehavior` ([#3604](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3604)) ([aa270ee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aa270ee6b6e2cc5ce77d8ec6c99a60b5cbabc26c))


### Bug Fixes

* allow for setting global default download behavior ([#3761](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3761)) ([7d72d13](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7d72d130e9f9c877857cff9e05404857eb6741a1))
* remove config settings on null values ([#3767](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3767)) ([38ff2f9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/38ff2f9f563ed41c68376f75cd8bedfb3f392fd4))
* remove unsubscribe by context ([#3733](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3733)) ([a3dc164](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a3dc164028270ab9d7e081b4097ccb606ab63927))
* **wdio-xvfb:** &lt;code&gt;autoXvfb&lt;/code&gt; should disable xvfb completely (&lt;a ([88cd6c4](https://github.com/GoogleChromeLabs/chromium-bidi/commit/88cd6c46b727749da9f6fc503b367ada43055175))

## [9.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v8.1.0...chromium-bidi-v9.0.0) (2025-09-09)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 142.0.7394.0 ([#3682](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3682))

### Features

* **chrome:** update the pinned browser version to 142.0.7394.0 ([#3682](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3682)) ([76a9580](https://github.com/GoogleChromeLabs/chromium-bidi/commit/76a95807035e44dc5e0bd9f1d0c17ced44d4d4a9))


### Bug Fixes

* correct broken realm lookup ([#3703](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3703)) ([031ba15](https://github.com/GoogleChromeLabs/chromium-bidi/commit/031ba1564de3730ea97f90936bc564370be8f168))
* do no duplicate headers ([#3701](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3701)) ([9c384d4](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9c384d41f1ec22153ca8b0121385b9478dfa1e13))
* handle target config errors gracefully ([#3699](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3699)) ([fd88aa5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/fd88aa54fd51db2ed4d6aabd9c05aecce6d105be))

## [8.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v8.0.0...chromium-bidi-v8.1.0) (2025-09-03)


### Features

* do not dispose request until all the collectors are gone ([#3669](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3669)) ([909176c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/909176cb4bbc763524ae8efb6458ed005f5a9487))


### Bug Fixes

* support network.getData on OOPiF ([#3674](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3674)) ([403dc7c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/403dc7cacf59aecede1e68e6fae61b6edeaacdaa))

## [8.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.3.2...chromium-bidi-v8.0.0) (2025-08-13)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 141.0.7354.0 ([#3628](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3628))

### Features

* **chrome:** update the pinned browser version to 141.0.7354.0 ([#3628](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3628)) ([bf73edf](https://github.com/GoogleChromeLabs/chromium-bidi/commit/bf73edfd432794778e0a6a427a27924dae6255d6))
* implement `emulation.setScriptingEnabled` command ([#3566](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3566)) ([a4a4033](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a4a40334f287b4b5dfe0b020eb5aa196c44994f0))
* support user prompts on OOPiF ([#3647](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3647)) ([73a4ff6](https://github.com/GoogleChromeLabs/chromium-bidi/commit/73a4ff6f1e9312f4cfe9f741d1f48708b74fdbf0))

## [7.3.2](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.3.1...chromium-bidi-v7.3.2) (2025-08-08)


### Features

* allow for empty headers in `network.setExtraHeaders` ([#3636](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3636)) ([510f2e9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/510f2e9750f192cde46c28fa70e609c28935df04))

## [7.3.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.3.0...chromium-bidi-v7.3.1) (2025-08-07)


### Bug Fixes

* do not override context config with undefined ([#3633](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3633)) ([edff136](https://github.com/GoogleChromeLabs/chromium-bidi/commit/edff13666effe34e863cdf32fdd95ad0ed6393c7))

## [7.3.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.2.0...chromium-bidi-v7.3.0) (2025-08-07)


### Features

* implement `network.setExtraHeaders` ([#3629](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3629)) ([937d719](https://github.com/GoogleChromeLabs/chromium-bidi/commit/937d719a8ed29dc25bf51d1f166ec1f10bc954fd))


### Bug Fixes

* allow svg element as a start node ([#3614](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3614)) ([32e4657](https://github.com/GoogleChromeLabs/chromium-bidi/commit/32e46573ee42c043c1462d5eea0a09999b0fa314))
* respect configs in OOPiF ([#3630](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3630)) ([c66d125](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c66d125b659c704242111570782cb20afb4b3f75))

## [7.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.1.1...chromium-bidi-v7.2.0) (2025-07-21)


### Features

* implement `emulation.setTimezoneOverride` ([#3589](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3589)) ([1b7acaa](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1b7acaae8e30194fb39e1d5d57b01effb1d2f4bd))
* support timezone offset emulation ([#3591](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3591)) ([a645f8e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a645f8e8956e67ee86b6f7e82a5f42912802d5cd))

## [7.1.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.1.0...chromium-bidi-v7.1.1) (2025-07-16)


### Bug Fixes

* associate preflight network request with context ([#3572](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3572)) ([0751fa9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0751fa96b3ad3e79da2396084f3d15ea12011bca))

## [7.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v7.0.0...chromium-bidi-v7.1.0) (2025-07-09)


### Features

* Extend `browser.createUserContext` with `unhandledPromptBehavior` parameter ([#3440](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3440)) ([de72d10](https://github.com/GoogleChromeLabs/chromium-bidi/commit/de72d10875fb77c6908cb116bb46a6b3d49491b7))
* implement network data collectors ([#3546](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3546)) ([6e7f127](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6e7f1270fd1a1a3292ba417eef21d1863000a30a))

## [7.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v6.1.0...chromium-bidi-v7.0.0) (2025-06-27)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 140.0.7259.0 ([#3524](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3524))

### Features

* **chrome:** update the pinned browser version to 140.0.7259.0 ([#3524](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3524)) ([784add0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/784add0cce2dc8756763e9a9d0ef54c28627e092))
* implement `emulation.setLocaleOverride` ([#3425](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3425)) ([8cefe61](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8cefe61e51ee9d706d38dea61c18740225b93bae))
* support `emulation.setScreenOrientationOverride` ([#3439](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3439)) ([0b17774](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0b177749a66e0fe5b9c395d0a05199ae4c246eb1))

## [6.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v6.0.0...chromium-bidi-v6.1.0) (2025-06-25)


### Features

* implement `browsingContext.downloadFinished` event ([#3427](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3427)) ([ea566e5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ea566e5f88bf70ee501b928ab14d2ec8730039b7))

## [6.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v5.3.1...chromium-bidi-v6.0.0) (2025-06-16)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 139.0.7215.0 ([#3450](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3450))

### Features

* **chrome:** update the pinned browser version to 139.0.7215.0 ([#3450](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3450)) ([4c53302](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4c5330210d3c737ab81bf57a93109bef83f04f5a))
* Support GATT descriptor event ([#3443](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3443)) ([41eddf8](https://github.com/GoogleChromeLabs/chromium-bidi/commit/41eddf84d7d3b38ec4d971bf94a5e8572e13b148))

## [5.3.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v5.3.0...chromium-bidi-v5.3.1) (2025-05-23)


### Bug Fixes

* generated entrypoints were incorrect ([#3436](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3436)) ([a62ec95](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a62ec9500716856d408cfe81a5689b57dbc7f8ec))

## [5.3.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v5.2.0...chromium-bidi-v5.3.0) (2025-05-22)


### Features

* `proxy` configuration per user context ([#3430](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3430)) ([95623ac](https://github.com/GoogleChromeLabs/chromium-bidi/commit/95623ac9548ad9a72924c193d3220b8d10895222))
* Support GATT characteristic event ([#3404](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3404)) ([d0eeb2d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d0eeb2d67bd8ee07bde9e151d74b9184c3c2f4aa))

## [5.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v5.1.0...chromium-bidi-v5.2.0) (2025-05-15)


### Features

* create the main entrypoint module ([#3376](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3376)) ([421892a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/421892a84e5182062bab1a922b8dce65752eb345))
* support `acceptInsecureCerts` per user context ([#3399](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3399)) ([a00309f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a00309f94825fed5e65e28617782ee197ab0d140))
* support GATT characteristic and descriptor simulation ([#3385](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3385)) ([89bff5e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/89bff5ee603f4c705792e3215d51ff854d9d6944))
* support GATT disconnection and service simulation ([#3383](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3383)) ([d201788](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d2017887389f3e1a0cf94c4ca868c4791def5ca4))

## [5.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v5.0.0...chromium-bidi-v5.1.0) (2025-05-06)


### Features

* support GATT connection simulation ([#3366](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3366)) ([b165c7a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b165c7a6256b289d3a685024265fa9ae5107aa29))


### Bug Fixes

* allow no frameId in javascript dialog events ([#3372](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3372)) ([6d01991](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6d019914a2012e2111e0bacbb110ae77711ccfdf))

## [5.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v4.2.0...chromium-bidi-v5.0.0) (2025-05-05)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 138.0.7155.0 ([#3357](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3357))

### Features

* **chrome:** update the pinned browser version to 138.0.7155.0 ([#3357](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3357)) ([c1d0253](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c1d0253da0f33bb34a570f983d504307fedf2098))


### Bug Fixes

* add .js extension to imports ([#3369](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3369)) ([d08a34b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d08a34b0c4d4b82177e6f19ad56c884e35c866bc))

## [4.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v4.1.1...chromium-bidi-v4.2.0) (2025-04-30)


### Features

* emulate geolocation positionUnavailable ([#3359](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3359)) ([6975bcc](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6975bcc8fa626d8620934328d25496998dcf95f8))
* provide proper context id in `browsingContext.userPrompt` ([#3349](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3349)) ([e504ba1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e504ba19c93381f874798e40ee1050d7122f55e7))

## [4.1.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v4.1.0...chromium-bidi-v4.1.1) (2025-04-24)


### Bug Fixes

* export cjs modules for esm ([#3342](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3342)) ([a5088c4](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a5088c46d76967d2681e758402c647c2a0c3c463))

## [4.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v4.0.1...chromium-bidi-v4.1.0) (2025-04-23)


### Features

* emulate geolocation altitude, altitudeAccuracy, heading and speed ([#3337](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3337)) ([f8e4e34](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f8e4e342c290e3d3b0fc4ee28204a7b8325e9872))

## [4.0.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v4.0.0...chromium-bidi-v4.0.1) (2025-04-15)


### Bug Fixes

* allow esm imports ([#3326](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3326)) ([8790d0d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8790d0dff7d8f89941135845512e8200c457ce6b))
* emit `browsingContext.userPrompt` only once ([#3325](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3325)) ([f41fd79](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f41fd793677af2467832d3becbb72f49db59ed35))
* string `unhandledPromptBehavior` capability implies `beforeUnload: accept` behavior ([#3318](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3318)) ([baa99ae](https://github.com/GoogleChromeLabs/chromium-bidi/commit/baa99ae0186ecee00f30db1d986ad37887dddb6f))

## [4.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v3.2.1...chromium-bidi-v4.0.0) (2025-04-14)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 137.0.7104.0 ([#3264](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3264))

### Features

* **chrome:** update the pinned browser version to 137.0.7104.0 ([#3264](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3264)) ([b85d415](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b85d415cc8397443ddb724b74cf3b548e8abc3a8))
* implement `emulation.setGeolocationOverride` ([#3311](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3311)) ([7b8b52d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7b8b52d57f2a936e77995d2e67da192d6cbbd56c))

## [3.2.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v3.2.0...chromium-bidi-v3.2.1) (2025-04-02)


### Bug Fixes

* preload scripts order ([#3271](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3271)) ([43d84a7](https://github.com/GoogleChromeLabs/chromium-bidi/commit/43d84a75d154d76a0b8e3cd3f35b7d01e176da14))

## [3.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v3.1.0...chromium-bidi-v3.2.0) (2025-03-31)


### Features

* support UserContext in setViewport ([#3262](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3262)) ([aa01302](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aa0130294710faee22bfae8a0069b976532a072d))

## [3.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v3.0.0...chromium-bidi-v3.1.0) (2025-03-27)


### Features

* `browsingContext.downloadWillBegin` event ([#3248](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3248)) ([209fac9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/209fac966f5b5089260a8dfd336cefc3c80cf495))
* provide proper `clientWindow` ([#3253](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3253)) ([8d24bf3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8d24bf3e7c62955f91ef878928da10f49385063e))
* support for disable Bluetooth simulation ([#3255](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3255)) ([9db4f0b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9db4f0bbc40e24456ba6340b6eb075c7ef7869a5))

## [3.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v2.1.2...chromium-bidi-v3.0.0) (2025-03-12)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 136.0.7059.0 ([#3196](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3196))

### Features

* `input.fileDialogOpened` + `unhandledPromptBehavior.file` ([#3155](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3155)) ([729606e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/729606e57609c7ccd81a0b18dd2a3270f2f79cc1))
* cancel intercepted file dialog ([#3205](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3205)) ([1829f9a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1829f9a39f0571f3f417a12e790c086e3e11b46f))
* **chrome:** update the pinned browser version to 136.0.7059.0 ([#3196](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3196)) ([0197dfe](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0197dfecef4c5b64dcebed8c407a709a67045f5c))

## [2.1.2](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v2.1.1...chromium-bidi-v2.1.2) (2025-03-03)


### Bug Fixes

* update handling of extension data types ([#3169](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3169)) ([dcd7316](https://github.com/GoogleChromeLabs/chromium-bidi/commit/dcd7316b51cf7c3a5c68a2e99a305d6d618a28ab))

## [2.1.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v2.1.0...chromium-bidi-v2.1.1) (2025-02-28)


### Bug Fixes

* throw no such web extension error ([#3160](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3160)) ([2271b0a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2271b0a09ef41745365b147acb234e80863ca739))

## [2.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v2.0.0...chromium-bidi-v2.1.0) (2025-02-28)


### Features

* webExtension module ([#3012](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3012)) ([3994b79](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3994b79aa92f33b299808f1f644ab309755b7fa9))


### Bug Fixes

* hide cdp.send after the mapper receives it ([#3137](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3137)) ([9ad3d11](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9ad3d113b1594aa1318ad2223292e5bc0b149566))

## [2.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v1.3.0...chromium-bidi-v2.0.0) (2025-02-18)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 135.0.7000.0 ([#3081](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3081))

### Features

* **chrome:** update the pinned browser version to 135.0.7000.0 ([#3081](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3081)) ([6061a41](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6061a4173460ebc355f0818414b3819aa0bf20b9))
* wait `none` waits for navigation to be committed ([#3086](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3086)) ([b8f30b1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b8f30b179c4c336c131741de900a19b67ed4c5f0))


### Bug Fixes

* **network:** propagate unsafe header error as invalid  ([#3100](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3100)) ([0a2ce3b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0a2ce3bb95da7b2c78b19add74dd5c6788c2190b))

## [1.3.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v1.2.0...chromium-bidi-v1.3.0) (2025-02-05)


### Features

* emit `browsingContext.navigationCommitted` event ([#3057](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3057)) ([58be24c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/58be24cfa09a04f6cf7d1068cfe697baee32298b))
* implement user context subscriptions ([#3039](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3039)) ([7decf27](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7decf27c36bc0a6b8d268472e260920300d49db2))
* support UserContext in PreloadScripts ([#3072](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3072)) ([dd2c9c0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/dd2c9c0ee62f761903c20c6d83ea2c0afaa7bf73))


### Bug Fixes

* group clicks in action ([#3052](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3052)) ([c2d7fe3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c2d7fe3e1d6c93718f7279dd05bc5a7051281c54))

## [1.2.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v1.1.0...chromium-bidi-v1.2.0) (2025-01-28)


### Features

* implement unsubscribe by id ([#3013](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3013)) ([5f8752d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5f8752d9032aca1d6766922113778701d0228fc0))


### Bug Fixes

* allow `bluetooth.requestDevicePromptUpdated` subscription ([#3044](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3044)) ([dd4751d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/dd4751d8bd4b9949a34cf568f20a5185d195c524))
* fix unsubscribe by id parser ([#3034](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3034)) ([a51a55d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a51a55d904a51a5c02480963846a0e8f77e569c6))
* handle max depth property ([#3027](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3027)) ([1393215](https://github.com/GoogleChromeLabs/chromium-bidi/commit/139321570632cd9567c488bd96b24b2b332d1398))
* rely on frameSubtreeWillBeDetached to emit contextDestroyed events ([#3029](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3029)) ([0d04e0b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0d04e0b72d6bab22b9f715740dd48af5e32d1b4e))
* support correct to base64 for non-latin ([#3031](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3031)) ([6fe665f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6fe665fead31d1a00a463d7c1ab5706abf416292)), closes [#3019](https://github.com/GoogleChromeLabs/chromium-bidi/issues/3019)

## [1.1.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v1.0.0...chromium-bidi-v1.1.0) (2025-01-17)


### Features

* support `goog:channel` along with `channel` ([#2996](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2996)) ([d7ed911](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d7ed9112dd3c81f893f9a4e902ffa6e237e7d5ac))


### Bug Fixes

* multi headers reporting incorrectly ([#2987](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2987)) ([0e58eb7](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0e58eb7d43b859b477686aedde6125169b0f14ad))
* wait for context should respect already existing contexts ([#2998](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2998)) ([f6cb8ec](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f6cb8ec43e9b915f3de7bf51d3627db6b12b2d51))

## [1.0.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.12.0...chromium-bidi-v1.0.0) (2025-01-15)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 134.0.6958.0 ([#2986](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2986))

### Features

* **chrome:** update the pinned browser version to 134.0.6958.0 ([#2986](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2986)) ([7e57049](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7e57049d23a6a36042d0389fc636c43a2e2a5bf5))
* heuristic for request's `initiatorType` and `destination` ([#2947](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2947)) ([357d5be](https://github.com/GoogleChromeLabs/chromium-bidi/commit/357d5be2efe030c2fbfeb0276fbc07dc4195b731))
* implement context locator ([#2968](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2968)) ([5bf3b18](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5bf3b1829eb3041b5a51ad9ef5c7082b7fb16d98))
* implement subscription IDs ([#2954](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2954)) ([23642a4](https://github.com/GoogleChromeLabs/chromium-bidi/commit/23642a40bad6cbe475f78bcdefcff497e8876ccb))


### Bug Fixes

* **network:** url for interception ([#2962](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2962)) ([2a3d277](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2a3d277da0c453684da6487bcd01c708f57c97ce))
* wait for fragment navigation to finish before finishing navigation command ([#2964](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2964)) ([b761bc3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b761bc301f011afa96fa0167ae8f9550f6471656))

## [0.12.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.11.1...chromium-bidi-v0.12.0) (2025-01-03)


### Features

* provide proper network timings ([#2919](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2919)) ([22e096b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/22e096bf65d86137de66d71f2728f0d3036d0d66))


### Bug Fixes

* regression in serialization ([#2928](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2928)) ([84f9983](https://github.com/GoogleChromeLabs/chromium-bidi/commit/84f99837c670d200e5ea5de9743e7fe17a54464e))

## [0.11.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.11.0...chromium-bidi-v0.11.1) (2024-12-18)


### Features

* throw a proper error if the browsing context was destroyed during action dispatching ([#2908](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2908)) ([62c3005](https://github.com/GoogleChromeLabs/chromium-bidi/commit/62c300595b9adab713a4af8c226b293eded3968a))

## [0.11.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.10.2...chromium-bidi-v0.11.0) (2024-12-17)


### ⚠ BREAKING CHANGES

* align navigation started with the spec

### Features

* abort navigation only after frame navigated ([#2898](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2898)) ([6c3d406](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6c3d406296ad70ca51eca5dd7ec92b8e62843c95))


### Bug Fixes

* align navigation started with the spec ([960531f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/960531f4663a74de0dc5623889357d5c80172dbf))

## [0.10.2](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.10.1...chromium-bidi-v0.10.2) (2024-12-11)


### Bug Fixes

* avoid extra getFrameOwner call ([#2839](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2839)) ([0ff2876](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0ff28760906e411635c9685cf4a6ba34fdd02183))
* implement the pattern matching according to the spec  ([#2832](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2832)) ([4563b2b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4563b2b1a365c2518f61fc2e2922ccd099b5c238))
* stop calling bringToFront before taking screenshots ([#2830](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2830)) ([6017898](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6017898ea4d06a5531ddf690bedbaa25c2147f0e))

## [0.10.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.10.0...chromium-bidi-v0.10.1) (2024-11-25)


### Features

* do not emit initial navigation events ([#2796](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2796)) ([c8c9cdf](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c8c9cdfe1cc28448b8d136bf575a87917cbc9325))
* implement `browser.getClientWindows` ([#2780](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2780)) ([7b91906](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7b9190627000b839dbe2a903d464a4715d980e72))


### Bug Fixes

* `browser.getClientWindows` returns a unique per window value ([#2783](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2783)) ([0f97130](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0f971303281aba1910786035facc5eb54a833232))

## [0.10.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.9.1...chromium-bidi-v0.10.0) (2024-11-13)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 133.0.6835.0 ([#2758](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2758))

### Features

* **chrome:** update the pinned browser version to 133.0.6835.0 ([#2758](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2758)) ([1aa0a5e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1aa0a5e3df21ff7a6bf3caf0554cdf0d6ecb0d01))


### Bug Fixes

* allow setting `bluetooth.simulateAdapter` multiple times ([#2762](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2762)) ([3963880](https://github.com/GoogleChromeLabs/chromium-bidi/commit/39638809e878cd0db5cd40d65489816033ad51b4))

## [0.9.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.9.0...chromium-bidi-v0.9.1) (2024-11-06)


### Reverts

* "chore: exclude targets that we do not interact with" ([#2736](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2736)) ([2a061d2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2a061d2808b6171eaf097caba2e88c285cf4deb8))

## [0.9.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.8.1...chromium-bidi-v0.9.0) (2024-10-23)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 132.0.6779.0 ([#2680](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2680))

### Features

* align abort navigation with the spec ([#2715](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2715)) ([6edf07b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6edf07b9891e9cc75a5af17feabe6c54c0adfbca))
* **chrome:** update the pinned browser version to 132.0.6779.0 ([#2680](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2680)) ([c80f6b5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c80f6b5fc6c6231af7611e635c84b2894d8c07ab))
* implement browsingContext.historyUpdated ([#2656](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2656)) ([48d496a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/48d496aee4215fd79e6e2a6eb80b6f8d2d6f92b4))


### Bug Fixes

* don't unblock on interception removed ([#2135](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2135)) ([b6cc9a1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b6cc9a1479e78954fdc93d1d37a6bfab70fff85d))

## [0.8.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.8.0...chromium-bidi-v0.8.1) (2024-10-11)


### Features

* implement Bluetooth Emulation BiDi mapping ([#2624](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2624)) ([48a233f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/48a233fa2504b891a6555894382d9091c1bed4cc))
* support MPArch sessions ([#2662](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2662)) ([662485f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/662485f5101c2e8937979f938cb97f9c58f6bfa0))

## [0.8.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.7.1...chromium-bidi-v0.8.0) (2024-09-30)


### ⚠ BREAKING CHANGES

* **chrome:** update the pinned browser version to 131.0.6724.0 ([#2622](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2622))

### Features

* `browsingContext.traverseHistory` only for top-level navigables ([#2627](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2627)) ([dd0dec5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/dd0dec59f7bedfbca59c61155eb0c8bb216c7bee))
* **chrome:** update the pinned browser version to 131.0.6724.0 ([#2622](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2622)) ([ff9658a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ff9658a3f80b8c612e41fcdcd5d5a054436ac333))


### Bug Fixes

* show correct log method ([#2644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2644)) ([4c66419](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4c66419b2ee6a34be894b92ed91bbbc75bea5a7b))

## [0.7.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.7.0...chromium-bidi-v0.7.1) (2024-09-16)


### Features

* initial implementation for the Web Bluetooth spec ([#2060](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2060)) ([ecb18d3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ecb18d3e53582424b579dae8dd3367e937c66d66))
* support ESM module ([#2451](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2451)) ([662a857](https://github.com/GoogleChromeLabs/chromium-bidi/commit/662a857fc45a07b9278b95399494920e533594bd))
* support network.setCacheBehavior ([#2593](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2593)) ([75ba46c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/75ba46c2d18d4f9542962388bde722c51beaf0bb))
* support only statusCode in continueResponse ([#2598](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2598)) ([1eeff5b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1eeff5bd9f84beadd1cf0305d9d288fb974fe2a2))


### Bug Fixes

* add bluetooth command parser and fix tests ([#2589](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2589)) ([98ad2d9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/98ad2d904c3be330c4fdf52032111156a1b88ab6))
* clear the buffered logs when browsing context is destroyed ([#2592](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2592)) ([36fb707](https://github.com/GoogleChromeLabs/chromium-bidi/commit/36fb7078ec0da6dc6b29befc1ce0e08a43bbf6d4)), closes [#475](https://github.com/GoogleChromeLabs/chromium-bidi/issues/475)
* provide invalid set cache props ([#2590](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2590)) ([0830f00](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0830f001231b795a56724407562d81b8e0a469ab))
* **spec:** update WebBluetooth implementation to match the latest spec ([#2588](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2588)) ([ec1ab96](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ec1ab9652196765293f1ae54ba73ac9d7a9e88d5))

## [0.7.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.5...chromium-bidi-v0.7.0) (2024-09-05)


### ⚠ BREAKING CHANGES

* emit `browsingContext.contextDestroyed` once ([#2563](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2563))

### Bug Fixes

* always provide url in `browsingContext.navigationStarted` ([#2483](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2483)) ([318d621](https://github.com/GoogleChromeLabs/chromium-bidi/commit/318d6212570ae8abe2c22e06c5bb618cdb34286c))
* emit `browsingContext.contextDestroyed` once ([#2563](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2563)) ([930d401](https://github.com/GoogleChromeLabs/chromium-bidi/commit/930d401862910bf78f9d5d66dd9b2218fd8f8aa3))
* fail previous navigation on the next one ([#2569](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2569)) ([0cfd51a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0cfd51ad7704f5d489b170946a073e17e23f0eeb))
* navigation with wait `None` ([#2557](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2557)) ([bf89379](https://github.com/GoogleChromeLabs/chromium-bidi/commit/bf8937958df8066995230f63435b7ea41adc1dfb))

## [0.6.5](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.4...chromium-bidi-v0.6.5) (2024-08-29)


### Bug Fixes

* allow pen to hover over ([#2524](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2524)) ([79feac8](https://github.com/GoogleChromeLabs/chromium-bidi/commit/79feac847d2c5763f2b21d04d7f31e251a805ee9))
* css and xPath selector start node is document ([#2543](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2543)) ([b6637c1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b6637c16be66622a32067e5c64130ae44d7e1244))
* do not emit pen events if it is not detected by digitizer. ([#2530](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2530)) ([3e581cd](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3e581cd2d46d814f61d30f26a8f138fc651ab4e9))
* emit realm events when subscribing ([#2486](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2486)) ([5980d74](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5980d74c23a6650375f3f9db27a733c5e578649c))

## [0.6.4](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.3...chromium-bidi-v0.6.4) (2024-08-05)


### Features

* implement capabilities handling in session.new ([#2448](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2448)) ([9c74a9c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9c74a9cc31df5cc4dd45fba716e361adc7d0b995))


### Bug Fixes

* do not log env ([#2471](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2471)) ([048fed1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/048fed10fe7d942d4992a201dad713f2f6b40adf))
* improve capabilities matching ([#2464](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2464)) ([8181e44](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8181e4481e0a78b1263e4b7b57afc892a9ff3b9a))

## [0.6.3](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.2...chromium-bidi-v0.6.3) (2024-07-26)


### Features

* implement part of request timings ([#2435](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2435)) ([370b649](https://github.com/GoogleChromeLabs/chromium-bidi/commit/370b6496b639be3b2f7c806a6ed0616c7089bcf4))

## [0.6.2](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.1...chromium-bidi-v0.6.2) (2024-07-22)


### Bug Fixes

* only auto-attach to a target once ([#2421](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2421)) ([7118b96](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7118b96f587ce7bb0fd69e72af33f37afff86fc2))
* support `default` for BeforeUnload prompt ([#2412](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2412)) ([f24ad85](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f24ad85e6232f5f39232db5ca6c9530ae303e1d5))
* support for cookies in ContinueRequest ([#2370](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2370)) ([0cd7e12](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0cd7e126e8182292701db330f0522b1da50497e1))

## [0.6.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.6.0...chromium-bidi-v0.6.1) (2024-07-12)


### Features

* include vendor-prefixed additional request data ([#2406](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2406)) ([76bce85](https://github.com/GoogleChromeLabs/chromium-bidi/commit/76bce8552ddf45633181ce2af1050fe46de55e65))
* include vendor-prefixed security details ([#2405](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2405)) ([414fa88](https://github.com/GoogleChromeLabs/chromium-bidi/commit/414fa88f2c50ea53f4d714e9fc8d2951f858f5cf))


### Bug Fixes

* default `beforeUnload` behavior is `accept` ([#2397](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2397)) ([0d79f4b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0d79f4b606c305d9ae2602204cb34952976ff86c))
* restore OOPiF state ([#2381](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2381)) ([3e9855c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3e9855c5a57b3c72745407906d081c0e57f042e9))
* the capability to ignore cert errors should be browser-wide  ([#2369](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2369)) ([6db665b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6db665b2a989f93b5fa3d2b3881ba16fd5248442))

## [0.6.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.24...chromium-bidi-v0.6.0) (2024-06-28)


### ⚠ BREAKING CHANGES

* default behavior changed from `ignore` to `dismiss`.

### Features

* body size for non-intercepted requests ([#2348](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2348)) ([ec07c7d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ec07c7df73ca75b30b54f6be9c4d7ae7f8c652a7))
* respect `unhandledPromptBehavior` capability ([#2351](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2351)) ([08672b9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/08672b97b6ac397818bffa898610e82170dcbdd6))


### Bug Fixes

* openerFrameId whenever possible ([#2329](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2329)) ([b3ab7ef](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b3ab7ef444cddf0913ae026cf41fc69509e05f82))
* provide `navigation` and `url` in events ([#2264](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2264)) ([9f058da](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9f058dabe91e7b7baa397ec393e67fddb4971b0d))
* provide type in `browsingContext.userPromptClosed` event ([#2349](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2349)) ([1326c16](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1326c162726695f09b477839dca9da802d1719bb))
* request body size ([#2339](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2339)) ([28dc58b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/28dc58ba043efb465636dad54ccf94677c8e2d35))

## [0.5.24](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.23...chromium-bidi-v0.5.24) (2024-06-17)


### Features

* add support for `originalOpener` in BrowsingContext.Info ([#2318](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2318)) ([a132812](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a132812466195843558d1a98f61805ef1f8b83af))


### Bug Fixes

* catch error when logging args ([#2319](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2319)) ([90b9708](https://github.com/GoogleChromeLabs/chromium-bidi/commit/90b970879f9285e1a943bb13a9c59af5815644aa))
* handle `registerPromiseEvent` with errors ([#2323](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2323)) ([d38472f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d38472f65319fe63607681bf1eb1aa4ee2f7df83))

## [0.5.23](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.22...chromium-bidi-v0.5.23) (2024-06-11)


### Bug Fixes

* remove requests after they have been completed ([#2302](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2302)) ([e4b4139](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e4b4139e534b64c56824208e8773b0d12b50acb9)), closes [#2301](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2301)

## [0.5.22](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.21...chromium-bidi-v0.5.22) (2024-06-10)


### Bug Fixes

* keep radius and force of input ([#2299](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2299)) ([3b473c3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3b473c3222d28ead9af4d39a286e0eaeea7b09a9))

## [0.5.21](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.20...chromium-bidi-v0.5.21) (2024-06-07)


### Features

* restore frame tree when reconnecting to browser ([#2289](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2289)) ([f7a0c75](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f7a0c754b132d1b48bbdaa6f36a41345c3b64598))
* support creating tab in background ([#2262](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2262)) ([e7c4b42](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e7c4b4224561c3fed6c389ac96df2972aaa10d13))


### Bug Fixes

* validate HTTP method ([#2284](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2284)) ([a22694a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a22694aa85de8e24fdae3da64879dad1ca4e48c4))
* wait for default realm to be created before evaluating script ([#2294](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2294)) ([444e728](https://github.com/GoogleChromeLabs/chromium-bidi/commit/444e728fab1efd684a7dc624f1abbc9444f76905))
* when reconnecting, save context's url ([#2276](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2276)) ([9585138](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9585138fa75a508bca42b8cc7df42ed6a46aff9f))

## [0.5.20](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.19...chromium-bidi-v0.5.20) (2024-05-31)


### Features

* send `browsingContext.contextCreated` event while subscribing ([#2255](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2255)) ([592c839](https://github.com/GoogleChromeLabs/chromium-bidi/commit/592c839b4cc9b1661f0b00c4a1a467d519255e7c))
* support document as `startNodes` in `browsingContext.locateNodes` ([#2218](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2218)) ([ad7318f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ad7318f29ff91831583ee67173ea3322672f1640))
* support for graphemes in key input ([#2207](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2207)) ([8e3a6c0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8e3a6c03ddfdc58c73d542d0bf960b890a1ba46b))


### Bug Fixes

* added missing input transformations ([#2186](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2186)) ([ea48dc2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ea48dc2d238d884d5d25a4fa623fed1ad591343b))
* css locator should allow nodes to be start nodes ([#2195](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2195)) ([4a361a5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4a361a541dc134656cb0b2207d20c4f47ba3c8ce))
* errors for `input.setFile` ([#2232](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2232)) ([49e3712](https://github.com/GoogleChromeLabs/chromium-bidi/commit/49e3712930770917ecc40e4e57a37bbc6c5f89df))
* expose the override data to the request events ([#2241](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2241)) ([947bb8e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/947bb8e1265e4bc86cd17592e59b48dcd35d9950))
* report correct value for DefaultValue in UserPrompt ([#2228](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2228)) ([df5ebf6](https://github.com/GoogleChromeLabs/chromium-bidi/commit/df5ebf68189cebe22fb559e141c0bc08b379932e))
* screenshot taken in scrolled viewport origin ([#2161](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2161)) ([b3c57c8](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b3c57c83fdef42dc96d017d7fba02faae36b0595))
* throw invalid argument for header ([#2246](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2246)) ([664d043](https://github.com/GoogleChromeLabs/chromium-bidi/commit/664d04302ec76bccb1a9d8838c9b34de20eb8cd2))

## [0.5.19](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.18...chromium-bidi-v0.5.19) (2024-04-24)


### Bug Fixes

* turn on a11y caches ([#2153](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2153)) ([49f6914](https://github.com/GoogleChromeLabs/chromium-bidi/commit/49f6914e9ab414ef0fb448b21d6c158db4f7a9cb))

## [0.5.18](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.17...chromium-bidi-v0.5.18) (2024-04-23)


### Features

* implement accessibility locator ([#2148](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2148)) ([e2a6303](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e2a630362e9eaae294dae54eb2e16a3e0082f1a6))


### Bug Fixes

* apply existing context check to default user context too ([#2121](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2121)) ([0b1bbe5](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0b1bbe5e2bb713365275c5deb7658e58e185a13f))
* get the correct status from last response extra info ([#2128](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2128)) ([440e9ab](https://github.com/GoogleChromeLabs/chromium-bidi/commit/440e9ab6f0092254997edd8450d1609db2f9addd))

## [0.5.17](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.16...chromium-bidi-v0.5.17) (2024-04-10)


### Features

* **network:** support more props for `initiator` ([#2115](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2115)) ([80dd9e6](https://github.com/GoogleChromeLabs/chromium-bidi/commit/80dd9e6352cf7fac054697401bc63f92c78b3ea5))


### Bug Fixes

* **browsingContext:** emit `navigationFailed` for `navigate` command failure ([#2118](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2118)) ([382a762](https://github.com/GoogleChromeLabs/chromium-bidi/commit/382a762b159f550aaa0968eb8b20358401ed2510))
* don't expect interception for cached events ([#2087](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2087)) ([063c1d1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/063c1d1d6cfee35a6a5df33612137b95bd7075f3))
* emit `network.responseCompleted` for redirects ([#2098](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2098)) ([219cfc9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/219cfc9201bcbfd01b00a1b9b5d4e7d995a62b6a))
* **network:** support Interception for OOPIF ([#2110](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2110)) ([5d0845c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5d0845c898a4008f2dccff5da220037a043f2895))
* **script:** support PreloadScript in OOPIF ([#2109](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2109)) ([baa263e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/baa263e5f4c67df0289d4b65d1aa8e527404d9b6))
* sending undefined viewport should keep previously set viewport ([#2119](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2119)) ([823e52d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/823e52d33648e696540ad66f9c1168ac93803c1d))

## [0.5.16](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.15...chromium-bidi-v0.5.16) (2024-03-27)


### Features

* support body in `network.continueRequest` ([#2075](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2075)) ([d0c4955](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d0c4955f2d5564c5f8da8db0bdf2fff5b515906d))


### Bug Fixes

* add fragment to url ([#2079](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2079)) ([d416b6c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d416b6c86011147121f5adea7e447bcd14d8b662))
* do not expect init or commit to arrive ([#2080](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2080)) ([d37d406](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d37d4065b8f7b6778a6a1ab40f82f0a3cb0c5907))
* don't block data url events when interception is enabled ([#2081](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2081)) ([1350b3b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1350b3b316b519836efe727ce20ee0e57422b1bd))
* emit `network.beforeRequestSent` event for data urls ([#2073](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2073)) ([5162b0a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5162b0ab1e214e8e53763765cd1c43673cdb7997))

## [0.5.15](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.14...chromium-bidi-v0.5.15) (2024-03-25)


### Features

* better support for `network.provideResponse` ([#2065](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2065)) ([99f81fe](https://github.com/GoogleChromeLabs/chromium-bidi/commit/99f81fe686663ac1cb5a084e72e6434dd97fd826))
* implement readonly capabilities ([#2070](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2070)) ([a93aa60](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a93aa60e49af1ef169c3814c9f89078ce0cd9de5))


### Bug Fixes

* continue blocking if CDP command fails ([#2068](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2068)) ([43e7f83](https://github.com/GoogleChromeLabs/chromium-bidi/commit/43e7f83f6430d8387b9685a15a21055b91983333))
* don't encode body 2 times ([#2069](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2069)) ([aa20457](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aa204578392152d271b20f70637e9978bcf24bed))
* throw NoSuchAlertException for prompts ([#2055](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2055)) ([f67f79b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f67f79b9c0a7ba11a7c2d2e05e764ca0e9d35319))

## [0.5.14](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.13...chromium-bidi-v0.5.14) (2024-03-21)


### Features

* support `innerText` locators ([#1988](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1988)) ([8c41582](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8c415827ce62705184e920899cf4846a9fee71e0))
* support `maxDepth` and `serializationOptions` in `browsingContext.locateNodes` ([#2048](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2048)) ([eca1e06](https://github.com/GoogleChromeLabs/chromium-bidi/commit/eca1e061f16abbaeafdcc57455e200dc68a0e224))
* support `maxNodeCount` in locators ([#2040](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2040)) ([ba68a85](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ba68a852c9540f73605b28f7a1d9f2c3f63bca92))
* support `startNodes` in locators ([#2042](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2042)) ([62d58a9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/62d58a9cf5dc59e768380c667f7dda7840b7175d))
* support userContext is setPermission ([#2033](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2033)) ([3186576](https://github.com/GoogleChromeLabs/chromium-bidi/commit/31865769195174da1fbcc15cc9632c5fcf8c7f25))


### Bug Fixes

* CDP quirkiness with intercept ([#2021](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2021)) ([8890fb0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8890fb0fa51ca4cc3bb15afc8eef9024dc6e77e2))
* check bidiValue before using ([#2039](https://github.com/GoogleChromeLabs/chromium-bidi/issues/2039)) ([6154420](https://github.com/GoogleChromeLabs/chromium-bidi/commit/61544207d39a883c2558681b96df36637a32bed2))

## [0.5.13](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.12...chromium-bidi-v0.5.13) (2024-03-15)


### Features

* `invalid selector` error ([#1985](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1985)) ([cba1d35](https://github.com/GoogleChromeLabs/chromium-bidi/commit/cba1d3502edc8ff5d9f541e5dd7b850c79b3c813))
* add support for `contexts` in `addInterception` ([#1945](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1945)) ([fc76be7](https://github.com/GoogleChromeLabs/chromium-bidi/commit/fc76be7c4742f743e01d613168b2946651a77463))
* start implementing `browsingContext.locateNodes` ([#1970](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1970)) ([d61f154](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d61f154d6236b53cda2173fc689099c4d7c9f751))
* support `network.continueResponse` authorization ([#1961](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1961)) ([528ad63](https://github.com/GoogleChromeLabs/chromium-bidi/commit/528ad63c66b09d3636ea0085a3dae19a3f2761fc))
* support base64 cookie values ([#1933](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1933)) ([9d1b975](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9d1b975522ede4f275fe73b5cc8317c35d790d2d))
* support xpath locators ([#1986](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1986)) ([b49184f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b49184f55038ff547dc6f6fd6d56de7cf0b11df9))


### Bug Fixes

* add `authChallenges` to response ([#1919](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1919)) ([e4a519a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e4a519a1684ab75c95b4760a1e031337e483967e))
* always provide `userContext` in cookie's `partitionKey` ([#1938](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1938)) ([0adf6d1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0adf6d1e00e7a972923eeb951bb3af4cd583178e))
* correctly process `NoSuchUserContextException` in cookie operations ([#1940](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1940)) ([7407608](https://github.com/GoogleChromeLabs/chromium-bidi/commit/74076089eb1f4f99a8a813fa724d11007e7f2b97))
* don't block on unsubscribed events ([#1954](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1954)) ([0abbff8](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0abbff888987e81216ca88434958843619dbff1e))
* don't throw unhandled errors ([#1996](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1996)) ([ab1c6d2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ab1c6d2c45b51fc05d2f14d7d547fb25e357610e))
* emit for target id ([#1979](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1979)) ([d1091bd](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d1091bd673968ba37c1cc0f02f3c013f92cac62f))
* the pattern matching logic ([#1995](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1995)) ([66010d1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/66010d1e545949ecd41f0db0b88148acbbdd751d))
* workaround issue with Script.Target ([#1947](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1947)) ([3cc317b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3cc317be7c6f05b5850728dc71feeadd9451d105))

## [0.5.12](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.11...chromium-bidi-v0.5.12) (2024-02-29)


### Features

* implement `storage.deleteCookies` ([#1915](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1915)) ([18d3d4f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/18d3d4f197320dcb152d9d0b729bcf2a77720cec))


### Bug Fixes

* implement less flaky network module ([#1871](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1871)) ([4ec8bad](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4ec8bad7d1f42b916bbab207ab68371e79063ff1))
* parse `browser.RemoveUserContextParameters` ([#1905](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1905)) ([a50821b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a50821b2661777be4509bf9bc9c6a0890e5e5a7a))

## [0.5.11](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.10...chromium-bidi-v0.5.11) (2024-02-23)


### Bug Fixes

* add `cdp.resolveRealm` command ([#1882](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1882)) ([08d3e45](https://github.com/GoogleChromeLabs/chromium-bidi/commit/08d3e45dc4ba5317b53b3aae994fc1fc5551e1bd))
* use better error handling ([#1877](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1877)) ([33536e0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/33536e0be142de0659241c4762b2040c52c3e980))

## [0.5.10](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.9...chromium-bidi-v0.5.10) (2024-02-22)


### Features

* support user context in permissions ([#1805](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1805)) ([a623dc7](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a623dc751952817f4676e9f6836792a1e11de234))


### Bug Fixes

* limit the mapper tab to 200 lines of logs ([#1846](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1846)) ([9c60d0a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9c60d0a553ce75f9412843de7777baf8f3db1300)), closes [#1839](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1839)

## [0.5.9](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.8...chromium-bidi-v0.5.9) (2024-02-07)


### Features

* allow Chrome-specific params in user contexts ([#1814](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1814)) ([4b9d742](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4b9d74262efe679bf1e13fb9045b6c1e3f33c4ed))
* generalize worker realm ([#1779](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1779)) ([a79e1a2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a79e1a2620f65ef7821d8dcf6322dcc3d3dbe5cb))


### Bug Fixes

* enable cdp Network domain when needed ([#1785](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1785)) ([7be907d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7be907d251a6faad0da3cf0b7f5b46505f341583))

## [0.5.8](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.7...chromium-bidi-v0.5.8) (2024-01-31)


### Features

* add temp `context` into worker realm for Puppeteer ([#1801](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1801)) ([3703549](https://github.com/GoogleChromeLabs/chromium-bidi/commit/37035497367dd151f5d88e9b97b30d7065350c0f))

## [0.5.7](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.6...chromium-bidi-v0.5.7) (2024-01-30)


### Features

* separate realm types ([#1709](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1709)) ([9307f05](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9307f0515d28903e147ca07639a449e9ee18aa24))
* support user context in cookies API ([#1781](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1781)) ([d838757](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d838757a16536dfe68c5f141ba0090cbfc363385))

## [0.5.6](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.5...chromium-bidi-v0.5.6) (2024-01-29)


### Features

* add CPD specific field in cookies ([#1759](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1759)) ([d24584a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d24584ac0d30f58306700d29d22db284a4f47f7a))


### Bug Fixes

* handle headless errors when creating a target ([#1757](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1757)) ([cd7e772](https://github.com/GoogleChromeLabs/chromium-bidi/commit/cd7e772b39eb774c8f65e59b7047eafb419bea09))

## [0.5.5](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.4...chromium-bidi-v0.5.5) (2024-01-25)


### Features

* allow not partitioned cookies ([#1718](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1718)) ([d54a4f1](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d54a4f186005fa0e3ea8e71a5929ca307c852f1d))
* implement user contexts ([#1715](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1715)) ([b75def3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b75def3778dc774d1bcee30f7029c6387568e960))
* provide logs before Mapper is launched via NodeJS runner ([#1737](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1737)) ([0b278f3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0b278f30f9e46037bd9ec3d8d250457405ee7125))
* return all cookies for a given browsing context ([#1746](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1746)) ([456d947](https://github.com/GoogleChromeLabs/chromium-bidi/commit/456d9473a7cb3b58127b92c45a8cffcb992969ef))

## [0.5.4](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.3...chromium-bidi-v0.5.4) (2024-01-17)


### Features

* implement `Input.setFiles` ([#1705](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1705)) ([50d1921](https://github.com/GoogleChromeLabs/chromium-bidi/commit/50d1921180af38aa06b45968a57919a0233f6865))
* implement `storage.getCookies` and `storage.setCookie` ([#1593](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1593)) ([2b08660](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2b08660200dfa08362660756356399734ecd3015))
* implement permissions ([#1645](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1645)) ([29c7b0b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/29c7b0b6ce6fb1bcfc3053c92bd24c7c7d580fa4))

## [0.5.3](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.2...chromium-bidi-v0.5.3) (2024-01-10)


### Features

* in `sharedId` use `loaderId` from deep serialized value ([#1631](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1631)) ([6819143](https://github.com/GoogleChromeLabs/chromium-bidi/commit/68191438866b90373faf838e86684264fdbf473e))

## [0.5.2](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.1...chromium-bidi-v0.5.2) (2023-12-15)


### Features

* implement dedicated workers ([#1619](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1619)) ([552cece](https://github.com/GoogleChromeLabs/chromium-bidi/commit/552cecedd736db19a477572793258e1994b11df9))
* implement evaluate for dedicated workers ([#1625](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1625)) ([ca083df](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ca083dfba38a9e5e3bce6d501ecea64fbe0713bc))

## [0.5.1](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.5.0...chromium-bidi-v0.5.1) (2023-11-20)


### Features

* export `MapperOptions` TS type ([#1575](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1575)) ([57c009b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/57c009b9a082dbda33464a6c888b74cc785f1b18))

## [0.5.0](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.34...chromium-bidi-v0.5.0) (2023-11-17)


### ⚠ BREAKING CHANGES

* `BidiServer.createAndStart` signature changed. New optional parameter `options` is added. Breaking change for Puppeteer, while ChromeDriver is not affected, as it uses the Mapper Tab.

### Features

* `acceptInsecureCerts` ([#1553](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1553)) ([8a13940](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8a139400403f9970d0ad35ee4bfac8c9cfce48b5))
* implement dedicated workers ([#1565](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1565)) ([8312aff](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8312aff6d66354ea6d5a5671dc8ab56d19aadf3a))


### Bug Fixes

* do not wait during browsingContext.traverseHistory ([#1557](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1557)) ([aaf45a2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aaf45a20ee0cefb3cd11d2a2190b1361d47d5bba))

## [0.4.34](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.33...chromium-bidi-v0.4.34) (2023-11-15)


### Features

* abort navigation if network request failed ([#1542](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1542)) ([2f86ba0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2f86ba0fccf840ea808703fef9055ee50d255a74))
* context close to support promptUnload ([#1508](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1508)) ([45a2100](https://github.com/GoogleChromeLabs/chromium-bidi/commit/45a2100123cc89ca23e7e841e9dc630201488c04))
* implement browsingContext.traverseHistory ([#1537](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1537)) ([76ca291](https://github.com/GoogleChromeLabs/chromium-bidi/commit/76ca291c12a3ccf1314b92cf1c31c665edc2b57b))
* make `internalId` `UUID` ([#1525](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1525)) ([8b108ce](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8b108cefee06ceb704db41ea95bbd4dc359aeffb))
* **network intercept:** populate "intercepts" in base event params ([#1500](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1500)) ([55d1622](https://github.com/GoogleChromeLabs/chromium-bidi/commit/55d1622f065dad36248977f1fed5ac143f526497))


### Bug Fixes

* allow interception for all requests ([#1530](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1530)) ([ec3fce9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ec3fce915e6016446de601fde6fa707b1c6f1eba))
* automatically continue ignored events ([#1528](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1528)) ([8d92718](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8d927181d8705210bb31af8dab2fb01abb358a34))

## [0.4.33](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.32...chromium-bidi-v0.4.33) (2023-10-30)


### Features

* add network intercept continue with auth ([#1470](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1470)) ([ad3a95e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ad3a95edddeeb451a71cf4a1b545800b255f672a)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* addPreloadScript respects new contexts ([#1478](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1478)) ([b0e55fa](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b0e55fa613dd4b6f9e3670bcff6da82ce44f4622))
* addPreloadScripts respects contexts param for old contexts ([#1475](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1475)) ([0cdde07](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0cdde074938f6cbdca8bb808891e4528a3714998))
* implement headersSize for network requests ([#1498](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1498)) ([e904ee0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e904ee0c446ca39491b29a60b9fa428541149310)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* implement network interception continue response ([#1443](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1443)) ([4515d1d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4515d1d5c96c2b310928c28ca31260c8cd5433d7)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* implement network interception provide response ([#1457](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1457)) ([1eca26e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1eca26e0c6d48e266384cdfe1f3bc7c93ebf2710)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* **logging:** pretty print received and sent bidi server messages ([#1490](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1490)) ([45fd24e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/45fd24e483deaa2e85819e2bf824127010c9b421))
* **network intercept:** implement continue with auth (cont.) ([#1484](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1484)) ([7cc9358](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7cc935885d1b44cbf34d52f5d86b459d11befcd8)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* **network intercept:** specify BeforeRequestSent whenever AuthRequi… ([#1494](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1494)) ([22eafee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/22eafee8d0f4e55d74a451732b5d217c187db438)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* network request: prioritize response status code over extraInfo ([#1466](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1466)) ([d1f3302](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d1f33024cda1bceecf307c2a19edcddcd5f7bec9)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* **network:** emit `responseStarted` event ("AND") ([#1497](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1497)) ([46220b7](https://github.com/GoogleChromeLabs/chromium-bidi/commit/46220b7596803ea5a5c1925f343b072a0e9f0e4d)), closes [#765](https://github.com/GoogleChromeLabs/chromium-bidi/issues/765)


### Bug Fixes

* Add `window.setSelfTargetId` for backward compatibility with chrome driver ([#1461](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1461)) ([fe98f94](https://github.com/GoogleChromeLabs/chromium-bidi/commit/fe98f94a8e9bed5253fb69485d62a47842bf5992))

## [0.4.32](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.31...chromium-bidi-v0.4.32) (2023-10-16)


### Features

* add quality for `webp` ([#1426](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1426)) ([d514bf9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d514bf96bedd1b53473abb212638110066831efb))
* implement device pixel ratio changes ([#1422](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1422)) ([49f6dee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/49f6dee572f67992d87ca4ec60e49350a8467ff2))
* implement document origin screenshots ([#1427](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1427)) ([b952297](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b95229776ff40f5ee7087ad4b93ff559098b8899))
* **network interception:** implement continue request ([#1331](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1331)) ([8a935b9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8a935b9c9295ceea567cabd2b696eb8fbcc53369)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* session handling refactoring. Step 1 ([#1385](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1385)) ([8fe37b9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8fe37b9f402bb2b52c585b6925dba4b15b04473a))
* unblock event queue when network events are blocked ([#1409](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1409)) ([e94f79d](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e94f79d88cba8b73f2a895a31fffd87b9f03f48a))

## [0.4.31](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.30...chromium-bidi-v0.4.31) (2023-10-06)


### Bug Fixes

* part 2 of [#1391](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1391) ([#1393](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1393)) ([a875831](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a8758313bf3a0f704973c749da86f01da7df4b83))

## [0.4.30](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.29...chromium-bidi-v0.4.30) (2023-10-06)


### Bug Fixes

* use 0.5 for default radiusX and radiusY ([#1391](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1391)) ([8a423d3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8a423d301981e18bcd517cc600c0b7456e029fbe))

## [0.4.29](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.28...chromium-bidi-v0.4.29) (2023-10-06)


### Features

* implement angle inputs ([#1342](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1342)) ([90933ee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/90933eee8c847c15135eeb8fe8d8b01c72927e0d))
* implement network interception fail request ([#1318](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1318)) ([c5f6581](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c5f658168d8eac5fc2dc851991ee825e6d5e1fbf)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)


### Bug Fixes

* mapper tab debugging logs ([#1336](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1336)) ([54ea831](https://github.com/GoogleChromeLabs/chromium-bidi/commit/54ea831451ba79c0111f1edc4f93fe1ddce0ef5d))
* round tilt values ([#1387](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1387)) ([2d4707f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2d4707f80c3766e0d994c659510ce5402b59f6b1))
* use half the width/height for touch event radii ([#1341](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1341)) ([aa84a40](https://github.com/GoogleChromeLabs/chromium-bidi/commit/aa84a4061449a22e490791c3abad3ce1374377b4))

## [0.4.28](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.27...chromium-bidi-v0.4.28) (2023-09-20)


### Features

* handle `Fetch.requestPaused` event ([#1304](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1304)) ([5b6a579](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5b6a579ed4ccab359935a6d531c623106afd3bbd)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* support redirect response ([#1313](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1313)) ([7c17942](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7c179422ea2f37c9857ca93bc0879a7a6354041a))


### Bug Fixes

* restore functionality to subscribe to all CDP events ([#1301](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1301)) ([171518f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/171518f1a0d3ebc24301075183725f30570e54a8))
* separate click count by button ([#1321](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1321)) ([9ebf2ed](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9ebf2ed14903ff0fb88613bdaf636c7a81526a7a))

## [0.4.27](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.26...chromium-bidi-v0.4.27) (2023-09-12)


### Features

* add get network intercepts method ([#1250](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1250)) ([57cc9e9](https://github.com/GoogleChromeLabs/chromium-bidi/commit/57cc9e992e2728257995bab4ce70ecd907208030)), closes [#1183](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1183)
* browsingContext.reload: return the navigation instead of empty ([#1255](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1255)) ([c534e0e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c534e0ed2bcf2c3f7f3213e75412bf83423b9d5b)), closes [#650](https://github.com/GoogleChromeLabs/chromium-bidi/issues/650)
* restore network redirects ([#1249](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1249)) ([5bbe93f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5bbe93f65752f425438c5f0e67d239a62d09e890))
* throw InvalidArgument instead of UnsupportedOperation for print… ([#1280](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1280)) ([b32ea31](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b32ea31ca346caacf4af673c857ab97cb1e9ba45))


### Bug Fixes

* flatten event type ([#1292](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1292)) ([8f4cd2b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8f4cd2bce395089e11a79094e9f438724fb16d71))
* replace empty string `namespaceURI` with null ([#1285](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1285)) ([93fdf47](https://github.com/GoogleChromeLabs/chromium-bidi/commit/93fdf4735d0497ee4ef92c8360a36e45ccfe93c1))

## [0.4.26](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.25...chromium-bidi-v0.4.26) (2023-09-08)


### Bug Fixes

* catch uncaught throws from #getHandleFromWindow ([#1273](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1273)) ([f41d5cf](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f41d5cf1decac1e1edf811fe3f482beff67dacba))

## [0.4.25](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.24...chromium-bidi-v0.4.25) (2023-09-08)


### Bug Fixes

* unhandled promises in CDP targets ([#1270](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1270)) ([3ef5922](https://github.com/GoogleChromeLabs/chromium-bidi/commit/3ef59223aa165f0c1106fbbcfdfb8e6d3702c2ab))

## [0.4.24](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.23...chromium-bidi-v0.4.24) (2023-09-07)


### Features

* add url getter to NetworkRequest ([#1251](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1251)) ([1d12f04](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1d12f04248aa27227447876fdd35cd460d915de6)), closes [#1183](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1183)
* default value for `userPromptOpened` ([#1260](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1260)) ([94b0718](https://github.com/GoogleChromeLabs/chromium-bidi/commit/94b0718f5d47391ac536aadaaf230463d62d3904))
* implement `UnableToCaptureScreenException` in browsingContext.captureScreenshot ([#1236](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1236)) ([8110918](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8110918d29148797f68959ba57ef6169f09b804b))
* Network Intercept: handle special schemes ([#1224](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1224)) ([27c6ccb](https://github.com/GoogleChromeLabs/chromium-bidi/commit/27c6ccbbff2eabcf77544dd4796c37a6abca49a5)), closes [#1183](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1183)


### Bug Fixes

* reject errors instead of throwing in CDP target init ([#1267](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1267)) ([694cc8a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/694cc8a56e59fc58cdca27232df90900458695bd))

## [0.4.23](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.22...chromium-bidi-v0.4.23) (2023-08-29)


### Features

* add network redirects ([#1215](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1215)) ([5de26ff](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5de26ff63e1d84d0dd1e60ff79ac775874629eed))
* implement clip for `BrowsingContext.captureScreenshot` ([#1212](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1212)) ([b17379f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/b17379f58de050a898874b7053f750eb3772845a))

## [0.4.22](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.21...chromium-bidi-v0.4.22) (2023-08-24)


### Features

* **add network intercept:** parse URL patterns ([#1186](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1186)) ([977fff2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/977fff21245e02313d5c1dc115c0ece24164bc47))


### Bug Fixes

* no-op for trivial pen and touch movements ([#1205](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1205)) ([005526b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/005526bbc1053dbde2861639514869ff8cf17a32))
* remove deep-serialization checks ([#1190](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1190)) ([df45817](https://github.com/GoogleChromeLabs/chromium-bidi/commit/df4581786200216655fae00e45f926fbf41f90c8))
* remove sandbox check ([#1202](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1202)) ([8c97280](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8c9728013ff55b637c9921a678e172e45b8894e3))

## [0.4.21](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.20...chromium-bidi-v0.4.21) (2023-08-21)


### Features

* scaffold network request interception ([#1050](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1050)) ([667186a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/667186af58104e502db41703c8d26bb17dcfabc5)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)


### Bug Fixes

* filter only sent cookies ([#1184](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1184)) ([22c043b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/22c043b3e09082bc42fc0dc562c0c52c8bb826a8)), closes [#1011](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1011)
* ignore cert errors in the test driver ([#1161](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1161)) ([d0de039](https://github.com/GoogleChromeLabs/chromium-bidi/commit/d0de03950ba4aa20d7273cea9f13065be9fbf2cd)), closes [#1162](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1162)

## [0.4.20](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.19...chromium-bidi-v0.4.20) (2023-08-01)


### Features

* implement `browser.close` ([#1116](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1116)) ([a715559](https://github.com/GoogleChromeLabs/chromium-bidi/commit/a71555990c3ccc505ff6fe9bcc3dab9276877244))
* **script:** implement user activation ([#1105](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1105)) ([2408d7f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/2408d7fdd37cb535b95227395a06222e8e718bd5))


### Bug Fixes

* don't publish `.tsbuildinfo` ([#1106](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1106)) ([4b1945f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4b1945f390c0b257925c248b044c9cec56d50942))
* use Result passing to prevent Unhandled promise rejections ([#1112](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1112)) ([e0dc19f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e0dc19f1dd22d7484387656089ce5819b096aa76))

## [0.4.19](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.18...chromium-bidi-v0.4.19) (2023-07-25)


### Bug Fixes

* Deferred no multiple resolved/rejects ([#1076](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1076)) ([5657baf](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5657baf4ff6eee308d926402faab20ba99b5ec90))

## [0.4.18](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.17...chromium-bidi-v0.4.18) (2023-07-21)


### Features

* implement browsingContext.activate ([#1002](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1002)) ([22e2417](https://github.com/GoogleChromeLabs/chromium-bidi/commit/22e24175df46163303d0b646ede81bb9ab034d8d))
* implement drag n' drop ([#1006](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1006)) ([6443045](https://github.com/GoogleChromeLabs/chromium-bidi/commit/6443045a41675a653a912a06442dfc1ed4e4be72))
* **print:** throw unsupported operation when content area is empty ([#992](https://github.com/GoogleChromeLabs/chromium-bidi/issues/992)) ([71a8b5c](https://github.com/GoogleChromeLabs/chromium-bidi/commit/71a8b5c74950db19ccc6e75000c843b5156ac49b)), closes [#518](https://github.com/GoogleChromeLabs/chromium-bidi/issues/518)
* refactor scripts and realms and fix generator serialization ([#1013](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1013)) ([73ea6f0](https://github.com/GoogleChromeLabs/chromium-bidi/commit/73ea6f08a17100a3259fcb7c9ccb686efca8c3e5)), closes [#562](https://github.com/GoogleChromeLabs/chromium-bidi/issues/562)
* support iterator serialization ([#1042](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1042)) ([9dff121](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9dff121dcb34156330151512d552c5c815a45449))


### Bug Fixes

* don't hold finished requests in memory ([#1058](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1058)) ([f15163a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/f15163ac730d8e4a6cee2410c7e0619f8bf2374f))
* NavigationStarted Event for sub-frames ([#1009](https://github.com/GoogleChromeLabs/chromium-bidi/issues/1009)) ([c4841f8](https://github.com/GoogleChromeLabs/chromium-bidi/commit/c4841f86aa3cbd3b9d9eca5ae05e1bde94ef434b))

## [0.4.17](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.16...chromium-bidi-v0.4.17) (2023-07-07)


### Features

* `addScriptToEvaluateOnNewDocument`: run immediately ([#919](https://github.com/GoogleChromeLabs/chromium-bidi/issues/919)) ([cfba71f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/cfba71f2285ecda7f5ee52a525c2f5fea1d35150))
* add Dialog (user prompt) handling ([#924](https://github.com/GoogleChromeLabs/chromium-bidi/issues/924)) ([474a3fa](https://github.com/GoogleChromeLabs/chromium-bidi/commit/474a3fa1b96482f7b38f3e72c071a3956165e888))
* preload scripts: support sandboxes ([#978](https://github.com/GoogleChromeLabs/chromium-bidi/issues/978)) ([ef65951](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ef65951a0ab29a7ff877dd680d2c53b9e7cd407e)), closes [#293](https://github.com/GoogleChromeLabs/chromium-bidi/issues/293)
* protocol: add WindowProxyProperties ([#952](https://github.com/GoogleChromeLabs/chromium-bidi/issues/952)) ([0deef4b](https://github.com/GoogleChromeLabs/chromium-bidi/commit/0deef4bf59d68e61b7e196e751f416dfb19763e5))
* prototype network request interception: scaffold protocol ([#845](https://github.com/GoogleChromeLabs/chromium-bidi/issues/845)) ([1b77f94](https://github.com/GoogleChromeLabs/chromium-bidi/commit/1b77f94eb3828d711f2e63041a9388756b658f83)), closes [#644](https://github.com/GoogleChromeLabs/chromium-bidi/issues/644)
* use `maxNodeDepth` + `includeShadowTree` for serialization ([#815](https://github.com/GoogleChromeLabs/chromium-bidi/issues/815)) ([09b4fc6](https://github.com/GoogleChromeLabs/chromium-bidi/commit/09b4fc62a6a9b351fd0c4a95fabee0e355886005))
* use generated types for WebDriverBidi ([#961](https://github.com/GoogleChromeLabs/chromium-bidi/issues/961)) ([4f70209](https://github.com/GoogleChromeLabs/chromium-bidi/commit/4f702096092037263ecb1b95434972978f5ba993))


### Bug Fixes

* add stack trace to Unknown errors ([#938](https://github.com/GoogleChromeLabs/chromium-bidi/issues/938)) ([9773a8a](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9773a8aedff750246950699278ec98fcad2956dd))
* Network Module clogging Processing Queue ([#964](https://github.com/GoogleChromeLabs/chromium-bidi/issues/964)) ([9366a5e](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9366a5e3ad20a117063b328acbde45005eee2b6a))
* preload scripts: fully remove optional context param ([#972](https://github.com/GoogleChromeLabs/chromium-bidi/issues/972)) ([e3e7d76](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e3e7d76c116350b30b3aebba54153519aeec33c0)), closes [#293](https://github.com/GoogleChromeLabs/chromium-bidi/issues/293) [#963](https://github.com/GoogleChromeLabs/chromium-bidi/issues/963)
* stop fragmentNavigated from emitting for normal navigation ([#960](https://github.com/GoogleChromeLabs/chromium-bidi/issues/960)) ([7f91b46](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7f91b465af0a51787b1aaaf579aa4a0ee0362932)), closes [#955](https://github.com/GoogleChromeLabs/chromium-bidi/issues/955)
* use non-force close for BrowsingContext.close ([#939](https://github.com/GoogleChromeLabs/chromium-bidi/issues/939)) ([055126f](https://github.com/GoogleChromeLabs/chromium-bidi/commit/055126f06da7fadafdbdb0f1f945a2b42e47579f))

## [0.4.16](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.15...chromium-bidi-v0.4.16) (2023-06-28)


### Bug Fixes

* set correct text for Enter key ([#909](https://github.com/GoogleChromeLabs/chromium-bidi/issues/909)) ([ed41381](https://github.com/GoogleChromeLabs/chromium-bidi/commit/ed413819200a9b43271a362647247f97f2d719b1))

## [0.4.15](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.14...chromium-bidi-v0.4.15) (2023-06-28)


### Features

* add `browsingContext.navigationStarted'` ([#881](https://github.com/GoogleChromeLabs/chromium-bidi/issues/881)) ([db5a1cc](https://github.com/GoogleChromeLabs/chromium-bidi/commit/db5a1cc74d91286f317c22b77209fb04daa69dde))


### Bug Fixes

* allow shift with printable keys ([#906](https://github.com/GoogleChromeLabs/chromium-bidi/issues/906)) ([5ec0ba2](https://github.com/GoogleChromeLabs/chromium-bidi/commit/5ec0ba2b891238c50d08ba5fd4cece844892f5b3))
* expand viewport validation tests ([#895](https://github.com/GoogleChromeLabs/chromium-bidi/issues/895)) ([7cc5aee](https://github.com/GoogleChromeLabs/chromium-bidi/commit/7cc5aee7a11b19cf6c9d8f724caf356cbb7f1f9b)), closes [#868](https://github.com/GoogleChromeLabs/chromium-bidi/issues/868)
* use correct location for key events ([#903](https://github.com/GoogleChromeLabs/chromium-bidi/issues/903)) ([88be8e3](https://github.com/GoogleChromeLabs/chromium-bidi/commit/88be8e31f6b47780def2771baa06f7836b3d69f9))
* use correct modifiers for mouse click ([#904](https://github.com/GoogleChromeLabs/chromium-bidi/issues/904)) ([9561fff](https://github.com/GoogleChromeLabs/chromium-bidi/commit/9561fff1e1e339af6cd268e877e2a8631f018652))

## [0.4.14](https://github.com/GoogleChromeLabs/chromium-bidi/compare/chromium-bidi-v0.4.13...chromium-bidi-v0.4.14) (2023-06-27)


### Features

* add realmDestroyed event ([#877](https://github.com/GoogleChromeLabs/chromium-bidi/issues/877)) ([e4c8d96](https://github.com/GoogleChromeLabs/chromium-bidi/commit/e4c8d965efe76cfd6bb809674a20587dde0b24d4))
* **browsingContext:** implement `setViewport` ([#817](https://github.com/GoogleChromeLabs/chromium-bidi/issues/817)) ([cfd6d55](https://github.com/GoogleChromeLabs/chromium-bidi/commit/cfd6d5559fbbd846256abe64dcf965dc341ed1b5))
* make the BiDi events less "chatty" ([#892](https://github.com/GoogleChromeLabs/chromium-bidi/issues/892)) ([8c1ad46](https://github.com/GoogleChromeLabs/chromium-bidi/commit/8c1ad461148f5820d62cf8961981e781e543e7d9))

## [0.4.13]() (2023-06-20)

### Features

- Add frameNavigated event (#865)

## [0.4.12]() (2023-06-15)

### Bug Fixes

- Recover `fromCache` change (#791)
- Network response fromCache (#831)
- Screenshots failing when setViewport is used (#851)

### Features

- Adds script.realmCreated (#850)

### Testing

- Add save_pdf method for debugging (#842)

## [0.4.11]() (2023-05-30)

### Bug Fixes

- Network Module stuck if ServedFromCache is send (#773)

### Miscellaneous Tasks

- Remove global crypto (#767)
- Small fixes for network module (#785)

## [0.4.10]() (2023-05-22)

### Bug Fixes

- Layering issue with Puppeteer (#728)

### Miscellaneous Tasks

- EventEmitter should return type this (#725)

## [0.4.9]() (2023-05-12)

### Bug Fixes

- Suppress error for releasing object (#701)

### Miscellaneous Tasks

- Pin Chrome (#703)
- Auto-update Chrome (#706)
- Update pinning + browsers version (#713)
- Configure the automatic browser roll PRs (#719)

## [0.4.8]() (2023-05-08)

### Bug Fixes

- Network request respects hasExtraInfo field (#645)
- Cdp session parameter name (#649)
- Fix all add preload script validation tests by adding channels and validating them  (#679)
- Don't throw error when encountering redirects (#690)

### Miscellaneous Tasks

- Insure TypeScript work with Puppeteer (#668)

## [0.4.6]() (2023-03-24)

### Miscellaneous Tasks

- Remove console.error statement, replace with logger (#517)

### Refactor

- Refactor script evaluator (#542)

## [0.4.5]() (2023-03-01)

### Miscellaneous Tasks

- Remove +Infinity from SpecialNumber (#473)

## [0.4.4]() (2023-02-17)

### Bug Fixes

- Fix a couple of pytest issues by introducing a pytest.ini file (#426)
- Fix filename typo: Outgoind -> Outgoing (#436)
- Fix WPT README badges and rename wpt-chromedriver consistently (#447)


## [0.4.3]() (2022-12-13)

### Miscellaneous Tasks

- Sort package.json scripts (#330)

## [0.4.2]() (2022-05-06)

### Bug Fixes

- Fix mac dependency
- Fix launch.json (#3)
- Fix example (#108)

### Refactor

- Refactoring

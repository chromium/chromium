# `miette` Release Changelog

<a name="5.4.1"></a>
## 5.4.1 (2022-10-28)

### Bug Fixes

* **graphical:** Fix panic with zero-width span at end of line (#204) ([b8810ee3](https://github.com/zkat/miette/commit/b8810ee3d8aee7d7723e081616dd4f2fe8748abe))

<a name="5.4.0"></a>
## 5.4.0 (2022-10-25)

### Features

* **version:** declare minimum supported rust version at 1.56.0 (#209) ([ac02a124](https://github.com/zkat/miette/commit/ac02a1242b1d6452a428846d2a84d2ac164fd914))
* **report:** `Report::new_boxed` ([0660d2f4](https://github.com/zkat/miette/commit/0660d2f43c0a793b1e289b26bcca73c8733bdcff))
* **error:** `impl AsRef<dyn StdError> for Report` ([1a27033d](https://github.com/zkat/miette/commit/1a27033d7afd0007907550b1fc9d589d6f658662))

### Bug Fixes

* **wrapper:** complete forwarding `Diagnostic` implementations ([3fc5c04c](https://github.com/zkat/miette/commit/3fc5c04cbbd4b92863290a488a23d5243c16fe60))

<a name="5.3.1"></a>
## 5.3.1 (2022-09-10)

### Bug Fixes

* **miri:** Resolve Miri's concerns around unsafe code (#197) ([5f3429b0](https://github.com/zkat/miette/commit/5f3429b0626034328a0c2f1317b8a0e712c63775))
* **graphical:** Align highlights correctly with wide unicode characters and tabs (#202) ([196c09ce](https://github.com/zkat/miette/commit/196c09ce7af9e54b63aaa5dae4cd199f2ecba3fa))

<a name="5.3.0"></a>
## 5.3.0 (2022-08-10)

### Bug Fixes

* **utils:** Fix off-by-one error in SourceOffset::from_location (#190) ([c3e6c983](https://github.com/zkat/miette/commit/c3e6c983363af7f7a88e52d50d57404defb1bf49))

### Features

* **graphical:** Allow miette users to opt-out of the rendering of the cause chain (#192) ([b9ea5871](https://github.com/zkat/miette/commit/b9ea587159464c0090d9510567e5ea93bb772b49))

<a name="5.2.0"></a>
## 5.2.0 (2022-07-31)

### Features

* **json:** `causes` support (#188) ([c95f58c8](https://github.com/zkat/miette/commit/c95f58c87a1335e956be23879754ac312a2b0853))

### Bug Fixes

* **docs:** readme was getting cut off during generation ([e286c705](https://github.com/zkat/miette/commit/e286c705fda28c02df67a584c0a013a1bbc38968))

<a name="5.1.1"></a>
## 5.1.1 (2022-07-09)

### Bug Fixes

* **deps:** bump minimum supports-color version (#182) ([ccf1b8ad](https://github.com/zkat/miette/commit/ccf1b8ade5b631e05fad79d1f9c5d268706d118e))
* **graphical:** handle an empty source (#183) ([12dc4007](https://github.com/zkat/miette/commit/12dc40070a99ac91b67e23f7c15ce8151965fc81))

<a name="5.1.0"></a>
## 5.1.0 (2022-06-25)

### Features

* **protocol:** Implement SourceCode for Arc<str> (and similar types) (#181) ([85da6a84](https://github.com/zkat/miette/commit/85da6a8407ef727b8f77184b8a61f5b9a7d3ccef))

<a name="5.0.0"></a>
## 5.0.0 (2022-06-24)

### Breaking Changes

* **theme:** restructure automatic color selection (#177) ([1816b06a](https://github.com/zkat/miette/commit/1816b06a2efcd5705dfe91147ab5651fe0b517d6))
    * The default theme now prefers ANSI colors, even if RGB is supported
    * `MietteHandlerOpts::ansi_colors` is removed
    * `MietteHandlerOpts::rgb_color` now takes an enum that controls the color
      format used when color support is enabled, and has no effect otherwise.

### Bug Fixes

* **json:** Don't escape single-quotes, that's not valid json (#180) ([b193d3c0](https://github.com/zkat/miette/commit/b193d3c002be8a42fd199911cef3465e2e0cb593))

<a name="4.7.1"></a>
## 4.7.1 (2022-05-13)

### Bug Fixes

* **tests:** add Display impl to diagnostic_source example ([0a4cf4ad](https://github.com/zkat/miette/commit/0a4cf4ad24eb668d6668400b9ab3e8c896b33e3a))

<a name="4.7.0"></a>
## 4.7.0 (2022-05-05)

### Features

* **diagnostic_source:** add protocol method for Diagnostic-aware source chaining (#165) ([bc449c84](https://github.com/zkat/miette/commit/bc449c842662909d93d3a6b7e117fdbde77544e7))

### Documentation

* **IntoDiagnostic:** Warn of potential data loss (#161) ([2451ad6a](https://github.com/zkat/miette/commit/2451ad6a963c222831977e89542a7349b66f11cf))

<a name="4.6.0"></a>
## 4.6.0 (2022-04-23)

### Features

* **spans:** add From shorthand for zero-length SourceSpans ([1e1d6152](https://github.com/zkat/miette/commit/1e1d61525381a6699deba103a3829874676eee9c))
* **related:** print related prefixes according to severity (#158) ([084ed138](https://github.com/zkat/miette/commit/084ed138b7598d549f38fe873a758d0ed03ef2b1))

### Bug Fixes

* **graphical:** fix issue with duplicate labels when span len is 0 (#159) ([1a36fa7e](https://github.com/zkat/miette/commit/1a36fa7ec80de77e910e04cdb902270970611b39))

<a name="v4.5.0"></a>
## 4.5.0 (2022-04-18)

### Features

* **spans:** make SourceSpan implement Copy (#151) ([5e54b29a](https://github.com/zkat/miette/commit/5e54b29acf87eacf0a0255a9d3db8966de697fcf))
* **help:** update macro to allow optional help text (#152) ([45093c2f](https://github.com/zkat/miette/commit/45093c2f587a281a37e80141d126d87944ca75b5))
* **labels:** allow optional labels in derive macro (#153) ([23ee3642](https://github.com/zkat/miette/commit/23ee3642d198ff4f78af9729d7a5223b0c676d1f))
* **help:** allow non-option values in #[help] fields ([ea55f458](https://github.com/zkat/miette/commit/ea55f458fa8acabc1c7e001c405f90025d6dbafc))
* **label:** use macro magic instead of optional flag for optional labels ([9da62cd0](https://github.com/zkat/miette/commit/9da62cd05d777f8bd962f1fe94a75c47b11ee07e))

### Bug Fixes

* **theme:** set correct field in MietteHandlerOpts::ansi_colors (#150) ([97197601](https://github.com/zkat/miette/commit/97197601ee8f36fedb559c9c8b2d73ce5b0ca0ee))

<a name="v4.4.0"></a>
## 4.4.0 (2022-04-04)

### Features

* **report:** Add conversion from Report to Box<dyn Error> (#149) ([b4a9d4cd](https://github.com/zkat/miette/commit/b4a9d4cd9bc43720613b7d2bb6b521d51922c6b8))

### Bug Fixes

* **docsrs:** use proper module names for docsrs URLs ([a0b972f8](https://github.com/zkat/miette/commit/a0b972f8765040fdbb08fdbe006ceb4dbc9c31f2))
* **clippy:** misc clippy fixes ([b98b0982](https://github.com/zkat/miette/commit/b98b09828215ffc623aa17aa0bc8a6f45173a3f0))
* **fmt:** cargo fmt ([37cda4a3](https://github.com/zkat/miette/commit/37cda4a3a456060050e42a199a68ab86ee679f79))

<a name="v4.3.0"></a>
## 4.3.0 (2022-03-27)

### Features

* **reporter:** Allow GraphicalReportHandler to disable url display (#137) ([b6a6cc9e](https://github.com/zkat/miette/commit/b6a6cc9e75198e53f1413c88694d950006833e05))

### Bug Fixes

* **colors:** handler_opts.color(false) should disable color (#133) ([209275d4](https://github.com/zkat/miette/commit/209275d4377fcaf397bde931f2972a1b7d8ce55c))
* **handler:** Apply MietteHandlerOpts::graphical_theme (#138) ([70e84f9a](https://github.com/zkat/miette/commit/70e84f9a019008a38ed22416f1fc399d32f50db4))

### Documentation

* **readme:** Fix a couple links (#141) ([126ffc58](https://github.com/zkat/miette/commit/126ffc5834683726fc8efff6604735f8cc806f9b))

### Miscellaneous Tasks

* **deps:** Update textwrap to 0.15.0 (#143) ([2d0054b3](https://github.com/zkat/miette/commit/2d0054b3c9bf1f6bdbea624ba65593ca41f03999))

<a name="v4.2.1"></a>
## 4.2.1 (2022-02-25)

### Bug Fixes

* **handlers:** source code propagation for JSON handler (#122) ([50bcec90](https://github.com/zkat/miette/commit/50bcec909aa60c20d4981484195130fbb9f3cacb))
* **clippy:** 1.59.0 clippy fix ([fa5b5fee](https://github.com/zkat/miette/commit/fa5b5fee549e53e9cf0c1d946bef242eebee6c48))
* **docs:** Docs overhaul (#124) ([5d23c0d6](https://github.com/zkat/miette/commit/5d23c0d61d0c7e778579d4d290b1f6e2c53fba31))

<a name="v4.2.0"></a>
## 4.2.0 (2022-02-22)

### Features

* **derive:** allow `Report` in `related` (#121) ([75d4505e](https://github.com/zkat/miette/commit/75d4505e7d55e816cac071eb126213b72bf48982))

<a name="v4.1.0"></a>
## 4.1.0 (2022-02-20)

`.with_source_code()` is here!!

### Features

* **report:** add `with_source_code` ([50519264](https://github.com/zkat/miette/commit/50519264d47d35ecbbe4846cf7d64139854adf6c))
* **handlers:** propagate source code to related errors ([3a17fcea](https://github.com/zkat/miette/commit/3a17fceabb0641c3d44f73a62b8116cc87d3c6bb))

### Bug Fixes

* **derive:** absolute path references to Diagnostic (#118) ([6eb3d2d8](https://github.com/zkat/miette/commit/6eb3d2d8a63bc38a53a472932a476b78c4fdb34c))

<a name="v4.0.1"></a>
## 4.0.1 (2022-02-18)

### Bug Fixes

* **graphical:** boolean was messing up graphical display ([5c085b39](https://github.com/zkat/miette/commit/5c085b39e28ad87777135bcca30d2ac99039de39))

<a name="v4.0.0"></a>
## 4.0.0 (2022-02-18)

### Breaking Changes

* **colors:** treat no-color mode as no-color instead of narratable (#94) ([9dcce5f1](https://github.com/zkat/miette/commit/9dcce5f1bdd76e7564d604ab8b87bbc7caad310a))
    * **BREAKING CHANGE**: NO_COLOR no longer triggers the narrated handler. Use
NO_GRAPHICS instead.
* **derive:** Make derive macro `diagnostic` attribute more flexible. (#115) ([5b8b5478](https://github.com/zkat/miette/commit/5b8b5478b63e91a51fadec87c6fed3e60d192b60))
    * **BREAKING CHANGE**: `diagnostic` attribute duplication will now error.

### Features

* **Report:** adds `.context()` method to the `Report` (#109) ([2649fd27](https://github.com/zkat/miette/commit/2649fd27c47893dc3ba2445a9932600d1b3d3e63))

### Bug Fixes

* **handlers:** Fix label position (#107) ([f158f4e3](https://github.com/zkat/miette/commit/f158f4e370bd25d589136a69058a6dff5e8aa468))


<a name="v3.3.0"></a>
## 3.3.0 (2022-01-08)

### Features

* **deps:** Bump owo-colors to 3.0.0 ([fe77d8c7](https://github.com/zkat/miette/commit/fe77d8c75478e9915a61613ec94b3de0a70e5e26))
* **handlers:** Add JSON handler (#90) ([53b24682](https://github.com/zkat/miette/commit/53b246829a2cf6317fe1ac0cf7603e37ffde349f))

### Bug Fixes

* **chain:** correct `Chain` structure exported (#102) ([52e5ec80](https://github.com/zkat/miette/commit/52e5ec806457c2784d85dc4e4a332c07e6eea818))
* **json:** proper escapes for JSON strings (#101) ([645ef6a1](https://github.com/zkat/miette/commit/645ef6a1b66a9a05f97883535f162cab4d0483f5))
* **deps:** switch to terminal_size ([51146535](https://github.com/zkat/miette/commit/51146535f5ea9eeaff1163d99d8b89a2567e93dd))

<a name="v3.2.0"></a>
## 3.2.0 (2021-10-06)

### Features

* **tabs:** Add replace tabs with spaces option (#82) ([1f70140c](https://github.com/zkat/miette/commit/1f70140c2e6a57237de78dab022e29440f98ae33))

### Bug Fixes

* **read_span** prevent multiline MietteSpanContents from skipping lines (#81) ([cb5a919d](https://github.com/zkat/miette/commit/cb5a919deb87f8fba748bed73b6f22ebe4e3390f))

<a name="v3.1.0"></a>
## 3.1.0 (2021-10-01)

### Features

* **SourceSpan:** add impl From<Range> (#78) ([0169fe20](https://github.com/zkat/miette/commit/0169fe20e7868cfee594b26b063267d17be0a84e))

<a name="v3.0.1"></a>
## 3.0.1 (2021-09-26)

No code changes this release. Just improved documentation and related tests.

<a name="v3.0.0"></a>
## 3.0.0 (2021-09-22)

It's here! Have fun!

It's a pretty significant change, so if you were using `miette`'s snippet
support previously, you'll need to update your code.

### Bug Fixes

* **report:** miscellaneous, hacky tweaks to graphical rendering ([80036781](https://github.com/zkat/miette/commit/80036781cda11de071187d59127c6d1c7cafa879))
* **protocol:** implement source/cause for Box<dyn Diagnostic> ([c3505fac](https://github.com/zkat/miette/commit/c3505fac269aebadc0fd62f9ee4e04bd00970dae))
* **derive:** Code is no longer required ([92a31509](https://github.com/zkat/miette/commit/92a3150921d366e2850249be14259a550fcee3bb))
* **graphical:** stop rendering red vbars before the last item ([e2e4027f](https://github.com/zkat/miette/commit/e2e4027fda55415ac07590e2d33e1f6d762df439))
* **graphical:** fix coalescing adjacent things when they cross boundaries ([18e0ed77](https://github.com/zkat/miette/commit/18e0ed7749d33c5030a5fa2f8eabdc50a717573b))
* **context:** get labels/snippets working when using .context() ([41cb710a](https://github.com/zkat/miette/commit/41cb710a7dff59a9bde126556be7f5a877c1dafd))
* **api:** put panic handler properly behind a flag ([55ca8e0b](https://github.com/zkat/miette/commit/55ca8e0b7ff60cef8a7f75c29fa78edbb8114043))
* **deps:** We do not use ci_info directly anymore ([8d1170e2](https://github.com/zkat/miette/commit/8d1170e2decee290f1679b823eb0f7ea04f3fb39))
* **graphical:** Fix off-by-one span_applies calculation (#70) ([a6902042](https://github.com/zkat/miette/commit/a69020422e546efbe9256e30d9da10ad67f5ce03))
* **theme:** remove code styling ([ce0dea54](https://github.com/zkat/miette/commit/ce0dea541a60f274bd97d3a1cfdaa9d217b632e2))
* **graphical:** render URLs even without a code ([77c5899b](https://github.com/zkat/miette/commit/77c5899bbd7c46733ea208a7506c1d07b773bc2c))
* **deps:** remove dep on itertools ([612967d3](https://github.com/zkat/miette/commit/612967d381f05e2e5a27e39a7a66942c7ec396f3))

### Features

* **report:** make a single big MietteHandler that can switch modes ([4c2463f9](https://github.com/zkat/miette/commit/4c2463f9aeaef43f69cac3abae059973f430bfa8))
    * **BREAKING CHANGE**: linkification option method on GraphicalReportHandler has been changed to .with_links(bool)
* **deps:** move fancy reporter (and its deps) to a feature ([247e8f8b](https://github.com/zkat/miette/commit/247e8f8b39271ffa7fd2c461e8ed769bebcbc589))
    * **BREAKING CHANGE**: The default fancy reporter is no longer available unless you enable the "fancy" feature. This also means you will not be pulling in a bunch of deps if you are using miette for a library
* **footer:** add footer support to graphical and narrated ([93374173](https://github.com/zkat/miette/commit/93374173e30c5d4ccdd0aa16557d68d54aaf3e59))
* **theme:** rename some theme items for clarity ([c5c0576e](https://github.com/zkat/miette/commit/c5c0576ec69d5ccc3700dd6fc411d071bb0114a7))
    * **BREAKING CHANGE**: These were part of the public API, so if you were using theming, this might have broken for you
* **theme:** more styling changes ([2c437403](https://github.com/zkat/miette/commit/2c43740346da954fd71653a079c53a1e9612c06f))
* **report:** add debug report as default, instead of narrated one ([9841d6fd](https://github.com/zkat/miette/commit/9841d6fd77ce665acb40f7459f410e83cdc131c0))
* **labels:** replace snippet stuff with simpler labels (#62) ([f87b158b](https://github.com/zkat/miette/commit/f87b158b22f6f943cd7e52ca186b5f3c542194fd))
* **protocol:** Make SourceCode Send+Sync ([9aa8ff0d](https://github.com/zkat/miette/commit/9aa8ff0d3190e0fb1ee5ad48cb540b961fc46366))
* **handlers:** Update graphical handler to use new label protocol (#66) ([4bb9d121](https://github.com/zkat/miette/commit/4bb9d12102c1e24b6f063e43bd87e894f16683e8))
* **report:** nicer, non-overlapping same-line highlights ([1a0f359e](https://github.com/zkat/miette/commit/1a0f359e3cd386f2738052d68790a3b54e64055b))
* **panic:** Add basic panic handler and installation function ([c6daee7b](https://github.com/zkat/miette/commit/c6daee7b930ff7b76ce6ab394460c7659124f2d6))
* **panic:** add backtrace support to panic handler and move set_panic_hook into fancy features ([858ac169](https://github.com/zkat/miette/commit/858ac169353e653ed0795fb1962f4ddde8fc3d06))
* **graphical:** simplify graphical header and remove a dep ([6c648463](https://github.com/zkat/miette/commit/6c6484633ed1580047fb3dc820486f3264fb6a19))
* **related:** Add related diagnostics (#68) ([8e11baab](https://github.com/zkat/miette/commit/8e11baab7b7b57d6220cf31a82715ac9b8b76f2f))
* **graphical:** compact graphical display a bit ([db637a36](https://github.com/zkat/miette/commit/db637a366b1bcf54ff761a43ddb2cdfaaac0e481))
* **graphical:** compact even more ([72c0bb9e](https://github.com/zkat/miette/commit/72c0bb9e65fa2fc7e8a1cf61ab1fe636ec063d2e))
* **graphical:** add theming customization for linums ([717f8e3d](https://github.com/zkat/miette/commit/717f8e3d8837e14d76825603c0cbdcabb66950ff))
* **handler:** context lines config support ([b33084bd](https://github.com/zkat/miette/commit/b33084bdbfeec90208f9dacd1976c8bde31642f3))
* **narrated:** updated narrated handler ([fbf6664e](https://github.com/zkat/miette/commit/fbf6664ef5582c9a15bba881a6ee1ca058102d7f))
* **narrated:** global footer and related diagnostics support ([3213fa61](https://github.com/zkat/miette/commit/3213fa610a17e3f52ece8c069eb123b2a38f1266))

<a name="3.0.0-beta.0"></a>
## 3.0.0-beta.0 (2021-09-22)

Time to get ready for release!

### Bug Fixes

* **graphical:** stop rendering red vbars before the last item ([dc2635e1](https://github.com/zkat/miette/commit/dc2635e15154ab33506bdeae46f34c99b403fff2))
* **graphical:** fix coalescing adjacent things when they cross boundaries ([491ce7c0](https://github.com/zkat/miette/commit/491ce7c0ce1f04c9b6fc09c250f188c1ec77df53))
* **context:** get labels/snippets working when using .context() ([e0296578](https://github.com/zkat/miette/commit/e02965787b5e6206dad46556a50edae578449789))

### Features

* **report:** nicer, non-overlapping same-line highlights ([338c885a](https://github.com/zkat/miette/commit/338c885a305035fc21f63e3566131af5befa14b3))
* **panic:** Add basic panic handler and installation function ([11a708a2](https://github.com/zkat/miette/commit/11a708a2244f1838351b2b59bfc407febe3c2a0e))
* **panic:** add backtrace support to panic handler and move set_panic_hook into fancy features ([183ecb9b](https://github.com/zkat/miette/commit/183ecb9b78a1c22d832e979db5054dcac36d8b7a))
* **graphical:** simplify graphical header and remove a dep ([9f36a4c2](https://github.com/zkat/miette/commit/9f36a4c25362486dfcf9ad2bd66c45e47d6fa4d2))
* **related:** Add related diagnostics (#68) ([25e434a2](https://github.com/zkat/miette/commit/25e434a2cec93e41f020372dedcf395adb2564de))
* **graphical:** compact graphical display a bit ([9d07dc5a](https://github.com/zkat/miette/commit/9d07dc5a1c190b6d52770e4f3c4a1dabd53e0fd5))
* **graphical:** compact even more ([712e75fd](https://github.com/zkat/miette/commit/712e75fd8c25c6309a49c7f81f83d5b6f855594c))

<a name="3.0.0-alpha.0"></a>
## 3.0.0-alpha.0 (2021-09-20)

This is the first WIP alpha release of miette 3.0!

It's a MAJOR rewrite of the entire snippet definition and rendering system,
and you can expect even more changes before 3.0 goes live.

In the meantime, there's this. :)

### Bug Fixes

* **report:** miscellaneous, hacky tweaks to graphical rendering ([8029f9c6](https://github.com/zkat/miette/commit/8029f9c6c39d9d9592a2183380e83add8f9938e1))
* **protocol:** implement source/cause for Box<dyn Diagnostic> ([3e8a27e2](https://github.com/zkat/miette/commit/3e8a27e263d6b22c1f2a9b192b2d305c2f0aa367))
* **derive:** Code is no longer required ([8a0f71e6](https://github.com/zkat/miette/commit/8a0f71e6d11cd6f89fbad67cce46e34aa75f3b39))

### Features

* **report:** make a single big MietteHandler that can switch modes ([3d74a500](https://github.com/zkat/miette/commit/3d74a500c3193fb1dff26591191a67eaab079671))
    * **BREAKING CHANGE**: linkification option method on GraphicalReportHandler has been changed to .with_links(bool)
* **deps:** move fancy reporter (and its deps) to a feature ([bc495e6e](https://github.com/zkat/miette/commit/bc495e6ed49f227895260d8877685e267c0d5814))
    * **BREAKING CHANGE**: The default fancy reporter is no longer available unless you enable the "fancy" feature. This also means you will not be pulling in a bunch of deps if you are using miette for a library
* **footer:** add footer support to graphical and narrated ([412436cd](https://github.com/zkat/miette/commit/412436cd689ac55e9ec8172f772c321288629553))
* **theme:** rename some theme items for clarity ([12a9235b](https://github.com/zkat/miette/commit/12a9235bec53d6dbd347f43dfaef167696a381e1))
    * **BREAKING CHANGE**: These were part of the public API, so if you were using theming, this might have broken for you
* **theme:** more styling changes ([9901030e](https://github.com/zkat/miette/commit/9901030eb160e72bc64144c44b8bf48cce8dfe48))
* **report:** add debug report as default, instead of narrated one ([eb1b7222](https://github.com/zkat/miette/commit/eb1b7222fc5b73b6fb8fee90b1de27e0b8d6d588))
* **labels:** replace snippet stuff with simpler labels (#62) ([0ef2853f](https://github.com/zkat/miette/commit/0ef2853f27ea84407789cbd0680956f9e3ee9168))
* **protocol:** Make SourceCode Send+Sync ([eb485658](https://github.com/zkat/miette/commit/eb485658cc5a0df894c59d6ad29f945fff2839a5))
* **handlers:** Update graphical handler to use new label protocol (#66) ([6cd44a86](https://github.com/zkat/miette/commit/6cd44a86c6e6f1d9c79006d4cfa89220dbd3a7b4))


<a name="2.2.0"></a>
## 2.2.0 (2021-09-14)

So it turns out [`3.0.0` is already under way](https://github.com/zkat/miette/issues/45), if you didn't already hear!

It's going to be an exciting release, but we'll still be putting out bugfixes
and (backwards-compatible) features in the `2.x` line until that's ready.

And there's definitely stuff in this one to be excited about! Not least of all
the ability to _forward_ diagnostic metadata when wrapping other
`Diagnostic`s. Huge thanks to [@cormacrelf](https://github.com/cormacrelf) for
that one!

We've also got some nice improvements to reporter formatting that should make
output look at least a little nicer--most notably, we now wrap messages and
footers along the appropriate column so formatting keeps looking good even
when you use newlines!

Finally, huge thanks to [@icewind1991](https://github.com/icewind1991) for
fixing a [really weird-looking bug](https://github.com/zkat/miette/pull/52)
caused by an off-by-one error. Oopsies 😅

### Features

* **report:** wrap multiline messages to keep formatting ([f482dcec](https://github.com/zkat/miette/commit/f482dcec6a4e981c256854f73506ed01abaa65f9))
* **report:** take terminal width into account for wrapping text ([bc725324](https://github.com/zkat/miette/commit/bc72532465bde00e11d83ff4a9f767051ee6771d))
* **report:** make header line as wide as terminal ([eaebde92](https://github.com/zkat/miette/commit/eaebde92cf528d50d799dd60acd98b16978e8681))
* **derive:** Add `#[diagnostic(forward(field_name), code(...))]` (#41) ([2fa5551c](https://github.com/zkat/miette/commit/2fa5551c81831734fd9a162463a4a939dff9dfba))

### Bug Fixes

* **report:** get rid of the weird arrow thing. it does not look good ([1ba3f2f5](https://github.com/zkat/miette/commit/1ba3f2f5d292419571302477195836f89d9c7cb5))
* **report:** fix wrapping for header and add wrapping for footer ([eb07d5bd](https://github.com/zkat/miette/commit/eb07d5bd66928457b4f3affe96aa6a0d39f642f7))
* **report:** Fix end of previous line wrongly being included in highlight (#52) ([d994add9](https://github.com/zkat/miette/commit/d994add912700873de3ebdb8d14d81516955c901))

<a name="2.1.2"></a>
## 2.1.2 (2021-09-10)

So it turns out I forgot to make snippets and other stuff forward through when
you use `.context()` &co. This should be fixed now 😅

### Bug Fixes

* **context:** pass on diagnostic metadata when wrapping with `Report` ([e4fdac38](https://github.com/zkat/miette/commit/e4fdac38ea8c295468ed0fce563a2df29241986a))

<a name="2.1.1"></a>
## 2.1.1 (2021-09-09)

This is a small, but visually-noticeable bug fix. I spent some time playing
with colors and styling and made some fixes that will improve where people's
eyes are drawn to, and also take into account color visibility issues a bit
more.

### Bug Fixes

* **report:** don't color error message text to draw eyes back to it ([6422f821](https://github.com/zkat/miette/commit/6422f8217495aeef38af4eb00feeb73ced36f7bf))
* **reporter:** improve color situation and style things a little nicer ([533ff5f3](https://github.com/zkat/miette/commit/533ff5f348324132044bd2782a17fd6c81c08259))

<a name="2.1.0"></a>
## 2.1.0 (2021-09-08)

This is a small release with a handful of quality of life improvements (and a small bugfix).

### Features

* **printer:** use uparrow for empty highlights and fix 0-offset display bug ([824cd8be](https://github.com/zkat/miette/commit/824cd8bebea2ae43a29d9d744d0386d00cc943e0))
* **derive:** make #[diagnostic] optional for enums, too ([ffe1b558](https://github.com/zkat/miette/commit/ffe1b558d0d7284e39fcb38c4f410cddb4cdb4bd))

<a name="2.0.0"></a>
## 2.0.0 (2021-09-05)

This release overhauls the toplevel/main experience for `miette`. It adds a
new `Report` type based on `eyre::Report` and overhauls various types to fit
into this model, as well as prepare for some [future changes in
Rust](https://github.com/nrc/rfcs/pull/1) that will make it possible to
integrate `miette` directly with crates like `eyre` instead of having to use
this specific `Report`.

On top of that, it includes a couple of nice new features, such as
`#[diagnostic(transparent)]`, which should be super useful when wrapping other
diagnostics with your own types!

### Breaking Changes

* **report:** anyhow-ify DiagnosticReport (#35) ([3f9da04b](https://github.com/zkat/miette/commit/3f9da04b866f3fd90f88e7e60f9fb7a322aef568))
    * `DiagnosticReport` is now just `Report`, and is a different, `eyre::Report`-like type.
    * `DiagnosticResult` is now just `Result`.
    * `.into_diagnostic()` now just transforms the error into a `Report`.
    * `DiagnosticReportPrinter` has been replaced with `ReportHandler`
    * `set_printer` has been replaced by `set_hook`
    * `code` is now optional.
    * `.into_diagnostic()` no longer takes a `code` argument.
    * `#[diagnostic]` is now optional when deriving `Diagnostic`.

### Features

* **derive:** Add `#[diagnostic(transparent,forward)]` (#36) ([53f5d6d1](https://github.com/zkat/miette/commit/53f5d6d1d62845b52e590fed5ce91a643b6e11f3))
* **Source:** impl Source for str, &str (make &'static str usable for testing) (#40) ([50c7a883](https://github.com/zkat/miette/commit/50c7a88360dc7cef815af2dbb9dc18ede0d1fdb4))
* **source:** Remove bound `T: Clone` from `Source` implementation for `Cow`. (#42) ([0427c9f9](https://github.com/zkat/miette/commit/0427c9f9666222084cb4494aabbd3e7dc5cdb789))

### Bug Fixes

* **reporter:** Only inc the line count if we haven't already done so with '\n' or '\r\n' (#37) ([5a474370](https://github.com/zkat/miette/commit/5a474370ddda92a3a92b6b84cd561ecaf4d6d858))
* **printer:** Show snippet message for unnamed sources (#39) ([84219f6c](https://github.com/zkat/miette/commit/84219f6c80c2c432fbeb4c40a591380285de8767))

<a name="1.1.0"></a>
## 1.1.0 (2021-08-29)

This is a small release of patches entirely not my own!

The exciting new feature is the ability to do `thiserror`-style
`#[diagnostic(transparent)]` when using the derive macro, which will defer
diagnostics to a Diagnostic referred to by the struct/enum!

Big thanks to [@cormacrelf](https://github.com/cormacrelf) and
[@felipesere](https://github.com/felipesere) for your contributions!

### Features

* **derive:** Add `#[diagnostic(transparent,forward)]` (#36) ([53f5d6d1](https://github.com/zkat/miette/commit/53f5d6d1d62845b52e590fed5ce91a643b6e11f3))

### Bug Fixes

* **reporter:** Only inc the line count if we haven't already done so with '\n' or '\r\n' (#37) ([5a474370](https://github.com/zkat/miette/commit/5a474370ddda92a3a92b6b84cd561ecaf4d6d858))

<a name="1.0.1"></a>
## 1.0.1 (2021-08-23)

This is a (literally) small release. I noticed that the crate's size had
increased significantly before I realized cargo was including the `images/`
folder. This is not needed, as these images are just hosted on GitHub.

`miette` should be smaller now, I hope :)

#### Bug Fixes

* **crate:**  reduce crate size by removing images ([5f74da67](https://github.com/zkat/miette/commit/5f74da671f2444efc4840c11492773a46cecf7e9))


<a name="1.0.0"></a>
## 1.0.0 (2021-08-23)

...you know what? I'm just gonna tag 1.0.0, because I don't want sub-1.0
versions anymore, but the Cargo ecosystem buries pre-releases pretty
thoroughly. Integers are cheap!

So here we are! We made it to 1.0, and with some _really_ nice goodies to boot.

Most fun is the fact that the default printer now has *clickabble url linking*
support. A new `Diagnostic::url()` method has been added to the protocol that,
is used to figure out what URL to send folks to! This should work on most
"modern" terminals, but more thorough support checking will be done in the
future. And of course, the narrated reporter prints them out too.

I also took the time to completely redo how messages, labels, and filenames
are handled in the system, and this is a pretty big change you might run into.
Godspeed!

Last but not least, we got our first external contribution! Thank you to
[@martica](https://github.com/martica) for the bug fix!

Anyway, here's to 1.0, and to many more after that. Enjoy! :)

#### Breaking Changes

* **snippets:**  Overhauled how snippets handle labels, sources, and messages, including the derive macro ([61283e9e](https://github.com/zkat/miette/commit/61283e9efe2825425c41027b3dbb5f4f9c9d83fb)

#### Features

* **links:**  added URL linking support and automatic docs.rs link generation ([7e76e2de](https://github.com/zkat/miette/commit/7e76e2dea4adf0e4a1349e049495c1f5a0bdab87))
* **theme:**  Add an initial `rgb` style with nicer colors ([3546dcec](https://github.com/zkat/miette/commit/3546dcec988ea40cc6aa8dd94c29432830cef662)) - [@martica](https://github.com/martica)

#### Bug Fixes

* **printer:**  clamp highlight length to at least 1 (#32) ([9d601599](https://github.com/zkat/miette/commit/9d6015996bf3010b573b9bb5d0e48cb85f290460))


<a name="1.0.0-beta.1"></a>
## 1.0.0-beta.1 (2021-08-22)

It's happening, folks! `miette` is now working towards stability and is now in
beta! We'll keep it like this for a little while until a few more people have
tried it out and given feedback. New features may still be added, and breaking
changes may still happen, but `miette` is now considered "good enough to use",
and breaking changes are expected to be more rare.

Oh, and as part of this release, the docs were overhauled, particularly the
README, so you might want to take a gander at them!

#### Breaking Changes

* **printer:**  rename default printer and consistify some naming conventions with printing ([aafa4a3d](https://github.com/zkat/miette/commit/aafa4a3de1298dd8e7625138d09a408ff3579d3f), breaks [#](https://github.com/zkat/miette/issues/))
* **into_diagnostic:**  .into_diagnostic() is now generic across any impl fmt::Display instead of expecting a `dyn` ([c1da4a0d](https://github.com/zkat/miette/commit/c1da4a0d2744e94e409cabeafe911e99598d4ee3))

#### Features

* **error:**  diagnostic-ify MietteError ([e980b723](https://github.com/zkat/miette/commit/e980b7237334b56f7b8c092956d35cd2bbadac41))

#### Bug Fixes

* **derive:**  #[diagnosic(severity)] works for named and unnamed variants/structs now ([adf0bc93](https://github.com/zkat/miette/commit/adf0bc933f62852514067ade96e07362c889f012))
* **protocol:**  oops, missed a spot after a rename ([5c077d30](https://github.com/zkat/miette/commit/5c077d30a4aca71f71e61b2561081575c04a4d64))


<a name="0.13.0"></a>
## 0.13.0 (2021-08-21)

This release includes some accessibility improvements: miette now includes a "narratable" printer that formats diagnostics like this:

```
Error: Received some bad JSON from the source. Unable to parse.
    Caused by: missing field `foo` at line 1 column 1700

Begin snippet for https://api.nuget.org/v3/registration5-gz-semver2/json.net/index.json starting
at line 1, column 1659

snippet line 1: gs":["json"],"title":"","version":"1.0.0"},"packageContent":"https://api.nuget.o
    highlight starting at line 1, column 1699: last parsing location

diagnostic help: This is a bug. It might be in ruget, or it might be in the source you're using,
but it's definitely a bug and should be reported.
diagnostic error code: ruget::api::bad_json
```

This style is the default in a number of situations:

1. The `NO_COLOR` env var is present and set, and not `0`.
2. The `CLICOLOR` env var is present and not set to `1`.
3. `stdout` or `stderr` are not TTYs.
4. A CI environment is detected.

You can override and customize this behavior any way you want by using the
`miette::set_reporter()` function at the toplevel of your application, but we
encourage you to at least make the narratable printer an option for your
users, since miette's default printer is exceptionally bad for screen
readers.

Our hope is that this release is only the starting point towards making
miette's error reporting not just really fancy and cool, but friendly and
accessible to everyone.

#### Features

* **printer:**  added (and hooked up) an accessible report printer ([5369a942](https://github.com/zkat/miette/commit/5369a9424e7ed2c66b193b85422fe8b98bc37b6c))


<a name="0.12.0"></a>
## 0.12.0 (2021-08-21)

This is a SUPER EXCITING release! With this, miette now has a full-featured
pretty-printer that can handle cause chains, snippets, help text, and lots
more!

Check out [the serde_json
example](https://github.com/zkat/miette/blob/5fd2765bf05edf25251ce199994b8815524fd47d/images/serde_json.png)
to see a "real-world" case!

This release also adds support for full `thiserror`-style format strings to
the `help()` diagnostic derive attribute!

We're rapidly approaching a 1.0-beta release. One more extra-fun treat left
and we can start stabilizing!

#### Features

* **derive:**  format string support for help() ([8fbad1b1](https://github.com/zkat/miette/commit/8fbad1b1cd173ce3c0b803f8b2db013e278c63a6))
* **printer:**  lots of small improvements to printer ([5fbcd530](https://github.com/zkat/miette/commit/5fbcd53026c131ceafe2a66bebbc20de570363c9))
* **reporter:**  fancy new reporter with unicode, colors, and multiline (#23) ([d675334e](https://github.com/zkat/miette/commit/d675334e48ddc188a34e166ad040eaceda117d0a))


<a name="0.11.0"></a>
## 0.11.0 (2021-08-18)

BIG changes this time. The whole end-to-end experience for tossing around
Diagnostics in your code has been overhauled, printing reports is easier than
ever, and we even have an `eyre::Report`-style wrapper you can pass around in
app-internal returns!

#### Features

* **reporter:**  Overhauled return type/main/DiagnosticReport experience. ([29c1403e](https://github.com/zkat/miette/commit/29c1403efdd7fd218f240ac458fd19bba17e9551))


<a name="0.10.0"></a>
## 0.10.0 (2021-08-17)

Lots of goodies in this release! I'm working hard on the [1.0.0
Roadmap](https://github.com/zkat/miette/issues/10), so things are changing
pretty quick, and I thought it would be nice to release this checkpoint.
#### Bug Fixes

* **protocol:**  keep the owned spans ([49151bb0](https://github.com/zkat/miette/commit/49151bb0950c0db9d2743c8fb78dcacfc27bc750))

#### Features/Breaking Changes

* **derive:**  Allow anything Clone + Into<SourceSpan> to be used as a Span ([385171eb](https://github.com/zkat/miette/commit/385171eb8178ce2e7d6d2d2849b78e0f09feb721))
* **offsets:**
  *  nice utility function to get an offset from a Rust callsite ([26f409c5](https://github.com/zkat/miette/commit/26f409c5252c3fda5ead140eb4d5ec282f47f0f7))
  *  utility function for converting from line/col to offset ([75c23127](https://github.com/zkat/miette/commit/75c2312755bf714c112badf6310b2bff1633f6bc))
  *  more utility From impls for SourceSpan ([95200366](https://github.com/zkat/miette/commit/95200366a1639b0b729db460ae1e50cce6fee9de))
* **protocol:**
  *  add Source impls for Cow and Arc ([53074d34](https://github.com/zkat/miette/commit/53074d3488e1404331fc1ca3c5e068ac57e9a852))
  *  reference-based DiagnosticReport! ([f390520b](https://github.com/zkat/miette/commit/f390520b45823d65055f9f872016e4ee27c0c20a))



<a name="0.9.0"></a>
## 0.9.0 (2021-08-17)

Yay new version already! A pretty significant API change, too! ��

#### Breaking Changes

`SourceSpan`s have changed a bit: for one, they're based on offset/length now,
instead of start/end. For two, they have a new `Option<String>` field,
`label`, which is meant to be used by reporters in different contexts. For
example, highlight snippets will use them as the labels for underlined
sections of code, while the snippet context will use the label as the "file
name" for the Source they point to.

  * **protocol:** new SourceSpans with labels ([acfeb9c5](https://github.com/zkat/miette/commit/acfeb9c5b0e390c924194ee0363fc49fa8defbac))

#### Bug Fixes

* **derive:**  allow unused variables for the snippets method ([f704d6a9](https://github.com/zkat/miette/commit/f704d6a9ae971dfe61fe9a0e0b4a1a7f98fd37bc))

#### Features

* **protocol:** implement From<(usize, usize)> for SourceSpan ([36b86df9](https://github.com/zkat/miette/commit/36b86df9f51984405efa6f38be8bbb984d605207))



<a name="0.8.1"></a>
## 0.8.1 (2021-08-17)

Just a small bump to update the readme (and docs.rs in the process) with the
new snippet derive stuff. No notable changes.

<a name="0.8.0"></a>
## 0.8.0 (2021-08-17)

You can full-on use `#[derive(Diagnostic)]` to define snippets now. That's a
big deal.

#### Features

* **derive:**  Support for deriving snippet method (#18) ([f6e6acf2](https://github.com/zkat/miette/commit/f6e6acf2d2c301fd411c7c9c4b63a2b19aa69242))

<a name="0.7.0"></a>
## 0.7.0 (2021-08-16)

Welp. `0.6.0` was basically completely broken, so I tore out the
`darling`-based derive macros and rewrote the whole thing using `syn`, and
things are much better now!

There's still a few bits and bobs to add, like snippets (oof. big.), and full
help format string support (they don't quite work in enums right now), but
otherwise, this is pretty usable~

#### Features

* **derive:**  improved derive support, including partial help format string support! ([9ef0dd26](https://github.com/zkat/miette/commit/9ef0dd261fa537b280f32ea6f149785a69e33938))

#### Bug Fixes

* **derive:**  move to plain syn to fix darling issues ([9a78a943](https://github.com/zkat/miette/commit/9a78a943950078c879a1eb06baf819348139e1de))


<a name="0.6.0"></a>
## 0.6.0 (2021-08-15)

We haz a basic derive macro now!

#### Features

* **derive:**  added basic derive macro ([0e770270](https://github.com/zkat/miette/commit/0e7702700de8a4cd9022d660aaf363b735943d55))


<a name="0.5.0"></a>
## 0.5.0 (2021-08-14)

I decided to yank some handy (optional) utilities from a project I'm using
`miette` in. These should make using it more ergonomic.

#### Features

* **utils:**  various convenience utilities for creating and working with Diagnostics ([a9601368](https://github.com/zkat/miette/commit/a960136802834bd3741ef637d91f73287870b1ad))


<a name="0.4.0"></a>
## 0.4.0 (2021-08-11)

Time for another (still experimental!) change to `Diagnostic`. It will
probably continue to change as miette gets experimented with, until 1.0.0
stabilizes it. But for now, expect semi-regular breaking changes of this kind.

Oh and I tracked down a rogue `\n` that was messing with the default reporter
and managed to get out of it with at least some of my sanity.

#### Breaking Changes

* **protocol:**  Simplify protocol return values further ([02dd1f84](https://github.com/zkat/miette/commit/02dd1f84d45c01fb4de2d31c158a7b6e08455f72), breaks [#](https://github.com/zkat/miette/issues/))

#### Bug Fixes

* **reporter:**
  *  fix reporter and tests... again ([d201dde4](https://github.com/zkat/miette/commit/d201dde4b559a2baa4259a0845582a5d14453c5a))
  *  fix extra newline after header ([0d2e3312](https://github.com/zkat/miette/commit/0d2e3312a4a262e99a131bc893097d295e59e8ca))


<a name="0.3.1"></a>
## 0.3.1 (2021-08-11)

This is a tiny release to fix a reporter rendering bug.

#### Bug Fixes

* **reporter:**  fix missing newline before help text ([9d430b6f](https://github.com/zkat/miette/commit/9d430b6f477fd8991ce217dffdbce8fbd28dcd7e))



<a name="0.3.0"></a>
## 0.3.0 (2021-08-08)

This version is the result of a lot of experimentation with getting the
`Diagnostic` API right, particularly `Diagnostic::snippets()`, which is
something that should be writable in several different ways. As such, it
includes some breaking changes, but they shouldn't be too hard to figure out.

#### Breaking Changes

* **protocol:**
  *  improvements to snippets API ([3584dc60](https://github.com/zkat/miette/commit/3584dc600c2b8b0f84a2a0c59856da9a9dc7fbab))
  *  help is a single Display ref now. ([80e7dabb](https://github.com/zkat/miette/commit/80e7dabbe450d4a78ed18174e2a383a6a1ed0557))

#### Bug Fixes

* **tests:**  updating tests ([60bdf47e](https://github.com/zkat/miette/commit/60bdf47e297999b48345b39ba1a3aacbbf79e6fc))

<a name="0.2.1"></a>
## 0.2.1 (2021-08-05)

I think this is the right thing to do re: From!

#### Bug Fixes

* **protocol:**  fix the default From<:T Diagnostic> implementation to cover more cases. ([781a51f0](https://github.com/zkat/miette/commit/781a51f03765c7351a95b34e8391f6a0cf5fc37c))

<a name="0.2.0"></a>
## 0.2.0 (2021-08-05)

Starting to get some good feedback on the protocol and APIs, so some improvements were made.

#### Breaking changes

You might need to add `+ Send + Sync + 'static` to your `Box<dyn Diagnostic>`
usages now, since `Diagnostic` no longer constrains on any of them.

Additionally, `Diagnostic::help()`, `Diagnostic::code()`, and `SpanContents`
have had signature changes that you'll need to adapt to.

* **protocol:**  protocol improvements after getting feedback ([e955321c](https://github.com/zkat/miette/commit/e955321cbd67372dfebb71a829ddb89baf9b169a))
* **protocol:**  Make use of ? and return types with Diagnostics more ergonomic ([50238d75](https://github.com/zkat/miette/commit/50238d75a2db2dccbe2ae2cba78d0dd6eac4ef2a))

<a name="0.1.0"></a>
## 0.1.0 (2021-08-05)

I'm really excited to put out this first release of `miette`! This version
defines the current protocol and includes a basic snippet reporter. It's fully
documented and ready to be used!

_Disclaimer_: This library is still under pretty heavy development, and you should only use this if you're interested in using something experimental. Any and all design comments and ideas are welcome over on [GitHub](https://github.com/zkat/miettee)

#### Bug Fixes

* **api:**  stop re-exporting random things wtf??? ([2fb9f93c](https://github.com/zkat/miette/commit/2fb9f93cbf02c4d41a5538e98c8bea72f40c5430))
* **protocol:**  use references for all return values in Diagnostic ([c3f41b97](https://github.com/zkat/miette/commit/c3f41b972da0e89220e7d9de08f420912ec8973a))

#### Features

* **protocol:**  sketched out a basic protocol ([e2387ce2](https://github.com/zkat/miette/commit/e2387ce2edd4165d04f47a084f3f1492a5de8d9d))
* **reporter:**  dummy reporter implementation + tests ([a437f445](https://github.com/zkat/miette/commit/a437f44511768e52cfedd856b5b1432c0716f378))
* **span:**  make span end optional ([1cb0ad38](https://github.com/zkat/miette/commit/1cb0ad38524696a733f6134092ffd998f76fb142))



<a name="0.0.0"></a>
## 0.0.0 (2021-08-03)

Don't mind me, just parking this crate name.



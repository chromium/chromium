# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 1.0.31 (2024-08-03)

This release allows using `libz-rs` in the latest version, v0.2.1.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release over the course of 95 calendar days.
 - 95 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Add exclusion rule to not package github or git specific files with crate. ([`25541bd`](https://github.com/Byron/flate2-rs/commit/25541bd2aa4fd24fbb2b370eb3c2742724f956ac))
    - Crate and update changelog in preparation for release. ([`1dbed76`](https://github.com/Byron/flate2-rs/commit/1dbed76ca0f63b215faa5ad321a42bb629686456))
    - Merge pull request #415 from folkertdev/bump-version-zlib-rs-0.2.1 ([`a7853c0`](https://github.com/Byron/flate2-rs/commit/a7853c0f803abb45858baf82d504ed14d77cb8c0))
    - Release version 1.0.31: bump libz-rs-sys version ([`e6f6949`](https://github.com/Byron/flate2-rs/commit/e6f694918b3237175e3729e6365e83f9a66518a9))
    - Merge pull request #414 from yestyle/main ([`9e6af00`](https://github.com/Byron/flate2-rs/commit/9e6af00a8bd9593f9e1c6421f9d27ccdb13a03b7))
    - Remove duplicate word in top-most doc ([`411d641`](https://github.com/Byron/flate2-rs/commit/411d6414398099c85e1b8fc568ec8929d208777d))
    - Merge pull request #408 from marxin/document-read-after-end ([`1a0daec`](https://github.com/Byron/flate2-rs/commit/1a0daec607455f30651674675abb01715586f4d1))
</details>

## v1.0.30 (2024-04-29)

### Documentation

 - <csr-id-f37b1b0421ddd0070237c2f34474682276290ece/> Document expected behavior when Read is done for ZLIB and DEFLATE decoders

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 9 commits contributed to the release over the course of 3 calendar days.
 - 3 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#404](https://github.com/Byron/flate2-rs/issues/404)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#404](https://github.com/Byron/flate2-rs/issues/404)**
    - CI verifies that docs can be built ([`bc1b3e9`](https://github.com/Byron/flate2-rs/commit/bc1b3e960437ecfe8cecdaac9246805bd5fb49b6))
    - Fix CI by assuring builds work with --all-features enabled ([`5ce4154`](https://github.com/Byron/flate2-rs/commit/5ce4154d2b50dfc4b82eaea772fed82c616b501f))
 * **Uncategorized**
    - Merge pull request #405 from Byron/fix-CI ([`d3bea90`](https://github.com/Byron/flate2-rs/commit/d3bea908edfbb1415f91dfdd298e4eab3e27db97))
    - Document expected behavior when Read is done for ZLIB and DEFLATE decoders ([`f37b1b0`](https://github.com/Byron/flate2-rs/commit/f37b1b0421ddd0070237c2f34474682276290ece))
    - Merge pull request #407 from striezel-stash/actions-checkout-v4 ([`5048843`](https://github.com/Byron/flate2-rs/commit/50488437cae9967d8afe4ba086b979f3e719b87c))
    - Merge pull request #406 from striezel-stash/fix-some-typos ([`42c86ce`](https://github.com/Byron/flate2-rs/commit/42c86cedde9815f0e27ca12712814b8cfdb7f386))
    - Update actions/checkout in GitHub Actions workflows to v4 ([`f7b99e9`](https://github.com/Byron/flate2-rs/commit/f7b99e90400ca8b375a61ec70b7246b05f0cfcf3))
    - Fix typos ([`563f1c4`](https://github.com/Byron/flate2-rs/commit/563f1c42f4685bb82d6ef1baebcec40ed8cae3ef))
    - Prepare bugfix release to make docs work again ([`1126a4a`](https://github.com/Byron/flate2-rs/commit/1126a4a7584f165a59e0818566938e7e07c386c4))
</details>

## v1.0.29 (2024-04-26)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 12 commits contributed to the release over the course of 130 calendar days.
 - 195 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Merge pull request #403 from folkertdev/bump-version-zlib-rs ([`9a25bc0`](https://github.com/Byron/flate2-rs/commit/9a25bc09b12f0e7b63efb7a5e19ce118ce3caa8b))
    - Zlib-rs support version bump ([`e9c87c0`](https://github.com/Byron/flate2-rs/commit/e9c87c088ae3ab7f486065176a5134790bd504f8))
    - Merge pull request #402 from jongiddy/bufread-tests ([`8a502a7`](https://github.com/Byron/flate2-rs/commit/8a502a791fbcbdb56b20f6d6dcd7096f0c8f1a33))
    - Merge pull request #400 from folkertdev/zlib-rs-c-api ([`320e7c7`](https://github.com/Byron/flate2-rs/commit/320e7c7aa08b264e73b7cdb87e868f94ba1472ff))
    - Test that BufRead and Write can be used after decoding ([`6a26c0c`](https://github.com/Byron/flate2-rs/commit/6a26c0c569fec440482085986cb9651cdb8a9847))
    - Add zlib-rs support via the libz-rs-sys C api for zlib-rs ([`7e6429a`](https://github.com/Byron/flate2-rs/commit/7e6429a9f90d78815074142873a4d9d434dcf061))
    - Merge pull request #398 from rust-lang/fix-imports ([`ae78497`](https://github.com/Byron/flate2-rs/commit/ae784978ff22dea373cf820b3ef5fbc6e948de3f))
    - Avoid redudant imports ([`20bbd74`](https://github.com/Byron/flate2-rs/commit/20bbd749a585f29b10ff498ca4cd8724a4d08520))
    - Merge pull request #394 from icmccorm/main ([`0a584f4`](https://github.com/Byron/flate2-rs/commit/0a584f4a0c465e33b52219e8ac74f2ec9a489f9f))
    - Switched to storing mz_stream as a raw pointer to fix tree borrows violation. ([`8386651`](https://github.com/Byron/flate2-rs/commit/8386651feec5c4292c14defd22f128816f7f0e49))
    - Merge pull request #388 from JakubOnderka/patch-1 ([`f0463d5`](https://github.com/Byron/flate2-rs/commit/f0463d5ddf3a4eb4f5d0602d8f1c1031e37362e9))
    - Fix build for beta and nightly ([`8ef8ae6`](https://github.com/Byron/flate2-rs/commit/8ef8ae64cb184053c7065197eee5da45d62fc418))
</details>

## v1.0.28 (2023-10-13)

<csr-id-82e45fa8901824344cf636a37c03b2595478a4d1/>

### Other

 - <csr-id-82e45fa8901824344cf636a37c03b2595478a4d1/> Dedupe code into `write_to_spare_capacity_of_vec` helper.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 11 commits contributed to the release over the course of 48 calendar days.
 - 62 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Merge pull request #378 from Byron/prep-release ([`a99b53e`](https://github.com/Byron/flate2-rs/commit/a99b53ec65d3c2e3f703e5e865d5e886a23a83dc))
    - Merge pull request #380 from Manishearth/reset-stream ([`223f829`](https://github.com/Byron/flate2-rs/commit/223f82937a88c229260762cce9f83fd0cb100836))
    - Reset StreamWrapper after calling mz_inflate / mz_deflate ([`7a61ea5`](https://github.com/Byron/flate2-rs/commit/7a61ea527a2ed4657ba8c6b8e04360f5156dadd2))
    - Prepare next patch-release ([`1260d3e`](https://github.com/Byron/flate2-rs/commit/1260d3e88891672ad15c2d336537bdd8f607bb70))
    - Merge pull request #375 from georeth/fix-read-doc ([`f62ff42`](https://github.com/Byron/flate2-rs/commit/f62ff42615861f1d890160c5f77647466505eac0))
    - Fix and unify docs of `bufread` and `read` types. ([`5b23cc9`](https://github.com/Byron/flate2-rs/commit/5b23cc91269c44165b07cbfee493d4aa353724fd))
    - Merge pull request #373 from anforowicz/fix-spare-capacity-handling ([`f285e9a`](https://github.com/Byron/flate2-rs/commit/f285e9abac98aa4f0a15d2e79993a261f166056c))
    - Fix soundness of `write_to_spare_capacity_of_vec`. ([`69972b8`](https://github.com/Byron/flate2-rs/commit/69972b8262fbe03c28450c4c18961b41ad92af6a))
    - Dedupe code into `write_to_spare_capacity_of_vec` helper. ([`82e45fa`](https://github.com/Byron/flate2-rs/commit/82e45fa8901824344cf636a37c03b2595478a4d1))
    - Merge pull request #371 from jongiddy/jgiddy/msrv-1.53 ([`20cdcbe`](https://github.com/Byron/flate2-rs/commit/20cdcbea7d0541809beb516d8717e793d8d965eb))
    - Use explicit Default for GzHeaderState enum ([`68ba8f6`](https://github.com/Byron/flate2-rs/commit/68ba8f6e62c7c0a12e3a62cffa2f1cdf7d1f5f30))
</details>

## v1.0.27 (2023-08-12)

<csr-id-afbbf48bd4acbc2400c8adae9227b1d5890b4d42/>

### New Features

 - <csr-id-c9cf23f929187a87f10f9658523ed4f80d57a5c2/> show clear compiler error when no backend is chosen.

### Other

 - <csr-id-afbbf48bd4acbc2400c8adae9227b1d5890b4d42/> Refer to `MultiGzDecoder` from `GzDecoder`.
   This may help dealing with multi-stream gzip files.
   `MultiGzDecoder` documentation was also improved to further clarify
   why such files would exist.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 46 commits contributed to the release over the course of 102 calendar days.
 - 105 days passed between releases.
 - 2 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 2 unique issues were worked on: [#301](https://github.com/Byron/flate2-rs/issues/301), [#359](https://github.com/Byron/flate2-rs/issues/359)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#301](https://github.com/Byron/flate2-rs/issues/301)**
    - Refer to `MultiGzDecoder` from `GzDecoder`. ([`afbbf48`](https://github.com/Byron/flate2-rs/commit/afbbf48bd4acbc2400c8adae9227b1d5890b4d42))
 * **[#359](https://github.com/Byron/flate2-rs/issues/359)**
    - Show clear compiler error when no backend is chosen. ([`c9cf23f`](https://github.com/Byron/flate2-rs/commit/c9cf23f929187a87f10f9658523ed4f80d57a5c2))
    - Add test to show how `--no-default-features` should respond. ([`2684a56`](https://github.com/Byron/flate2-rs/commit/2684a56e218f02f8f0b4dce2d2a8515223974407))
 * **Uncategorized**
    - Merge pull request #369 from rust-lang/next-release ([`1f7085d`](https://github.com/Byron/flate2-rs/commit/1f7085d3d212e6301bd245005499ee7a43d81e71))
    - Prepare 1.0.27 release ([`ccd3d3a`](https://github.com/Byron/flate2-rs/commit/ccd3d3a417585094aa2661a13cd54a384728dbfc))
    - Merge pull request #367 from jongiddy/note-read-loss ([`7f96363`](https://github.com/Byron/flate2-rs/commit/7f963632372d4bc4a1c3d9c7c8fccea2509acb76))
    - Merge pull request #362 from Byron/maintenace-doc ([`b1e993a`](https://github.com/Byron/flate2-rs/commit/b1e993aa57a0af20a1de7c54cc23937fe46fdae5))
    - Fix typo ([`02cd317`](https://github.com/Byron/flate2-rs/commit/02cd317738df29252c23bdaa8e059a3bc57515f9))
    - Document that `read::GzDecoder` consumes bytes after end of gzip ([`b2079e3`](https://github.com/Byron/flate2-rs/commit/b2079e33f176bd62ac368a236f2f9e0ca44ed5b0))
    - Merge pull request #361 from PierreV23/main ([`5d462b3`](https://github.com/Byron/flate2-rs/commit/5d462b32b795cff9b9fd17f73aa744ea78c31e85))
    - Merge pull request #324 from jsha/prefer-multigz ([`956397a`](https://github.com/Byron/flate2-rs/commit/956397a96c10aeb555fd9a6e4fae6e3647371d4d))
    - Remove introductory paragraph that described other tools unrelated to `flate2` ([`fc30d9e`](https://github.com/Byron/flate2-rs/commit/fc30d9e24bffad84eba0d8bcc046e594126398a5))
    - Apply suggestions from code review ([`f0bf8a6`](https://github.com/Byron/flate2-rs/commit/f0bf8a6516936faf65b5a4ad856465d9c5ad9b95))
    - Further unify documentation, make sure sentences end with a period. ([`c9fe661`](https://github.com/Byron/flate2-rs/commit/c9fe661d150b0750385c77796b351bd62760bcde))
    - Merge pull request #366 from wcampbell0x2a/fix-readme-backend-link ([`1df4333`](https://github.com/Byron/flate2-rs/commit/1df4333a44215d425ffef6abad87619b89297748))
    - Tweak the {Gz,MultiGz}Decoder docs more ([`955728b`](https://github.com/Byron/flate2-rs/commit/955728bb94b43dc8763c667e7c5d5c09edf3b7c8))
    - Fix broken link on README.md ([`ea0ad07`](https://github.com/Byron/flate2-rs/commit/ea0ad07bd32e471e49268f990eeba996ed7fe683))
    - Apply suggestions to impartial to Gz and MultiGz implementations. ([`1e09571`](https://github.com/Byron/flate2-rs/commit/1e095719b361f0a3e857fa6d539cef7cfad4166f))
    - Add top-level comparison between `GzDecoder` and `MultiGzDecoder` ([`e21986e`](https://github.com/Byron/flate2-rs/commit/e21986e28c728ceec2c53c16ca5dbbd8a5ccfd5b))
    - Applies copies of minor improvements ([`a232574`](https://github.com/Byron/flate2-rs/commit/a2325748912d02e3e1d80d6529aa786297ab768e))
    - Merge pull request #363 from jongiddy/fix-trailing-zero-crc ([`b90ec09`](https://github.com/Byron/flate2-rs/commit/b90ec0932740efc0595e9dcb7c16c7a67b1b39dd))
    - Fix header CRC calculation of trailing zeros ([`230256e`](https://github.com/Byron/flate2-rs/commit/230256e3ade2494710687d4fe3d8e686b4426566))
    - Merge pull request #323 from jongiddy/partial-filename-write ([`d8e74e1`](https://github.com/Byron/flate2-rs/commit/d8e74e1c5d54c77624725b135c32df09cfcbf20a))
    - Merge branch 'main' into partial-filename-write ([`afa9c8f`](https://github.com/Byron/flate2-rs/commit/afa9c8fe017f235606cd57ac61c10604e9223fd0))
    - Minor improvements to the MultiGzDecoder documentation ([`7cfdd4e`](https://github.com/Byron/flate2-rs/commit/7cfdd4e93cfc42ec7c7ea6303087033746d26fde))
    - Merge pull request #326 from DavidKorczynski/cifuzz-int ([`307d84b`](https://github.com/Byron/flate2-rs/commit/307d84bd0826afbbe88c441b7b78ad6779fb79f7))
    - Change the fuzz-time to 3 minutes to avoid waiting for fuzzing. ([`51ab99a`](https://github.com/Byron/flate2-rs/commit/51ab99ac30dc86cda6a62cbcc3d593b0db7a2d00))
    - Merge pull request #356 from markgoddard/issues/355 ([`6b52d0e`](https://github.com/Byron/flate2-rs/commit/6b52d0e63c94830583fbd8916f97b033361002fe))
    - Add MAINTENANCE.md ([`b58db7f`](https://github.com/Byron/flate2-rs/commit/b58db7f3c03167eff55bc38d8ce628cec65b0057))
    - Simplify doc-tests ([`08f7d73`](https://github.com/Byron/flate2-rs/commit/08f7d7391ba291c7106372adba258b7f6731d2b1))
    - Unify documentation style of newly added functions. ([`acd2ab9`](https://github.com/Byron/flate2-rs/commit/acd2ab9c9000ef16fdaafbaeb338b66b8263222a))
    - Forgot to add grave accent's ([`1e389c5`](https://github.com/Byron/flate2-rs/commit/1e389c54f9ad53ac73df1fa207049d3aef23d0f4))
    - Add functions that allow (de)compress instances ([`2754030`](https://github.com/Byron/flate2-rs/commit/27540301ca1035070bfb8f4d8ab32f06bd5ae2a8))
    - Recommend MultiGzDecoder over GzDecoder in docs ([`7d5856d`](https://github.com/Byron/flate2-rs/commit/7d5856d0bb724eb77a558c89a5bae878e1d8dc3c))
    - Merge pull request #360 from Byron/no-default-features ([`74870ae`](https://github.com/Byron/flate2-rs/commit/74870aebd5952c92e1c290de6e870bf2fa91d7f7))
    - Fix Read encoder examples ([`3281693`](https://github.com/Byron/flate2-rs/commit/3281693768a6e30587d822de49a68392b212b10d))
    - Merge pull request #347 from JohnTitor/note-to-multi-gz-decoder ([`f537522`](https://github.com/Byron/flate2-rs/commit/f537522b6812ebe54686b1d8d98571092d6ae07c))
    - Fix a comment on the `Compression` struct ([`aedc7a6`](https://github.com/Byron/flate2-rs/commit/aedc7a692315c9f173d81bb90ce9be666c009149))
    - Fix GzDecoder Write partial filenames and comments ([`3ea8c3d`](https://github.com/Byron/flate2-rs/commit/3ea8c3dcdb3937fb6102c16627c52ce74ac63d13))
    - Add notes about multiple streams to `GzDecoder` ([`6e111fe`](https://github.com/Byron/flate2-rs/commit/6e111fe8643321a7b00bd96e6e385521e8431e11))
    - Merge pull request #346 from jongiddy/move-gzip-parsing ([`fe15e4d`](https://github.com/Byron/flate2-rs/commit/fe15e4da08b3433f427cd1d41c6a5ed80a621651))
    - Merge pull request #345 from jongiddy/move-read-test ([`5d2851e`](https://github.com/Byron/flate2-rs/commit/5d2851e405db6e8f08cc00d14a5521c65f238084))
    - Merge pull request #344 from jongiddy/header-in-gzstate ([`a9b5fc4`](https://github.com/Byron/flate2-rs/commit/a9b5fc46ce09ff94f9eb5fb3ebd29f2b21baa16b))
    - Move gzip header parsing out of bufread module ([`a5e2eba`](https://github.com/Byron/flate2-rs/commit/a5e2ebaac545df3b0b102ffa474306b6077c72a3))
    - Move blocked_partial_header_read test to read module ([`4a622d9`](https://github.com/Byron/flate2-rs/commit/4a622d9798357af9cbdbbb096b16953cdda6a0c1))
    - Move GzHeader into GzState ([`ecb6838`](https://github.com/Byron/flate2-rs/commit/ecb6838f4a11ca9926a40a954e7ebbc745004807))
</details>

## v1.0.26 (2023-04-28)

<csr-id-8c5ce474a691eb02dacacf0b8235d5af25fd84c9/>
<csr-id-0f55af19052275f142fd0886ff595a18086cce09/>

### Other

 - <csr-id-8c5ce474a691eb02dacacf0b8235d5af25fd84c9/> Upgrade to windows-2022
 - <csr-id-0f55af19052275f142fd0886ff595a18086cce09/> Specify tag instead of branch on actions/checkout

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 23 commits contributed to the release over the course of 126 calendar days.
 - 154 days passed between releases.
 - 2 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 2 unique issues were worked on: [#285](https://github.com/Byron/flate2-rs/issues/285), [#335](https://github.com/Byron/flate2-rs/issues/335)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#285](https://github.com/Byron/flate2-rs/issues/285)**
    - Make clippy happy + a few more cleanups ([`76327b1`](https://github.com/Byron/flate2-rs/commit/76327b14b08ecb616b023d6a6713cc37e9004f99))
 * **[#335](https://github.com/Byron/flate2-rs/issues/335)**
    - Bump miniz-oxide to prevent assertion failure ([`a9000e1`](https://github.com/Byron/flate2-rs/commit/a9000e119824eaa3447555d73cca9a459f516837))
 * **Uncategorized**
    - Merge pull request #341 from JohnTitor/release-1.0.26 ([`5bedbab`](https://github.com/Byron/flate2-rs/commit/5bedbab1d25c9a0c7adf332fb7c42d3e10e0c979))
    - Merge pull request #342 from JohnTitor/gha-checkout ([`13b4bc0`](https://github.com/Byron/flate2-rs/commit/13b4bc08e6972aa43103cdaab78eef2747765192))
    - Merge pull request #343 from JohnTitor/windows-2022 ([`914559b`](https://github.com/Byron/flate2-rs/commit/914559bd61dd41fe5a42abfac1d715515b0f0365))
    - Merge pull request #325 from jongiddy/write-multigzdecoder ([`70944ea`](https://github.com/Byron/flate2-rs/commit/70944ead31984af1beb1b06a72d8b339e0c0a93b))
    - Merge pull request #322 from passware/features/zlib-default ([`a9100e5`](https://github.com/Byron/flate2-rs/commit/a9100e52011386a41c75ddf55d6eb786f79f66dd))
    - Upgrade to windows-2022 ([`8c5ce47`](https://github.com/Byron/flate2-rs/commit/8c5ce474a691eb02dacacf0b8235d5af25fd84c9))
    - Specify tag instead of branch on actions/checkout ([`0f55af1`](https://github.com/Byron/flate2-rs/commit/0f55af19052275f142fd0886ff595a18086cce09))
    - Prepare 1.0.26 release ([`e219320`](https://github.com/Byron/flate2-rs/commit/e219320433bb65ccc4305d4d2c38c707343b7843))
    - Merge pull request #330 from AntonJMLarsson/main ([`4f33b5a`](https://github.com/Byron/flate2-rs/commit/4f33b5abcf96b0dafa47cbf7a942d231ffc3a7c4))
    - Merge pull request #337 from yestyle/main ([`d9b2394`](https://github.com/Byron/flate2-rs/commit/d9b23944d3324451536143801cec72a2002331cf))
    - Fix a typo in doc for write::GzDecoder ([`146b12c`](https://github.com/Byron/flate2-rs/commit/146b12cb9720e336ffaadad82f2b77020c1ab11a))
    - Merge pull request #336 from wcampbell0x2a/add-docs-cfg-all-features ([`e094dad`](https://github.com/Byron/flate2-rs/commit/e094dad0346a25739e85b93baeec0a164a37f3c4))
    - Enable all-features, Use doc_auto_cfg on docs.rs ([`f862ed1`](https://github.com/Byron/flate2-rs/commit/f862ed10417507a8bbcbea761d130aca6343b852))
    - Merge pull request #332 from JohnTitor/msrv-policy ([`6d8a1fd`](https://github.com/Byron/flate2-rs/commit/6d8a1fdaf3bd5a5e272d4c2825b48884bd5c72ad))
    - Merge pull request #333 from JohnTitor/fix-docs-get-mut ([`431dc85`](https://github.com/Byron/flate2-rs/commit/431dc85a45dcb764ed02e38fb23af4093bf53913))
    - Fix left-overs on decoder docs ([`cdbbf7e`](https://github.com/Byron/flate2-rs/commit/cdbbf7ea1267158b3be0f0a306d1a0472ae130b8))
    - Mention MSRV policy ([`73012b6`](https://github.com/Byron/flate2-rs/commit/73012b6794ce6f041126904e6ad346266243ab4e))
    - Merge pull request #331 from JohnTitor/good-by-extern-crates ([`657cc9c`](https://github.com/Byron/flate2-rs/commit/657cc9c15a0e86a2fbfa06ed791c5c41f01d794d))
    - Remove `extern crate`s ([`d102310`](https://github.com/Byron/flate2-rs/commit/d10231062103ed40d3e03a3eb4da14ee05922a33))
    - Merge pull request #329 from MichaelMcDonnell/decompress_file ([`1c0d5c8`](https://github.com/Byron/flate2-rs/commit/1c0d5c80af6d0766360b76fa274117802c461912))
    - Overflow bug in crc combine ([`ab188ff`](https://github.com/Byron/flate2-rs/commit/ab188ff81409c074ad1a0000b434d2aa08dea10a))
</details>

## v1.0.25 (2022-11-24)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 12 commits contributed to the release over the course of 179 calendar days.
 - 179 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#317](https://github.com/Byron/flate2-rs/issues/317)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#317](https://github.com/Byron/flate2-rs/issues/317)**
    - Bump miniz_oxide to 0.6 ([`cc5ed7f`](https://github.com/Byron/flate2-rs/commit/cc5ed7f817cc5e5712b2bb924ed19cab4f389a47))
 * **Uncategorized**
    - Merge pull request #327 from thomcc/prep-1.0.25 ([`8431d9e`](https://github.com/Byron/flate2-rs/commit/8431d9e0c0fdaea16c4643c723631223802b2c86))
    - Prep release 1.0.25 ([`7d1399c`](https://github.com/Byron/flate2-rs/commit/7d1399c79a4ada11fd6df54d18397d9246fe6488))
    - Add CIFuzz Github action ([`696eb15`](https://github.com/Byron/flate2-rs/commit/696eb15fe2c2be6004c6bd6b88e008807b1643e5))
    - Add write::MultiGzDecoder for multi-member gzip data ([`ab891f1`](https://github.com/Byron/flate2-rs/commit/ab891f17cfabd07fba7d0749dbab8addc2bb3984))
    - Added feature for enabling default zlib-sys features ([`6c86c6c`](https://github.com/Byron/flate2-rs/commit/6c86c6cca249a3f19c993a11583f3fc0c4b6c74b))
    - Add decompress file example ([`57b2d33`](https://github.com/Byron/flate2-rs/commit/57b2d33f9cd7ce68d16ae42ae0bf51440339324e))
    - Merge pull request #296 from atouchet/lic ([`37252dd`](https://github.com/Byron/flate2-rs/commit/37252dd33556edd52eebf6c07c194941e7a4ca4c))
    - Fix link to Rust COPYRIGHT file ([`c4d7ed7`](https://github.com/Byron/flate2-rs/commit/c4d7ed7c5537327769994c54a3c517ca597c5ad3))
    - Use SPDX license format and update links ([`6a8352e`](https://github.com/Byron/flate2-rs/commit/6a8352edb05d095ea67c4f715b553a03f3ada131))
    - Remove unneeded libc dependency in favor of std ([`a4628b7`](https://github.com/Byron/flate2-rs/commit/a4628b7955a5564e1fb8ce37ed18affb09f42b18))
    - Stop re-exporting libc types from `std::ffi::c` ([`d11e290`](https://github.com/Byron/flate2-rs/commit/d11e29005498dc7d85f6e980834b37bc5bbcb12e))
</details>

## v1.0.24 (2022-05-28)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 9 commits contributed to the release.
 - 47 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Merge pull request #302 from joshtriplett/libz-ng-sys ([`6daf762`](https://github.com/Byron/flate2-rs/commit/6daf762f36be1d8bf7646ff75e59547c54b1f816))
    - Add support for native zlib-ng via libz-ng-sys ([`0bf9491`](https://github.com/Byron/flate2-rs/commit/0bf9491b8c0d809d5e8522bce9c98845b2d59f09))
    - Merge pull request #303 from joshtriplett/remove-miniz-sys ([`40b126e`](https://github.com/Byron/flate2-rs/commit/40b126efe0e5423edeae60bb6d666b7408f82e8e))
    - Merge pull request #304 from joshtriplett/drop-publish-docs ([`ba39d44`](https://github.com/Byron/flate2-rs/commit/ba39d44ff4632a6f04259ad7f1206d38505418c2))
    - Eliminate the use of cfg-if, now that the conditionals are simpler ([`9067d1e`](https://github.com/Byron/flate2-rs/commit/9067d1e27c3a7d2dca2c62bdb0051a8d1b08aa52))
    - Remove miniz-sys, and map it to miniz_oxide for compatibility ([`97f0a1c`](https://github.com/Byron/flate2-rs/commit/97f0a1c5770fb6cb1ee6fa5291dc54e21f6db8bb))
    - Drop CI step to publish documentation; docs live on docs.rs now ([`3bf8c04`](https://github.com/Byron/flate2-rs/commit/3bf8c0408169fc193b32f4dbcd4ed9aaef21ff71))
    - Merge pull request #305 from joshtriplett/fix-ci ([`7ba55eb`](https://github.com/Byron/flate2-rs/commit/7ba55eb28cb1cc99d459b5664fc14e19c48949c8))
    - Fix CI by disabling zlib-ng-compat on mingw ([`0dd8cb6`](https://github.com/Byron/flate2-rs/commit/0dd8cb6e5f46753222a1f5011fcd4fc9f1c928b5))
</details>

## v1.0.23 (2022-04-11)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 6 commits contributed to the release over the course of 119 calendar days.
 - 210 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 5 unique issues were worked on: [#282](https://github.com/Byron/flate2-rs/issues/282), [#283](https://github.com/Byron/flate2-rs/issues/283), [#284](https://github.com/Byron/flate2-rs/issues/284), [#292](https://github.com/Byron/flate2-rs/issues/292), [#293](https://github.com/Byron/flate2-rs/issues/293)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#282](https://github.com/Byron/flate2-rs/issues/282)**
    - Minor examlpe tweaks ([`a5a38c5`](https://github.com/Byron/flate2-rs/commit/a5a38c553838f240400546ba3f937966ba54f7e2))
 * **[#283](https://github.com/Byron/flate2-rs/issues/283)**
    - Make Clippy happier, minor cleanups ([`0fa0a7b`](https://github.com/Byron/flate2-rs/commit/0fa0a7b40b8e022f47b59453adda938e5156ac0f))
 * **[#284](https://github.com/Byron/flate2-rs/issues/284)**
    - Spellchecking comments, rm trailing spaces ([`b2e976d`](https://github.com/Byron/flate2-rs/commit/b2e976da21c18c8f31132e93a7f803b5e32f2b6d))
 * **[#292](https://github.com/Byron/flate2-rs/issues/292)**
    - Remove `tokio` support ([`4e5e0c5`](https://github.com/Byron/flate2-rs/commit/4e5e0c51f07b293a26a4b3d10ebeaa40b6c9149a))
 * **[#293](https://github.com/Byron/flate2-rs/issues/293)**
    - Update to miniz_oxide 0.5 / quickcheck 1.0 / rand 0.8 ([`0d22be4`](https://github.com/Byron/flate2-rs/commit/0d22be4a7454b4cef8813de513b93e87fed087b3))
 * **Uncategorized**
    - Bump to 1.0.23 ([`3b2c3a1`](https://github.com/Byron/flate2-rs/commit/3b2c3a12c5beed91f8952e6e925ca7839218976d))
</details>

## v1.0.22 (2021-09-13)

<csr-id-afd8dd45fa669ad0664ae2971f657c23752d2cb9/>

### Other

 - <csr-id-afd8dd45fa669ad0664ae2971f657c23752d2cb9/> fix endless loop in read implementation
   Introduced by commit 7212da84fa0a9fe77c240e95a675f0567e9d698c
   Adds a test to prevent this behavior

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 14 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#280](https://github.com/Byron/flate2-rs/issues/280)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#280](https://github.com/Byron/flate2-rs/issues/280)**
    - Fix endless loop in read implementation ([`afd8dd4`](https://github.com/Byron/flate2-rs/commit/afd8dd45fa669ad0664ae2971f657c23752d2cb9))
 * **Uncategorized**
    - Bump to 1.0.22 ([`63ecb8c`](https://github.com/Byron/flate2-rs/commit/63ecb8c0407c619c7a20529699b89369061ece88))
</details>

## v1.0.21 (2021-08-30)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release over the course of 137 calendar days.
 - 212 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 5 unique issues were worked on: [#266](https://github.com/Byron/flate2-rs/issues/266), [#267](https://github.com/Byron/flate2-rs/issues/267), [#270](https://github.com/Byron/flate2-rs/issues/270), [#277](https://github.com/Byron/flate2-rs/issues/277), [#278](https://github.com/Byron/flate2-rs/issues/278)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#266](https://github.com/Byron/flate2-rs/issues/266)**
    - Update cloudflare-zlib ([`f9e26de`](https://github.com/Byron/flate2-rs/commit/f9e26de64fa10160c38933223814f05bed3bf20a))
 * **[#267](https://github.com/Byron/flate2-rs/issues/267)**
    - Initial oss-fuzz integration. ([`7546110`](https://github.com/Byron/flate2-rs/commit/7546110602fcc934ae506ed8d5cd9516e945d1ee))
 * **[#270](https://github.com/Byron/flate2-rs/issues/270)**
    - Use the error message from z_stream.msg for [De]CompressError ([`c378248`](https://github.com/Byron/flate2-rs/commit/c37824894daacc0ad7bbca566c48a897cf973c4f))
 * **[#277](https://github.com/Byron/flate2-rs/issues/277)**
    - Renamed corrupt test gzip file ([`33f9f3d`](https://github.com/Byron/flate2-rs/commit/33f9f3d028848760207bb3f6618669bf5ef02c3d))
 * **[#278](https://github.com/Byron/flate2-rs/issues/278)**
    - Avoid quadratic complexity in GzDecoder ([`7212da8`](https://github.com/Byron/flate2-rs/commit/7212da84fa0a9fe77c240e95a675f0567e9d698c))
 * **Uncategorized**
    - Bump to 1.0.21 ([`d0f0146`](https://github.com/Byron/flate2-rs/commit/d0f01469a7fd70e87049be389b8110a2f01bbd05))
    - Fix some examples to use `read_to_end` ([`19708cf`](https://github.com/Byron/flate2-rs/commit/19708cfa6808298eaa74e4c5b22ba666f8b92a37))
</details>

## v1.0.20 (2021-01-29)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release over the course of 3 calendar days.
 - 88 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#261](https://github.com/Byron/flate2-rs/issues/261)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#261](https://github.com/Byron/flate2-rs/issues/261)**
    - Add [De]Compress::new_gzip ([`d70165c`](https://github.com/Byron/flate2-rs/commit/d70165c6620311153f9c364a8df43ee4d2e2bdf9))
 * **Uncategorized**
    - Bump to 1.0.20 ([`90d9e5e`](https://github.com/Byron/flate2-rs/commit/90d9e5ed866742ce8b3946d156830e300d1e5aab))
</details>

## v1.0.19 (2020-11-02)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 5 commits contributed to the release over the course of 32 calendar days.
 - 32 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 2 unique issues were worked on: [#252](https://github.com/Byron/flate2-rs/issues/252), [#253](https://github.com/Byron/flate2-rs/issues/253)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#252](https://github.com/Byron/flate2-rs/issues/252)**
    - Make `Compression::{new, none, fast, best}` const fn ([`0a631bb`](https://github.com/Byron/flate2-rs/commit/0a631bbd19873c50ab87a5d120dcec28725631a9))
 * **[#253](https://github.com/Byron/flate2-rs/issues/253)**
    - Update to cfg-if 1.0 ([`31fb078`](https://github.com/Byron/flate2-rs/commit/31fb07820345691352aaa64f367c1e482ad9cfdc))
 * **Uncategorized**
    - Bump to 1.0.19 ([`04dd8b6`](https://github.com/Byron/flate2-rs/commit/04dd8b6de301468535ec8fa4f1ffd722d750ab50))
    - Clarify performance in README ([`301e53c`](https://github.com/Byron/flate2-rs/commit/301e53cc8c08bd1fa34ae7814108ec5e36583416))
    - Fix documentation for the bufread types ([`2a6dc3b`](https://github.com/Byron/flate2-rs/commit/2a6dc3b66fe4313da1a58ebf6b4442998a39b841))
</details>

## v1.0.18 (2020-09-30)

<csr-id-d18b2782604e0ac776c2f665be3f6cfa412e6700/>
<csr-id-b4b20b94bf0b671d22682fd7ea2751b370113fab/>

### Other

 - <csr-id-d18b2782604e0ac776c2f665be3f6cfa412e6700/> Add myself as a maintainer
 - <csr-id-b4b20b94bf0b671d22682fd7ea2751b370113fab/> Update version numbers in sample dependency lines

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release over the course of 41 calendar days.
 - 42 days passed between releases.
 - 2 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#249](https://github.com/Byron/flate2-rs/issues/249)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#249](https://github.com/Byron/flate2-rs/issues/249)**
    - Update version numbers in sample dependency lines ([`b4b20b9`](https://github.com/Byron/flate2-rs/commit/b4b20b94bf0b671d22682fd7ea2751b370113fab))
 * **Uncategorized**
    - Bump version ([`026562b`](https://github.com/Byron/flate2-rs/commit/026562b149a94bb20bc0abf4484e411952e42809))
    - Update repository and homepage URLs ([`8bcabf6`](https://github.com/Byron/flate2-rs/commit/8bcabf678c973eccf1792b43b866040e5cefc196))
    - Add the keyword "zlib-ng" so people looking for it will find this crate ([`89f2bfe`](https://github.com/Byron/flate2-rs/commit/89f2bfe19212b7c8e26c0737bf601fb11bac28cb))
    - Update keywords to mention "deflate" ([`b64f8c0`](https://github.com/Byron/flate2-rs/commit/b64f8c0ebd334427662b2ea55ceb6f1243274b2a))
    - Remove badges; crates.io no longer supports them ([`566c5f7`](https://github.com/Byron/flate2-rs/commit/566c5f7ece02aadf7f5fd302d28355afa1b379a3))
    - Add myself as a maintainer ([`d18b278`](https://github.com/Byron/flate2-rs/commit/d18b2782604e0ac776c2f665be3f6cfa412e6700))
</details>

## v1.0.17 (2020-08-18)

<csr-id-6f91706d1da64ad590a03eb94a0919af60802215/>

### Other

 - <csr-id-6f91706d1da64ad590a03eb94a0919af60802215/> rewrite description, no longer "bindings to miniz.c"
   flate2 does not just provide "bindings to miniz.c", and in fact doesn't
   use miniz.c by default. Rewrite the description for clarity.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 3 commits contributed to the release over the course of 1 calendar day.
 - 50 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 2 unique issues were worked on: [#247](https://github.com/Byron/flate2-rs/issues/247), [#248](https://github.com/Byron/flate2-rs/issues/248)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#247](https://github.com/Byron/flate2-rs/issues/247)**
    - Rewrite description, no longer "bindings to miniz.c" ([`6f91706`](https://github.com/Byron/flate2-rs/commit/6f91706d1da64ad590a03eb94a0919af60802215))
 * **[#248](https://github.com/Byron/flate2-rs/issues/248)**
    - Update to libz-sys 1.1.0, and support use with zlib-ng ([`bdf1c12`](https://github.com/Byron/flate2-rs/commit/bdf1c121cb6148ead2085c95b7e76a21fb9c2aa7))
 * **Uncategorized**
    - Bump to 1.0.17 ([`c70ba6b`](https://github.com/Byron/flate2-rs/commit/c70ba6b086843e9873e6c298c0c6e9c22fa1fa86))
</details>

## v1.0.16 (2020-06-29)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 5 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.16 ([`9e22186`](https://github.com/Byron/flate2-rs/commit/9e2218692d168fa9056a028e1670117bf23a13fa))
    - Bump miniz-oxide dependency ([`101f431`](https://github.com/Byron/flate2-rs/commit/101f4316af76932611d60df54f8a1347c7ff7035))
</details>

## v1.0.15 (2020-06-23)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 3 commits contributed to the release.
 - 98 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#242](https://github.com/Byron/flate2-rs/issues/242)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#242](https://github.com/Byron/flate2-rs/issues/242)**
    - Reset mem_level to DEF_MEM_LEVEL. ([`eb58d68`](https://github.com/Byron/flate2-rs/commit/eb58d68abc6b71c9d378873a3034e5bf0ecfff69))
 * **Uncategorized**
    - Bump to 1.0.15 ([`9b0fc13`](https://github.com/Byron/flate2-rs/commit/9b0fc13aef4d47c9bd0ba8729526c42ac4d3ec09))
    - Fix `systest` build ([`a448e54`](https://github.com/Byron/flate2-rs/commit/a448e540d1b35c7548f7c8e28ed26335b77c52a9))
</details>

## v1.0.14 (2020-03-17)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 8 commits contributed to the release over the course of 126 calendar days.
 - 126 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 4 unique issues were worked on: [#227](https://github.com/Byron/flate2-rs/issues/227), [#228](https://github.com/Byron/flate2-rs/issues/228), [#230](https://github.com/Byron/flate2-rs/issues/230), [#231](https://github.com/Byron/flate2-rs/issues/231)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#227](https://github.com/Byron/flate2-rs/issues/227)**
    - Remove deprecated Error::description ([`f367ab0`](https://github.com/Byron/flate2-rs/commit/f367ab0c592757362fc607264412f4d9bc42f035))
 * **[#228](https://github.com/Byron/flate2-rs/issues/228)**
    - Support cloudfire optimized version of zlib as a backend ([`120ba81`](https://github.com/Byron/flate2-rs/commit/120ba81a6f9577a1ae2ee09179743bf1177597cd))
 * **[#230](https://github.com/Byron/flate2-rs/issues/230)**
    - Expose zlib options on cloudflare-zlib ([`5ef8702`](https://github.com/Byron/flate2-rs/commit/5ef87027cf9a9a6c876886279f74215c7965a902))
 * **[#231](https://github.com/Byron/flate2-rs/issues/231)**
    - Update comment on cfg_if! ([`42ef27a`](https://github.com/Byron/flate2-rs/commit/42ef27aa8382dd3cfda69ac6f00512deba363112))
 * **Uncategorized**
    - Bump to 1.0.14 ([`962930c`](https://github.com/Byron/flate2-rs/commit/962930cb3a1b1c50b710c9f4749f1686365eb70a))
    - Remove wasm-specific case for backend ([`9feca9d`](https://github.com/Byron/flate2-rs/commit/9feca9d4b04d8e4a094c5452039015df65c8321a))
    - Update rust installation on osx ([`4d62a89`](https://github.com/Byron/flate2-rs/commit/4d62a8936981a74a377459155e6d41333c66cf62))
    - Run rustfmt ([`c26967c`](https://github.com/Byron/flate2-rs/commit/c26967c44ffe75116bd985876527feb1fcc494ad))
</details>

## v1.0.13 (2019-11-11)

<csr-id-08279448248c4b5b47161ac0348ddd6514906713/>

### Other

 - <csr-id-08279448248c4b5b47161ac0348ddd6514906713/> update minimum versions
   crc32fast has dependency specifications which are compatible and cfg-if
   0.1.6 is the first to include APIs used in the ffi module.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 5 commits contributed to the release over the course of 20 calendar days.
 - 40 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 2 unique issues were worked on: [#218](https://github.com/Byron/flate2-rs/issues/218), [#221](https://github.com/Byron/flate2-rs/issues/221)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#218](https://github.com/Byron/flate2-rs/issues/218)**
    - Typo fix: DEFALTE vs DEFLATE ([`7e68e58`](https://github.com/Byron/flate2-rs/commit/7e68e58cb32c9ce4ffdb5031c76a34b984d77a61))
 * **[#221](https://github.com/Byron/flate2-rs/issues/221)**
    - Update minimum versions ([`0827944`](https://github.com/Byron/flate2-rs/commit/08279448248c4b5b47161ac0348ddd6514906713))
 * **Uncategorized**
    - Bump to 1.0.13 ([`c9b256a`](https://github.com/Byron/flate2-rs/commit/c9b256a624d04353bc6928dacf9d0c99aa61c261))
    - Fix Github Actions for recent system changes ([`53e45a6`](https://github.com/Byron/flate2-rs/commit/53e45a68bf47004494ca5c8cf06d197e99aa713e))
    - Fix Windows CI ([`400e1e4`](https://github.com/Byron/flate2-rs/commit/400e1e4a72e53935297fdd5305031f4715f96cf0))
</details>

## v1.0.12 (2019-10-02)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 13 commits contributed to the release over the course of 47 calendar days.
 - 48 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.12 ([`70425f8`](https://github.com/Byron/flate2-rs/commit/70425f82ecf82d26f0de0809e84b44c00c461d5e))
    - Merge pull request #216 from alexcrichton/rust-default ([`5aa2c7b`](https://github.com/Byron/flate2-rs/commit/5aa2c7bc7cfaeee88da95d3330fdce0be61300d0))
    - Run rustfmt ([`6305575`](https://github.com/Byron/flate2-rs/commit/63055751bf614560cfddb3fc9b1fe2ec773e2768))
    - Fix rustdoc invocation ([`518b598`](https://github.com/Byron/flate2-rs/commit/518b59841d49e5ffcd5aba772c1663c40b1749f6))
    - Reorganize backend configuration ([`db85870`](https://github.com/Byron/flate2-rs/commit/db85870d3011a5b0821ebbc9aade43cab5fbd325))
    - Switch to the Rust backend by default ([`c479d06`](https://github.com/Byron/flate2-rs/commit/c479d064e24c8184a5f552adf0ffac7e1acc9e4e))
    - Run rustfmt ([`5751ad9`](https://github.com/Byron/flate2-rs/commit/5751ad961c7bf9940f488d4b25ef7c7ee1ce3385))
    - Remove no longer needed `extern crate` declarations ([`cbcfe15`](https://github.com/Byron/flate2-rs/commit/cbcfe155750b4afcf3a170f0dbbb84fa9c067c81))
    - Merge pull request #213 from fisherdarling/2018edition ([`69ff34a`](https://github.com/Byron/flate2-rs/commit/69ff34a476d6c1f7f74ff1f91608766e2d7cfa51))
    - Upgrade to 2018 edition ([`57972d7`](https://github.com/Byron/flate2-rs/commit/57972d77dab09acad4aa2fa3beedb1f69fa64b27))
    - Update quickcheck ([`537fb77`](https://github.com/Byron/flate2-rs/commit/537fb77132a15b772fcc9c35a4c8c679d40aedf7))
    - Remove last uses of `try!` ([`bf47471`](https://github.com/Byron/flate2-rs/commit/bf4747109a17b1461e237f2ee8fcb9ca9c00b3ea))
    - Switch CI to GitHub Actions ([`660035c`](https://github.com/Byron/flate2-rs/commit/660035c944d4368d56862f80cc31505d5748a6c2))
</details>

## v1.0.11 (2019-08-14)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 4 commits contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.11 ([`5d7cdc9`](https://github.com/Byron/flate2-rs/commit/5d7cdc9ce703a07659a168fbcdec5850cc159636))
    - Test miniz-sys feature on CI ([`ab41741`](https://github.com/Byron/flate2-rs/commit/ab41741e75eec97e1a8571ab98a7fe5efa24342b))
    - Merge pull request #206 from alexcrichton/fix-optional-build ([`1ba5fce`](https://github.com/Byron/flate2-rs/commit/1ba5fced38abd2f01675a12142865fafd7de3c42))
    - Move `libc` back to a non-optional dependency ([`b00caf4`](https://github.com/Byron/flate2-rs/commit/b00caf4c5c27e00a4c7d0386df60580bc4078428))
</details>

## v1.0.10 (2019-08-14)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 14 commits contributed to the release over the course of 54 calendar days.
 - 54 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.10 ([`6dd9602`](https://github.com/Byron/flate2-rs/commit/6dd96024b9092e3489adfcb8416d952da0aeed76))
    - Merge pull request #204 from alexcrichton/configure-zalloc ([`b6640f2`](https://github.com/Byron/flate2-rs/commit/b6640f2bfb723b00f3536a75239e350b6c0e7eea))
    - Merge pull request #199 from alexcrichton/dependabot/cargo/rand-0.7 ([`b14b216`](https://github.com/Byron/flate2-rs/commit/b14b216c0f7dc2479df358985272ac348dcfd80f))
    - Configure allocation/free functions in zlib/miniz ([`c0e3114`](https://github.com/Byron/flate2-rs/commit/c0e31143a233335d4b0dca86f4f93d69496df304))
    - Update rand requirement from 0.6 to 0.7 ([`a4630d6`](https://github.com/Byron/flate2-rs/commit/a4630d67853cfac9bbe14ce2b7196b9a68a94ccf))
    - Merge pull request #202 from oyvindln/rust_backend ([`0954103`](https://github.com/Byron/flate2-rs/commit/09541037e6d7a0c499ed3e93e18fcc4a0709c7b1))
    - Combine implementation for StreamWrapper, and a bit more cleanup Ignore warning so build works on nightly while bug in libz is not fixed ([`23dd2a0`](https://github.com/Byron/flate2-rs/commit/23dd2a09847c37a75f0445487ccac109a229324b))
    - Move the rest of the platform specific stuff other than the zlib things to ffi ([`2f5c651`](https://github.com/Byron/flate2-rs/commit/2f5c6510f3bef70a9b2db2587826a0ed992d860f))
    - Start preparing to move stuff to ffi ([`b34608c`](https://github.com/Byron/flate2-rs/commit/b34608c6a5ceb72de551f0934326fb499bc0431f))
    - Readd line that was needed for wasm32 ([`75f9d37`](https://github.com/Byron/flate2-rs/commit/75f9d37b4bf6a33c5eca57c415b9aeeb57f9da42))
    - Clean up the backend-specific code a little, remove libc entirely for rust backend ([`b561eb6`](https://github.com/Byron/flate2-rs/commit/b561eb6d2e6497d4ffa5eb45f89b8fb9dc67ac23))
    - Use miniz_oxide directly ([`344f58b`](https://github.com/Byron/flate2-rs/commit/344f58ba9a8066bbd4f2f321b16a89711b57d53e))
    - Run `cargo fmt` ([`dfb1082`](https://github.com/Byron/flate2-rs/commit/dfb1082207b60b6a0a6d66d1c76d73e54464cde1))
    - Merge pull request #182 from alexcrichton/dependabot/cargo/quickcheck-0.8 ([`18d077f`](https://github.com/Byron/flate2-rs/commit/18d077fbfe91142fff8ce41c3b441883ea094d66))
</details>

## v1.0.9 (2019-06-21)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 1 day passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.9 ([`c772bd2`](https://github.com/Byron/flate2-rs/commit/c772bd2ee2fe82a69cfa2b317bdc8f641c54a889))
    - Fix truncation when decompression to huge buffers ([`95ec404`](https://github.com/Byron/flate2-rs/commit/95ec404a2948925370201d0dee2c324845f3aeab))
</details>

## v1.0.8 (2019-06-19)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 10 commits contributed to the release over the course of 34 calendar days.
 - 96 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.8 ([`8b85ac1`](https://github.com/Byron/flate2-rs/commit/8b85ac1e08cff8e656f2b6f85ce65779763440ba))
    - Merge pull request #196 from twittner/builder ([`0f3921d`](https://github.com/Byron/flate2-rs/commit/0f3921d21881123cb49680e50c5d3df165a3e7ef))
    - Add a note about availability of new_with_window_bits. ([`070ebaa`](https://github.com/Byron/flate2-rs/commit/070ebaa4e7548b37ba673e108073585e6f47e03a))
    - Remove builder. ([`5653ccb`](https://github.com/Byron/flate2-rs/commit/5653ccbb435da899fbf03da622f0dd3cb68f129b))
    - Fix installing Rust on CI ([`22ae82b`](https://github.com/Byron/flate2-rs/commit/22ae82b94ae47977ef761ea895b7579f43141984))
    - Only enable `Builder::window_bits` for zlib feature. ([`84a62bb`](https://github.com/Byron/flate2-rs/commit/84a62bb6d657e4af8f6695b0fc5324ec89455b9a))
    - Add `Builder` to configure `Compress`/`Decompress`. ([`338d97a`](https://github.com/Byron/flate2-rs/commit/338d97a56bc56fd15cbf763c6bf138dc429ab187))
    - Bump miniz-sys to 0.1.12 ([`7d3cbe5`](https://github.com/Byron/flate2-rs/commit/7d3cbe57e28b3c34de5b397647b1d9cd139376af))
    - Merge pull request #194 from RReverser/wasi ([`cc85538`](https://github.com/Byron/flate2-rs/commit/cc855389c20d71a369bf652c2e77f72f7899e9e4))
    - Disable compilation of miniz-sys on WASI ([`6a6f440`](https://github.com/Byron/flate2-rs/commit/6a6f440ea237dfaa1fc6eee37c26ea266ffa11bc))
</details>

## v1.0.7 (2019-03-14)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 31 commits contributed to the release over the course of 86 calendar days.
 - 102 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.7 ([`85166b2`](https://github.com/Byron/flate2-rs/commit/85166b21ff6456382c5bdecab5dfc531d99fd354))
    - Merge pull request #189 from quininer/fix-old ([`d90189d`](https://github.com/Byron/flate2-rs/commit/d90189d81d50143a14fadbd51680c2003ecd5038))
    - Fix lifetime for rustc 1.30 ([`8c0c094`](https://github.com/Byron/flate2-rs/commit/8c0c0948408af88a34c1141b114524101a87b2e1))
    - Avoid Take/Cursor in favor of manual impl ([`68f285a`](https://github.com/Byron/flate2-rs/commit/68f285a57ab00aaa55a30517a5a681d1c49bf659))
    - Merge pull request #185 from quininer/async-gzip ([`433f50c`](https://github.com/Byron/flate2-rs/commit/433f50cd68cd847a3b4c4316fe3531b718dfddfa))
    - Remove Next enum ([`c5b2b88`](https://github.com/Byron/flate2-rs/commit/c5b2b88243ab9d4cd7b30c8c7ad3864b984343a9))
    - Small improve ([`38a6809`](https://github.com/Byron/flate2-rs/commit/38a6809ef81fae005c95ce30f36685ad4d9462f7))
    - Refactor pipelines configuration slightly ([`10e060b`](https://github.com/Byron/flate2-rs/commit/10e060bd3d65fbb611263d7d577afeaa90d1b23c))
    - Use buffer ([`b6eed5d`](https://github.com/Byron/flate2-rs/commit/b6eed5d09b84f188f619070b4481824e6840ec4a))
    - Tweak syntax for Windows ([`448f16d`](https://github.com/Byron/flate2-rs/commit/448f16dc2ae3746451645f20ae866174315dbb3e))
    - Merge pull request #187 from alexcrichton/azure-pipelines ([`9f7c681`](https://github.com/Byron/flate2-rs/commit/9f7c6818e342014e4d0e085621de329577f43dba))
    - Add a Windows builder ([`bf7f769`](https://github.com/Byron/flate2-rs/commit/bf7f769986f60e0c372e663eff1de76a65009fa2))
    - Update build badge ([`8e8761d`](https://github.com/Byron/flate2-rs/commit/8e8761db1db0ba3c98104cdf969527fdcd799330))
    - Merge pull request #186 from alexcrichton/azure-pipelines ([`bd35717`](https://github.com/Byron/flate2-rs/commit/bd357177f39e9b668f70b2d18e99ce8b06d1c9fd))
    - Test out azure ([`64a4815`](https://github.com/Byron/flate2-rs/commit/64a48154c726ef2a8d53c189e70b874f9a3a2dd6))
    - Fix gzheader crc ([`4a9a761`](https://github.com/Byron/flate2-rs/commit/4a9a7615e49855c9860f3fc119849e64ddca7b87))
    - Don't expose multi method ([`ecd46e1`](https://github.com/Byron/flate2-rs/commit/ecd46e13798b1e03ab0ed4fc7c6ceb6f33037861))
    - Impl AsyncRead for GzDecoder ([`5a31284`](https://github.com/Byron/flate2-rs/commit/5a312843030a273218b635b7d8208c3eec76244c))
    - Replace old read_gz_header ([`c96b7b7`](https://github.com/Byron/flate2-rs/commit/c96b7b7bd2944822cf00611f2a9593c74a65c4da))
    - Merge MultiGzDecoder ([`9dc0d9e`](https://github.com/Byron/flate2-rs/commit/9dc0d9eebfe6f6528de8cc940c1fa19d700da2dc))
    - Fix wouldblock ([`6512539`](https://github.com/Byron/flate2-rs/commit/65125391d66566c026b24591125f4e33113d8bf4))
    - Keep old behavior ([`844830c`](https://github.com/Byron/flate2-rs/commit/844830cc5c36312a1b41f8fe56f83cb416cd899c))
    - Add async reader test ([`f17ea5b`](https://github.com/Byron/flate2-rs/commit/f17ea5b0fac4cad3047abcaa9c23d1f12b865e2e))
    - Impl async gzheader parse ([`0981506`](https://github.com/Byron/flate2-rs/commit/098150678695259fd0c853300d341f2199561fc3))
    - Update quickcheck requirement from 0.7 to 0.8 ([`f1d4801`](https://github.com/Byron/flate2-rs/commit/f1d480104f39c6c39a690cea91f985e4bd55267a))
    - Merge pull request #183 from dodomorandi/new_tokio ([`d06d479`](https://github.com/Byron/flate2-rs/commit/d06d479deabeabb749fa5e18c1a4f0db1302b2f3))
    - Reverted version ([`77e0c41`](https://github.com/Byron/flate2-rs/commit/77e0c41bdb78a77aded305a670eed04a83087020))
    - Usage of tokio subcrates and a bit more ([`39c9704`](https://github.com/Byron/flate2-rs/commit/39c9704b8baaa3c5996402383d5c74ea33905ac1))
    - Updated to new tokio API ([`8b8aa4f`](https://github.com/Byron/flate2-rs/commit/8b8aa4fd37fa756c92c31fc98a240241240b31be))
    - Tweak travis config ([`ab3d280`](https://github.com/Byron/flate2-rs/commit/ab3d280eca79d65b7445147e1e000a8d10434630))
    - Run `cargo fmt` ([`4569bd8`](https://github.com/Byron/flate2-rs/commit/4569bd8fd1bc23133de02dd8e89a64a4f4daeb53))
</details>

## v1.0.6 (2018-12-02)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 6 commits contributed to the release over the course of 12 calendar days.
 - 13 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.6 ([`9d8ccf1`](https://github.com/Byron/flate2-rs/commit/9d8ccf1b4fe6285115940281184a33460a2229ad))
    - Merge pull request #177 from srijs/crc32fast ([`c745576`](https://github.com/Byron/flate2-rs/commit/c74557615f064535edecc8e87952d88c67ab759a))
    - Replace flate2-crc with crc32fast ([`6c3a5b3`](https://github.com/Byron/flate2-rs/commit/6c3a5b386512ab0df4a8d1522938c63771e1b39c))
    - Bump to 0.1.1 ([`e5b7198`](https://github.com/Byron/flate2-rs/commit/e5b719862e56c693671e5d3782afa075b27c1eec))
    - Merge pull request #174 from erickt/master ([`c3dee69`](https://github.com/Byron/flate2-rs/commit/c3dee69415d5ba543fb2b2ee7369eac2781ee31e))
    - Add license symlinks to flate2-crc ([`f598b33`](https://github.com/Byron/flate2-rs/commit/f598b3304ff0c118a47d69b1e5d38b7f2d0f1014))
</details>

## v1.0.5 (2018-11-19)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 6 commits contributed to the release over the course of 33 calendar days.
 - 33 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.5 ([`32c5e91`](https://github.com/Byron/flate2-rs/commit/32c5e916c45f235a50c93578ce7d5a0f7645575c))
    - Add a version to miniz-sys dep ([`8a422c9`](https://github.com/Byron/flate2-rs/commit/8a422c90be29e6e80b3f9f829be2af76f6051429))
    - Merge pull request #172 from alexcrichton/simd-fast-path ([`5b5a529`](https://github.com/Byron/flate2-rs/commit/5b5a5292fb544e35b1023992aa2a4d780ba6c0ea))
    - Implement a SIMD fast path for CRC checksums ([`9b44592`](https://github.com/Byron/flate2-rs/commit/9b4459213c7ce5432d53be92cd2cf56d1221740e))
    - Upgrade to rand 0.6 ([`37a60a7`](https://github.com/Byron/flate2-rs/commit/37a60a727230565cbe791da7c7ceaf4e95ec7208))
    - Bump miniz-sys to 0.1.11 ([`e4c531f`](https://github.com/Byron/flate2-rs/commit/e4c531f472805996a77ecdce0b83253a6078623b))
</details>

## v1.0.4 (2018-10-16)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release.
 - 12 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.4 ([`cfcee6f`](https://github.com/Byron/flate2-rs/commit/cfcee6fac57763e125ecca02e66c72fd0d39d26c))
    - Update with a comment about libc types on wasm ([`23d36af`](https://github.com/Byron/flate2-rs/commit/23d36af30879977bde79af47771e99f7420da49d))
    - Make all wasm32-related cfg invocations aware of emscripten ([`95648ff`](https://github.com/Byron/flate2-rs/commit/95648ffe8e6427cafd1a21f1b2954e023a05887c))
    - Build wasm on travis ([`4355637`](https://github.com/Byron/flate2-rs/commit/4355637996f22454829c2b5c90250449a47b7580))
    - Use `[patch]` to get wasm working temporarily ([`930dacf`](https://github.com/Byron/flate2-rs/commit/930dacf7c24004b2e26c3f79fd90ca903f94b88b))
    - Disable compilation of miniz-sys on wasm ([`92c650c`](https://github.com/Byron/flate2-rs/commit/92c650ce1bea181ea4185f228bfc930e74cf2b68))
    - Initial wasm support ([`a3863ce`](https://github.com/Byron/flate2-rs/commit/a3863ce7c59fc98e3079dc4760b1819425d37bc8))
</details>

## v1.0.3 (2018-10-04)

<csr-id-37fab358591d97e93783c430bc456b6f4235da1a/>
<csr-id-76c4231b3352e9d21de6bbac1ac691601b503f20/>

### Other

 - <csr-id-37fab358591d97e93783c430bc456b6f4235da1a/> :GzDecoder does not support tokio
 - <csr-id-76c4231b3352e9d21de6bbac1ac691601b503f20/> update rand to 0.4, quickcheck to 0.6

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 60 commits contributed to the release over the course of 294 calendar days.
 - 308 days passed between releases.
 - 2 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.3 ([`711ba5c`](https://github.com/Byron/flate2-rs/commit/711ba5ce65791f82de7971154fc48e7161093140))
    - Bump dep on miniz-sys ([`8f45fac`](https://github.com/Byron/flate2-rs/commit/8f45fac59894e75fd8b403bf8df89e1055a7a1ef))
    - Switch to `write_all` in gzdecoder-write.rs ([`a8c59dc`](https://github.com/Byron/flate2-rs/commit/a8c59dc4e91709599e42169b619eb36a22da478d))
    - Merge pull request #167 from alexcrichton/dependabot/cargo/quickcheck-0.7 ([`37c7d4b`](https://github.com/Byron/flate2-rs/commit/37c7d4ba701f72a0f638c4f03ba9c68e4e871bde))
    - Update quickcheck requirement from 0.6 to 0.7 ([`7cb9cbe`](https://github.com/Byron/flate2-rs/commit/7cb9cbe07ae77c51f9c30b3e553769ededba6b32))
    - Update `ctest` dependency ([`3d5a5dd`](https://github.com/Byron/flate2-rs/commit/3d5a5ddbc7f3bba3c16095f09d4eca08864ed9a0))
    - Bump to 1.0.2 ([`89ae2a1`](https://github.com/Byron/flate2-rs/commit/89ae2a1dc50d86e133f30fde0bd2fc51afa247ae))
    - Merge pull request #163 from robinst/patch-1 ([`0393c50`](https://github.com/Byron/flate2-rs/commit/0393c502df3f5f7f6de0cd69031a7f5662d93e5e))
    - Merge pull request #132 from fafhrd91/master ([`f7d428e`](https://github.com/Byron/flate2-rs/commit/f7d428ed7d8d3eb684f53072a2b108db475b176c))
    - Recommend using bufread types for decoding `&[u8]` ([`2e69f17`](https://github.com/Byron/flate2-rs/commit/2e69f17b94941f23c89f46cc393ec5cd6c91ec43))
    - Use io::Chain stead of custom implementation ([`955f6da`](https://github.com/Byron/flate2-rs/commit/955f6da3935a245e8105a1a9320338e04b2b574f))
    - Simplify GzDecoder::write impl ([`6cb7bad`](https://github.com/Byron/flate2-rs/commit/6cb7badeb9a1445ccb628738753c82f322eb4003))
    - Replace try! with ? ([`cf45702`](https://github.com/Byron/flate2-rs/commit/cf45702b679f30a2cf56d5797f6b872e6878a445))
    - PartialEq for DecompressErrorInner ([`903877f`](https://github.com/Byron/flate2-rs/commit/903877f78ce703a50fe97f1446125000118a9693))
    - Better strategy for partial gz header ([`fad7b1d`](https://github.com/Byron/flate2-rs/commit/fad7b1d5492319912d9ca49d8bd1cc844b190442))
    - Partial crc write test ([`766034b`](https://github.com/Byron/flate2-rs/commit/766034bb005209b0df4bae2056427056f532a941))
    - Check crc on finish ([`dc2a61c`](https://github.com/Byron/flate2-rs/commit/dc2a61cf7f628644cff465339e2f96dacd95cb19))
    - Check stream crc on finish ([`8495029`](https://github.com/Byron/flate2-rs/commit/84950291d20ae5a380f5a55c65f1f6ada55e685d))
    - Do not use buffer if header can be parsed immediately ([`92df505`](https://github.com/Byron/flate2-rs/commit/92df5058ed7b5d18e9c73016bbcb0ba6de3f7927))
    - Add buffer for gz header ([`afe579e`](https://github.com/Byron/flate2-rs/commit/afe579e542220f344b6d478d096b396823cac1a5))
    - Better var names ([`92a798f`](https://github.com/Byron/flate2-rs/commit/92a798f1e010a477ab8ed7f572ffef403132031f))
    - Avoid extra byte in zio::Writer ([`d916363`](https://github.com/Byron/flate2-rs/commit/d9163633db1a7cfa99792ceea5c8c1968208df25))
    - Do not write after StreamEnd ([`c2cf3cf`](https://github.com/Byron/flate2-rs/commit/c2cf3cf0beae8b3ecd9fdba33f6577bf22da5907))
    - Fill crc bytes only after StreamEnd ([`a260d7e`](https://github.com/Byron/flate2-rs/commit/a260d7e67e5c2f11ebddfbc8213d07db7cb9afd0))
    - :GzDecoder does not support tokio ([`37fab35`](https://github.com/Byron/flate2-rs/commit/37fab358591d97e93783c430bc456b6f4235da1a))
    - Add write::GzDecoder ([`da3d935`](https://github.com/Byron/flate2-rs/commit/da3d935904ce30d4a512746e70f0611e3689f0c6))
    - Update the miniz.c file ([`bfb0f04`](https://github.com/Byron/flate2-rs/commit/bfb0f04f3cb5582b9a80ad7bd7d9459e957952b2))
    - Allow failures in rust backend ([`69d74ea`](https://github.com/Byron/flate2-rs/commit/69d74ea92b4e766ea9bc39387653b4cb01124547))
    - Remove verbose from builds ([`0f66d7c`](https://github.com/Byron/flate2-rs/commit/0f66d7c90c264ab88af2e247f26c2cf31187f6ae))
    - Merge pull request #159 from alexcrichton/dependabot/cargo/rand-0.5 ([`e0c24a9`](https://github.com/Byron/flate2-rs/commit/e0c24a9d04534d557db35ee58c0ae3b565086a42))
    - Remove 1.21.0 from travis ([`4615e0d`](https://github.com/Byron/flate2-rs/commit/4615e0dd9e273828ee04d863fef511f8bd6a1a5e))
    - Fix rand 0.5 compat ([`1e20fa9`](https://github.com/Byron/flate2-rs/commit/1e20fa9ac1e18a81ed2d3e87913e81759d696873))
    - Update rand requirement to 0.5 ([`72b0ac0`](https://github.com/Byron/flate2-rs/commit/72b0ac09e0248b99dff9c24f80e4b5f9850a0a29))
    - Fix rustc compat ([`9cfae1c`](https://github.com/Byron/flate2-rs/commit/9cfae1c96b37be6179f5e4903ec891e7463479b9))
    - Don't infinitely return errors on invalid headers ([`1ce113b`](https://github.com/Byron/flate2-rs/commit/1ce113bffda58d2467f3f869fc399d831e560e0c))
    - Merge pull request #155 from kornelski/master ([`9eb6555`](https://github.com/Byron/flate2-rs/commit/9eb6555fac57fb910bafcda860e1e483c0b92659))
    - Replace try!() with ? ([`ac0e1a6`](https://github.com/Byron/flate2-rs/commit/ac0e1a6e046d977c791dea2e6fef2001dde7a8a9))
    - Merge pull request #154 from quadrupleslap-forks-things/master ([`cfc895c`](https://github.com/Byron/flate2-rs/commit/cfc895ceb1e104e7b33a6470b5326fd6431658c2))
    - Added Compress::set_level ([`4398453`](https://github.com/Byron/flate2-rs/commit/43984538ea3776327bb731adc4aec9a5254c5003))
    - Remove rustfmt from CI ([`4e0d485`](https://github.com/Byron/flate2-rs/commit/4e0d48590ff73e214cf7e094928e3df2072356e9))
    - Change documentation to use write_all ([`0837ac0`](https://github.com/Byron/flate2-rs/commit/0837ac0091cb3bcc5fac42c7ff78c7587e4745f9))
    - Merge pull request #74 from Lukazoid/set-dictionary ([`993c788`](https://github.com/Byron/flate2-rs/commit/993c7885cf8b60af0a492e3a5575003b9803ebb2))
    - The set_dictionary can now return error on failure ([`9655960`](https://github.com/Byron/flate2-rs/commit/9655960157e00c873301294de2d996e5ad9d2a8a))
    - Formatting new code ([`2802713`](https://github.com/Byron/flate2-rs/commit/2802713ce39cd23efe62153ea5914dc7219d8218))
    - Only importing FlushCompress for the zlib tests ([`312fecd`](https://github.com/Byron/flate2-rs/commit/312fecd502c6da6c94058c93c7d181ddc49140e3))
    - No longer using Status to signal a dictionary is required ([`4b783d4`](https://github.com/Byron/flate2-rs/commit/4b783d42098d28c714e3087b9d2650b7dec44bdb))
    - Merge remote-tracking branch 'upstream/master' into set-dictionary ([`bcf73cb`](https://github.com/Byron/flate2-rs/commit/bcf73cbce796e4233f04f105682e403635ff4d35))
    - Rustfmt ([`4a4f85f`](https://github.com/Byron/flate2-rs/commit/4a4f85f334b3a94112c9d0457aa75513387f38df))
    - Merge pull request #150 from mdsteele/empty ([`533cae9`](https://github.com/Byron/flate2-rs/commit/533cae9d3642ff9a3cd089a7b48bd64f2b65d795))
    - Fix formatting ([`08572dd`](https://github.com/Byron/flate2-rs/commit/08572dd7c2d9cc0670e566ab1d205ebf4335da1b))
    - Add tests for reading with an empty buffer, and fix bug in GzDecoder ([`31cbd0b`](https://github.com/Byron/flate2-rs/commit/31cbd0bb2a7e0ecd32146709141faa7debcae580))
    - Run `cargo fmt` ([`b769bb7`](https://github.com/Byron/flate2-rs/commit/b769bb798b7e652cb415969bbf7cb2b0beaf06e6))
    - Merge pull request #146 from ignatenkobrain/patch-1 ([`29a5080`](https://github.com/Byron/flate2-rs/commit/29a508065e16cffd8949efc8a06c60a85cbed350))
    - Update rand to 0.4, quickcheck to 0.6 ([`76c4231`](https://github.com/Byron/flate2-rs/commit/76c4231b3352e9d21de6bbac1ac691601b503f20))
    - Merge pull request #145 from oyvindln/link_collision_fix ([`490a60a`](https://github.com/Byron/flate2-rs/commit/490a60afae4f0802f39c5d74839e6b083d4c0792))
    - Workaround to avoid having miniz_oxide_c_api export symbols that collide with miniz-sys ([`3267727`](https://github.com/Byron/flate2-rs/commit/326772710d96aa5e26f81a9311c9cb7635992f3c))
    - Merge pull request #144 from ignatenkobrain/license ([`1e79297`](https://github.com/Byron/flate2-rs/commit/1e792972efb2bf2a84d59eb2251f31bd44ca1d5a))
    - Include LICENSE-* to miniz-sys ([`72f781d`](https://github.com/Byron/flate2-rs/commit/72f781dcd7072522977dcdcbb03c8e53dea97cf4))
    - Merge pull request #139 from pravic/patch-1 ([`5222d4c`](https://github.com/Byron/flate2-rs/commit/5222d4cf5cc2d7b17a198990cb5bbc6ffcbf88a8))
    - Update crate version in README ([`7e9ff2b`](https://github.com/Byron/flate2-rs/commit/7e9ff2b25b673084090f0276ff2e4382ae5bf2e6))
</details>

## v1.0.1 (2017-11-30)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 4 commits contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 1.0.1 ([`5f85d4a`](https://github.com/Byron/flate2-rs/commit/5f85d4a596e9ada5138dacd473fbc81216fd525d))
    - Remove no longer needed `Errors` sections ([`6a732e9`](https://github.com/Byron/flate2-rs/commit/6a732e9168ff028709356826a696f0c550e0d192))
    - Merge pull request #138 from est31/master ([`e5036e6`](https://github.com/Byron/flate2-rs/commit/e5036e61be32fadbb7162d20d87ee7c20cf45861))
    - Emit a RFC 1952 compatible XFL flag again ([`a336dae`](https://github.com/Byron/flate2-rs/commit/a336dae277fa3ab566341c3ebe04eaf6b9f0c796))
</details>

## v1.0.0 (2017-11-29)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 31 commits contributed to the release over the course of 75 calendar days.
 - 75 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Allow publication of 1.0 ([`044ac12`](https://github.com/Byron/flate2-rs/commit/044ac12983473bb1dcaa5054848111339a7add4b))
    - Merge pull request #135 from est31/master ([`264419f`](https://github.com/Byron/flate2-rs/commit/264419f0e3679cff7c2092b2360821fc71ff5e35))
    - Derive Clone for GzHeader ([`0623308`](https://github.com/Byron/flate2-rs/commit/062330846e5f35bfb45c11206d03ce5d71ce6047))
    - Support retrieving and setting the OS byte ([`72bc3d8`](https://github.com/Byron/flate2-rs/commit/72bc3d88118a4385f50f27331441312e9f95c9fa))
    - Merge pull request #133 from GuillaumeGomez/master ([`945b134`](https://github.com/Byron/flate2-rs/commit/945b1342ca10b5f624550e5499741f619c6c5724))
    - Fix pulldown diff ([`9d736f7`](https://github.com/Byron/flate2-rs/commit/9d736f77b5d8e032d4479b221e62132b842db072))
    - Update the README ([`bd1f8bd`](https://github.com/Byron/flate2-rs/commit/bd1f8bdd39831073ecdbf4c911521e5aed3d2779))
    - Tweak travis configuration ([`7390794`](https://github.com/Byron/flate2-rs/commit/739079431f2184b9c29c60bee3376fea7ec347cc))
    - Note about read/write duality on types ([`a2ad3de`](https://github.com/Byron/flate2-rs/commit/a2ad3de290f0f5806790a550d9f3e26ffc995971))
    - Fix tokio tests ([`6117578`](https://github.com/Byron/flate2-rs/commit/6117578b6e6bf6a1861ddcce53fb049c9d5ee690))
    - Defer gz decoding errors to reading ([`0d93fc8`](https://github.com/Byron/flate2-rs/commit/0d93fc86bfd85bb2570923495690b21696e69fab))
    - Merge branch 'flush-refactor' of https://github.com/chrisvittal/flate2-rs ([`6262121`](https://github.com/Byron/flate2-rs/commit/6262121fc65af9b25e74544b28d9c215d0e8d36d))
    - Return a `Result` from compression instead of panicking ([`de7d056`](https://github.com/Byron/flate2-rs/commit/de7d05663cca5eddbfcaeaad125f12bab2db74e0))
    - Switch `Compression` to a struct ([`7e0390b`](https://github.com/Byron/flate2-rs/commit/7e0390b51336b9737eefb10d1afc14d5e311fb9d))
    - Now we're working on 1.0.0! ([`367f295`](https://github.com/Byron/flate2-rs/commit/367f2951e33276b944e0f3aa026c4238044a37a2))
    - Remove the Read/Write extension traits ([`f81a0be`](https://github.com/Byron/flate2-rs/commit/f81a0be0ee559de7cc42c4de03794a5e5ebff894))
    - Fix typo in README ([`cde5d88`](https://github.com/Byron/flate2-rs/commit/cde5d88b0ae22e75d2b86726a614500dc175aadc))
    - Disable warnings when compiling miniz ([`0bbb11c`](https://github.com/Byron/flate2-rs/commit/0bbb11c7dd7090aa5a2f8b11cd050e4aa629f42f))
    - Clarify wording of license information in README. ([`baf6668`](https://github.com/Byron/flate2-rs/commit/baf66689e22055bb896656f34c45cbab8dfb244b))
    - Merge pull request #130 from oyvindln/doc_update ([`434490f`](https://github.com/Byron/flate2-rs/commit/434490fc144e43e9d3cd4a4ee6e9acfc5d7833b0))
    - Mention rust backend & update miniz and flate info ([`4b535ca`](https://github.com/Byron/flate2-rs/commit/4b535cab935137ddad15923b98fafd4dca9d56c4))
    - Merge pull request #128 from oyvindln/rust-backend ([`7f01a9f`](https://github.com/Byron/flate2-rs/commit/7f01a9fc582dadaecc9e5bcc63b60e25fe15d7e6))
    - Merge pull request #129 from ovibos/patch-1 ([`dbf4568`](https://github.com/Byron/flate2-rs/commit/dbf4568cdd515bc90e8a6ce938d32215606442ad))
    - Fix zlib redundant crc32; add rust_backend to travis CI ([`3eea583`](https://github.com/Byron/flate2-rs/commit/3eea58324d99333d5b2048828c60c116c30e83d8))
    - Fix typo ([`7f780d0`](https://github.com/Byron/flate2-rs/commit/7f780d079c86ac78203d8176af366d45dacf1721))
    - Change git repo to crates.io ([`2fcbfd5`](https://github.com/Byron/flate2-rs/commit/2fcbfd55930cc9aa28726d6d022aa401f7783a46))
    - Use add feature and ffi code to use miniz_c_api as back-end ([`b6ce6e9`](https://github.com/Byron/flate2-rs/commit/b6ce6e980d7d63e4b6269c4a77a293d8284c4a23))
    - Fix license directive ([`2b75ebc`](https://github.com/Byron/flate2-rs/commit/2b75ebc78ca0477b9c6a9d321e0db0e00e20338d))
    - Bump miniz-sys to 0.1.10 ([`6a075aa`](https://github.com/Byron/flate2-rs/commit/6a075aacfabcb6d8cdea5a3b0462dc61d34796d5))
    - Update gcc dependency ([`a0b03f8`](https://github.com/Byron/flate2-rs/commit/a0b03f89b8b349fd218a7e16056e6d5a6ce657f4))
    - Remove unused import ([`4cc0a8c`](https://github.com/Byron/flate2-rs/commit/4cc0a8c510951a7c3795861f7777f5b762030e0d))
</details>

## v0.2.20 (2017-09-15)

<csr-id-d5248cff4370cf778ad135c80eae2df04bff2cf6/>

### Other

 - <csr-id-d5248cff4370cf778ad135c80eae2df04bff2cf6/> :mtime documentation clarification
   `mtime` info was taken from RFC 1952:
   http://www.zlib.org/rfc-gzip.html#member-format

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 57 commits contributed to the release over the course of 155 calendar days.
 - 155 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.20 ([`2c42b6d`](https://github.com/Byron/flate2-rs/commit/2c42b6d683f7cd55265839768c2a118e0e343962))
    - Don't specifically set the OS in the gz header ([`1e94537`](https://github.com/Byron/flate2-rs/commit/1e94537e0255881772968a5bbcb377965ff44383))
    - Remove line merge accidentally added ([`d9291ed`](https://github.com/Byron/flate2-rs/commit/d9291edbc95cc8613cb2784e7ef412dbb32f4f98))
    - Merge branch 'master' into flush-refactor ([`537aa2a`](https://github.com/Byron/flate2-rs/commit/537aa2ad2be1485edccf23b8ff685ecad8d06763))
    - Return an error when `write` returns 0 bytes ([`a428791`](https://github.com/Byron/flate2-rs/commit/a428791ae67308d92b8d720d0a30a8e57d5b2645))
    - Remove the need for `pub(crate)` ([`d4168a5`](https://github.com/Byron/flate2-rs/commit/d4168a52d4d8e129cce73ab1c2c87e765c39ba57))
    - Remove unused imports under the tokio feature ([`e8a11e7`](https://github.com/Byron/flate2-rs/commit/e8a11e779acaf28bf2fd7aa8a03f1995140e9d4f))
    - Refactor the deflate, gz and zlib modules ([`d6fdbca`](https://github.com/Byron/flate2-rs/commit/d6fdbca04a22a85cdf490d86ffe1738eae58f298))
    - Another attempt to fix travis... ([`e534385`](https://github.com/Byron/flate2-rs/commit/e53438543d167a7da95ddced6fb2ec148dd810c3))
    - Try to fix travis again ([`b7f2e2e`](https://github.com/Byron/flate2-rs/commit/b7f2e2e4b3930d1328ee2cc27ed2bdd40c439b5a))
    - Remove unused `mut` clauses ([`3ee4ce4`](https://github.com/Byron/flate2-rs/commit/3ee4ce43f3029e507f05c261a017bc4fd98c1e77))
    - Fix travis ([`28b656b`](https://github.com/Byron/flate2-rs/commit/28b656b5d4fd78fe3e5ed335771945cdb14b14a3))
    - Merge pull request #118 from saurvs/patch-2 ([`877dc59`](https://github.com/Byron/flate2-rs/commit/877dc59b77cea29c73a87e93d863186727cde015))
    - Add x86_64-pc-windows-gnu appveyor target ([`5f261ed`](https://github.com/Byron/flate2-rs/commit/5f261ed5098f63258614a852e222f63811147df0))
    - Merge pull request #115 from AndyGauge/examples ([`2b324f6`](https://github.com/Byron/flate2-rs/commit/2b324f65d27a89ebc391aa56abfd6a6ff20ce39a))
    - Crate level examples, including FlateReadExt ([`1538c03`](https://github.com/Byron/flate2-rs/commit/1538c03deeb4e3222d2542b0ea178dd753631fc5))
    - Merge pull request #114 from AndyGauge/examples ([`1b1cd65`](https://github.com/Byron/flate2-rs/commit/1b1cd65cd59c7e6cdb89535167cf3c6429ef00b1))
    - Added deflate examples ([`c0de871`](https://github.com/Byron/flate2-rs/commit/c0de871c2099f46d2dd4c4d2615a6773c85b9d73))
    - Merge pull request #111 from AndyGauge/examples ([`a73c33e`](https://github.com/Byron/flate2-rs/commit/a73c33ec960a5fe1cf0167187a82dac4358c0bfc))
    - Examples directory provides runnable examples and doc comments have examples to be compiled into documentation.  Examples are for Zlib and Gzip structs ([`4f949ba`](https://github.com/Byron/flate2-rs/commit/4f949ba2623b0bc11ae7961d53c54409e84834fa))
    - Add _Nonexhaustive member to Flush enums ([`50a34d6`](https://github.com/Byron/flate2-rs/commit/50a34d6aa47051c4568565abca974c1a2f8b162d))
    - Clean up docs and new Flush trait. ([`e6da230`](https://github.com/Byron/flate2-rs/commit/e6da230b9ad1292b5e1b50aa004da1abddcf6285))
    - Split Flush into two enums. ([`1bdd11c`](https://github.com/Byron/flate2-rs/commit/1bdd11c447d8c088dc6d02ac4a0002b4b907c601))
    - Fix a gz case where flush is called early ([`42156e6`](https://github.com/Byron/flate2-rs/commit/42156e68eedc5bf2cf212aaf0e8f4ba06a164ad6))
    - Clarify some more error sections ([`b4b5e5b`](https://github.com/Byron/flate2-rs/commit/b4b5e5b67fadfbd6dd0d0a0759f77e4a57b71f88))
    - Merge pull request #107 from opilar/bugfix/errors-docs ([`0c964d0`](https://github.com/Byron/flate2-rs/commit/0c964d0347fad10117aa769bb14e38f692cd69be))
    - Add `Debug for Compression` ([`9423dfc`](https://github.com/Byron/flate2-rs/commit/9423dfca73c8d1cbbc3f6d0a2c1ff4751bc79828))
    - Merge branch 'master' of https://github.com/kper/flate2-rs ([`2af7098`](https://github.com/Byron/flate2-rs/commit/2af7098846ef550641ebf6625a17d78cda45a006))
    - Require various bounds on constructors ([`878fa94`](https://github.com/Byron/flate2-rs/commit/878fa94175b4d9927e7739414af6f730366fa451))
    - Merge branch 'bugfix/read-trait' of https://github.com/opilar/flate2-rs ([`7c36a58`](https://github.com/Byron/flate2-rs/commit/7c36a58cef84a822cbec5bb2e0016a0954f605fd))
    - Merge pull request #106 from opilar/bugfix/doc-links ([`76af403`](https://github.com/Byron/flate2-rs/commit/76af403f3982ae18aea7ab94f06622826f72b515))
    - Errors to it's section ([`39591b0`](https://github.com/Byron/flate2-rs/commit/39591b060ddd9038e56154f58f3c471f20df49f0))
    - Documentation links ([`882bcf1`](https://github.com/Byron/flate2-rs/commit/882bcf18e30417a390a1ad3fbba4cd9dd65496f5))
    - Implement custom Debug trait for `StreamWrapper` #83 ([`3012817`](https://github.com/Byron/flate2-rs/commit/3012817e430fd6d9c01b9623f56b1011bf2b8d3e))
    - Implement custom Debug trait for `BufReader` #83 ([`6773395`](https://github.com/Byron/flate2-rs/commit/6773395f104880c98fb8f14d47b0f8ed1175a141))
    - Merge pull request #103 from nivkner/master ([`4eefbc0`](https://github.com/Byron/flate2-rs/commit/4eefbc0ce69a733d7795f89b5f19469242d4ab47))
    - Remove  bounds from impl ([`ea990e1`](https://github.com/Byron/flate2-rs/commit/ea990e1fbfcaf3d3c1a8e39fd8763808ab69f75a))
    - Expand Documentation about the GzHeader datetime method ([`e0715cc`](https://github.com/Byron/flate2-rs/commit/e0715cc8b29b8749bc5ede82f1dfcebd5cc03a8c))
    - Implement Debug for public types #83 ([`e3fd774`](https://github.com/Byron/flate2-rs/commit/e3fd774793d32f381073ce012fa5ff105bced8b7))
    - Remove  bound on structs ([`a46bd1b`](https://github.com/Byron/flate2-rs/commit/a46bd1b05e9395739a9f2718f01f5e3a94924867))
    - Add a method to get mtime of a GzHeader as a datetime ([`4596fcb`](https://github.com/Byron/flate2-rs/commit/4596fcbefbb9ff4b1b78f4b4f6d3f6ee74cf4f69))
    - Merge pull request #102 from opilar/feature/badges ([`acc254f`](https://github.com/Byron/flate2-rs/commit/acc254f24df94e32831df0a4f4176a44946fca00))
    - Docs badge ([`147d423`](https://github.com/Byron/flate2-rs/commit/147d4233399f9187890731bea2b0873c597bdde2))
    - Add crates badge ([`df81a79`](https://github.com/Byron/flate2-rs/commit/df81a79140dbc49ba4580074069c7dc813510f13))
    - Add badges in Cargo.toml ([`0f2b9f3`](https://github.com/Byron/flate2-rs/commit/0f2b9f3c5ce7f8ec0356b5541181f99ba114c77f))
    - Merge pull request #101 from Matt8898/traits ([`46ffa20`](https://github.com/Byron/flate2-rs/commit/46ffa201bce239d13511235f7bb6e03967570e62))
    - Implement common traits for GzHeader, Compression, Flush and Status. ([`b370a2c`](https://github.com/Byron/flate2-rs/commit/b370a2cb2346bdbd714d24e09fff06d7a71961ac))
    - Merge pull request #99 from Matt8898/gzu8vec ([`b22245f`](https://github.com/Byron/flate2-rs/commit/b22245f01be7e4e8f244591ba2d255151cd7a22d))
    - GzBuilder methods now take Into<Vec<u8>>. ([`11f44f4`](https://github.com/Byron/flate2-rs/commit/11f44f4b3651c8200041c28ca69ccf5857942af4))
    - Merge pull request #97 from opilar/bugfix/gz-headers-docs ([`72df49c`](https://github.com/Byron/flate2-rs/commit/72df49ce8cbdd535e3150c96cca44d0e4744c605))
    - Document discourage usage ([`40f6eff`](https://github.com/Byron/flate2-rs/commit/40f6eff7c91d134fa347e05de499049802050097))
    - Merge pull request #98 from opilar/bugfix/gz-builder-doc ([`e1f7fd0`](https://github.com/Byron/flate2-rs/commit/e1f7fd0119a4f8d84a1b84b9f379a6bc03bb6ad7))
    - GzBuilder documentation rewrite ([`583f8d2`](https://github.com/Byron/flate2-rs/commit/583f8d20d3ded5d9285471c093caa72266008451))
    - Document the GzBuilder panic cases ([`72d6a70`](https://github.com/Byron/flate2-rs/commit/72d6a70c43686dc35e8d4e32023c35909cfe9ef4))
    - :mtime documentation clarification ([`d5248cf`](https://github.com/Byron/flate2-rs/commit/d5248cff4370cf778ad135c80eae2df04bff2cf6))
    - Merge pull request #95 from SamWhited/line_endings ([`abe6dc4`](https://github.com/Byron/flate2-rs/commit/abe6dc4a95944aba416f94fa8358a034e28fa4b3))
    - Use Unix style (LF) line endings ([`8915319`](https://github.com/Byron/flate2-rs/commit/891531963e7589ab89a759b85d88cb76ec7dfded))
</details>

## v0.2.19 (2017-04-12)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 1 day passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.19 ([`899e4be`](https://github.com/Byron/flate2-rs/commit/899e4beb3049c7145d8a8265cc5c8f60c3c564f0))
    - Add async-I/O support to the crate ([`9abf34f`](https://github.com/Byron/flate2-rs/commit/9abf34f639105805618464fbd6670d62c7c69d1a))
</details>

## v0.2.18 (2017-04-11)

<csr-id-65a49bc4c2b733eafcb8750af19c522dd52f1a58/>
<csr-id-81c527c5e9f317a036d0ae5e834fbe503b2bd219/>
<csr-id-39796589aee196dc6e0b60282930f46072fbef43/>

### Other

 - <csr-id-65a49bc4c2b733eafcb8750af19c522dd52f1a58/> implement get_ref, get_mut and into_inner wherever missing
 - <csr-id-81c527c5e9f317a036d0ae5e834fbe503b2bd219/> add get_ref to writer
 - <csr-id-39796589aee196dc6e0b60282930f46072fbef43/> add get_ref, and rename inner to get_mut
   The renaming is to bring this in line with all the other readers.

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 23 commits contributed to the release over the course of 77 calendar days.
 - 80 days passed between releases.
 - 3 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.18 ([`a3a6752`](https://github.com/Byron/flate2-rs/commit/a3a6752253740ce81e6af46e0db6e318292c3603))
    - Merge pull request #94 from veldsla/multigzdocs ([`06b8e3f`](https://github.com/Byron/flate2-rs/commit/06b8e3f1eefc3c94dac6318a6512393c9998a071))
    - Extend MultiGzDecoder docs ([`937b9bf`](https://github.com/Byron/flate2-rs/commit/937b9bfce1d5b57fb4fd683a54278e68da524c10))
    - Add forwarding impls for opposite read/write trait ([`39812e6`](https://github.com/Byron/flate2-rs/commit/39812e6017948e89458e0fc2c382e2067587b1ac))
    - Added missing cast of the raw adler to a u32 ([`4ece6f0`](https://github.com/Byron/flate2-rs/commit/4ece6f0f161ee9e728c52885b3dc36065afe0e2e))
    - Merge pull request #68 from vandenoever/crc ([`c728bca`](https://github.com/Byron/flate2-rs/commit/c728bcadb0304bf82dcfa9bebda8ebddc68903e5))
    - Make Crc and CrcReader public and add a combine method. ([`5129117`](https://github.com/Byron/flate2-rs/commit/512911787dec359a5c55fff066baf19744618dad))
    - Exposed deflateSetDictionary and inflateSetDictionary functionality when using zlib ([`e21bad3`](https://github.com/Byron/flate2-rs/commit/e21bad3c54e9ce8e0ade78ac8c8a6f8ffc3d40c3))
    - Merge pull request #69 from Lukazoid/stable-stream-address ([`aed90cf`](https://github.com/Byron/flate2-rs/commit/aed90cf1a3f2e149010296817c60846432fe0c70))
    - Merge pull request #70 from Lukazoid/integration-tests-line-endings ([`3a53fcb`](https://github.com/Byron/flate2-rs/commit/3a53fcb9b7fd031e8e8ff1b302590bd91e42c490))
    - Added a .gitattributes to configure how files are handled in git. ([`c906ffb`](https://github.com/Byron/flate2-rs/commit/c906ffb68c2e36d13dd02c81fe2d039a6a87271d))
    - Included missing import when using miniz. ([`a2ccdaf`](https://github.com/Byron/flate2-rs/commit/a2ccdafad89b168ebfa7e2ae6b268286e7acc134))
    - When using zlib the stream object is now allocated on the heap. ([`6cd3249`](https://github.com/Byron/flate2-rs/commit/6cd324916d8a8b4000beb96239d768e418fd7add))
    - Merge pull request #66 from sid0/get-stuff ([`fb85b41`](https://github.com/Byron/flate2-rs/commit/fb85b41d8e6ccb2cfd0dcf54022620a45eb0d75b))
    - Implement get_ref, get_mut and into_inner wherever missing ([`65a49bc`](https://github.com/Byron/flate2-rs/commit/65a49bc4c2b733eafcb8750af19c522dd52f1a58))
    - Add get_ref to writer ([`81c527c`](https://github.com/Byron/flate2-rs/commit/81c527c5e9f317a036d0ae5e834fbe503b2bd219))
    - Add get_ref, and rename inner to get_mut ([`3979658`](https://github.com/Byron/flate2-rs/commit/39796589aee196dc6e0b60282930f46072fbef43))
    - Merge pull request #65 from vandenoever/flush ([`a6289ee`](https://github.com/Byron/flate2-rs/commit/a6289eeda010a4f6b7b8522a762536cf45809546))
    - Add flush_finish() ([`6fa7e95`](https://github.com/Byron/flate2-rs/commit/6fa7e95a2a1c87dddbbbdc0e89d6e6338a64c5d6))
    - Merge pull request #43 from veldsla/multigz ([`b36a942`](https://github.com/Byron/flate2-rs/commit/b36a942ecd864a0c580e4e267ec8927ee646d57f))
    - Fix tests on zlib ([`f8a20de`](https://github.com/Byron/flate2-rs/commit/f8a20de7bfaaa603cb8f7456c6d09c89bf002c00))
    - Add a binding for `Decompress::reset` ([`93c9806`](https://github.com/Byron/flate2-rs/commit/93c9806be3eaf58a925f2905196a5ec77495bfff))
    - Add a workspace ([`1c9edb9`](https://github.com/Byron/flate2-rs/commit/1c9edb942fba5c19c2d9d8240cb785a6432f0f1e))
</details>

## v0.2.17 (2017-01-20)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 3 commits contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump versions again ([`c87a82b`](https://github.com/Byron/flate2-rs/commit/c87a82b8e71486bad14fa481709012829c77d0a4))
    - Update flate2 categories ([`caa6cb3`](https://github.com/Byron/flate2-rs/commit/caa6cb37e49158fb030f0f5bb04e0dcb1fe28911))
    - Update miniz-sys doc url ([`40c635a`](https://github.com/Byron/flate2-rs/commit/40c635ae62eb9610a0108b60985c7042317bfb61))
</details>

## v0.2.16 (2017-01-20)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 6 commits contributed to the release.
 - 1 day passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.16 and 0.1.8 ([`bad0a6e`](https://github.com/Byron/flate2-rs/commit/bad0a6ee96dd19ff8b386c0ee96b884d0edecac2))
    - Update docs urls ([`3abeacf`](https://github.com/Byron/flate2-rs/commit/3abeacff70b1a03eeade8083afcd68f024cdf17b))
    - Miniz-sys is external ffi bindings ([`fb8214b`](https://github.com/Byron/flate2-rs/commit/fb8214bee19479c363bca431ed7232afe9a56df5))
    - Update categories ([`5ed7a84`](https://github.com/Byron/flate2-rs/commit/5ed7a84008d383027028b441a6dcc957c75aad6c))
    - Merge pull request #59 from shepmaster/patch-1 ([`762ede5`](https://github.com/Byron/flate2-rs/commit/762ede58b253a17c1f8ac54667b894f986c1a3f3))
    - Add categories to Cargo.toml ([`6bc498e`](https://github.com/Byron/flate2-rs/commit/6bc498ec6a398698095c5cb588fc36b3f7045b15))
</details>

## v0.2.15 (2017-01-18)

<csr-id-dd7a500d3f0ec473ae74e1a21b232c8a12e4bdef/>
<csr-id-424bcaf3eadfe33ea5dc3514a0d4d4dfecaf82cf/>
<csr-id-404e054f0bd9713538b65e21a6d4cde15c9c3ea9/>

### Other

 - <csr-id-dd7a500d3f0ec473ae74e1a21b232c8a12e4bdef/> typo
 - <csr-id-424bcaf3eadfe33ea5dc3514a0d4d4dfecaf82cf/> small fixes
 - <csr-id-404e054f0bd9713538b65e21a6d4cde15c9c3ea9/> remove extraneous word

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 25 commits contributed to the release over the course of 250 calendar days.
 - 256 days passed between releases.
 - 3 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.15 ([`1446d7c`](https://github.com/Byron/flate2-rs/commit/1446d7c655ca9a0cd15c0afbbb418cc87372feee))
    - Fix an infinite loop in flush ([`d98941c`](https://github.com/Byron/flate2-rs/commit/d98941c622d0704e524fbf47d80e040de46f4f00))
    - Merge pull request #56 from kornholi/big-streams ([`6fac4f7`](https://github.com/Byron/flate2-rs/commit/6fac4f7a11824785707c394300d9790180a036bd))
    - Keep track of total bytes processed manually ([`817ff22`](https://github.com/Byron/flate2-rs/commit/817ff22a80ac06e8d8d6292d3c2f719958c9d911))
    - Update documentation with modern docs ([`21aa304`](https://github.com/Byron/flate2-rs/commit/21aa304ee003bd438ccabcb6c33cdd67d6c0c0d1))
    - Upgrade quickcheck dependency ([`4c4b766`](https://github.com/Byron/flate2-rs/commit/4c4b7666877d27b424f248b5a1634cc5c6d67d5b))
    - Fix license to be correct on miniz-sys ([`a49ff67`](https://github.com/Byron/flate2-rs/commit/a49ff67ad2fab0095021fab3b98608e1d8d3ca49))
    - Update travis token ([`1753e03`](https://github.com/Byron/flate2-rs/commit/1753e030f50be2427517a58b2d1d4c46656a7984))
    - Fix README tests ([`ed8a38e`](https://github.com/Byron/flate2-rs/commit/ed8a38e33827153f6c5e4b63fc3c67a51a0ebf77))
    - Merge pull request #52 from l1048576/loosen-trait-bounds ([`bf0b5e4`](https://github.com/Byron/flate2-rs/commit/bf0b5e458da245711b06240ada13a7da6e428361))
    - Remove unnecessary trait bounds from reader types ([`ed2f108`](https://github.com/Byron/flate2-rs/commit/ed2f10843045c6e6f887c9ce5bcfbea7f8a7c193))
    - Pass --target on appveyor ([`ab3cdab`](https://github.com/Byron/flate2-rs/commit/ab3cdab0367c0f7744fa495e2372dee5c86d004b))
    - Fix tests/compile with zlib ([`d595ab3`](https://github.com/Byron/flate2-rs/commit/d595ab3f844fca3650a12445ca052a36c3a1e1e6))
    - Handle another error case in `decompess` ([`cad0ac6`](https://github.com/Byron/flate2-rs/commit/cad0ac673cc02d76675c765e1430b852226fb13f))
    - Merge pull request #47 from tshepang/patch-1 ([`633e94f`](https://github.com/Byron/flate2-rs/commit/633e94fb0117610e2fe59b965a30168f193561ce))
    - Merge pull request #48 from tshepang/patch-2 ([`6a07a4b`](https://github.com/Byron/flate2-rs/commit/6a07a4b372f0ed97f3e087ebc6ce56a826d7f1ed))
    - Merge pull request #49 from tshepang/patch-3 ([`48d257d`](https://github.com/Byron/flate2-rs/commit/48d257dbdbef5845c24b21e8ba19ce1ffe657592))
    - Typo ([`dd7a500`](https://github.com/Byron/flate2-rs/commit/dd7a500d3f0ec473ae74e1a21b232c8a12e4bdef))
    - Small fixes ([`424bcaf`](https://github.com/Byron/flate2-rs/commit/424bcaf3eadfe33ea5dc3514a0d4d4dfecaf82cf))
    - Remove extraneous word ([`404e054`](https://github.com/Byron/flate2-rs/commit/404e054f0bd9713538b65e21a6d4cde15c9c3ea9))
    - Merge pull request #46 from posborne/issue-45-regression-test ([`0ca3390`](https://github.com/Byron/flate2-rs/commit/0ca339096903aa2eb208955c730b4f5012c22803))
    - Regression test for issue #45 ([`15b59cd`](https://github.com/Byron/flate2-rs/commit/15b59cd467be6bdac28852eaccf0b125ae475de4))
    - Don't unwrap() when flushing ([`65ac1f6`](https://github.com/Byron/flate2-rs/commit/65ac1f68f6ac25a914e8513a0f2531b405218be6))
    - Implemented multi member gz decoder ([`7a2f0e7`](https://github.com/Byron/flate2-rs/commit/7a2f0e78fed4c7a597e9ce2bbcb890382e5a98ef))
    - Add a number of quickcheck tests ([`395d15e`](https://github.com/Byron/flate2-rs/commit/395d15e0c1b322122b0d7839facefb2d6277ba9e))
</details>

## v0.2.14 (2016-05-06)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 12 commits contributed to the release over the course of 101 calendar days.
 - 103 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.14 ([`6bbb4b9`](https://github.com/Byron/flate2-rs/commit/6bbb4b97a95c00913d902072df350b8a6b8c4781))
    - Work for non-zero-length vectors ([`522256b`](https://github.com/Byron/flate2-rs/commit/522256b653087b615b5244fd601ab599026d2430))
    - Delegate _vec methods to normal methods ([`65cc004`](https://github.com/Byron/flate2-rs/commit/65cc004dfbb429acfb6225d548e034872767ba37))
    - Merge the mem/stream modules ([`3cc2956`](https://github.com/Byron/flate2-rs/commit/3cc295669a2d1071bb0f5e6b502ac3b2e6cf7e12))
    - Finally delete the entire raw module ([`42566e0`](https://github.com/Byron/flate2-rs/commit/42566e0461b222154ee2717ac4dbd8e78e38097f))
    - Refactor deflate/zlib writing to use the mem module ([`40ff16c`](https://github.com/Byron/flate2-rs/commit/40ff16c3ae34ca24b871a2c3e34dc861fa625600))
    - Add buffered gz types ([`b5e85fa`](https://github.com/Byron/flate2-rs/commit/b5e85fa7f894543271af118f3d4edf6b0c3c599a))
    - Add a suite of get_ref/get_mut methods ([`605c9e0`](https://github.com/Byron/flate2-rs/commit/605c9e0af72819c1ecfb907ba93a51d0d6fb99c5))
    - Add a `bufread` module for buffered types ([`b91748a`](https://github.com/Byron/flate2-rs/commit/b91748acd93c023c2bf374094afb7fca7c55aea5))
    - Add a BufRead implementation for CrcReader ([`bb5d7c9`](https://github.com/Byron/flate2-rs/commit/bb5d7c9f2a4284ebc6d36d4173c79ed521fe0ec4))
    - Return u32 from crc::sum ([`98874c3`](https://github.com/Byron/flate2-rs/commit/98874c3a4469a8b6681c5af2c055eaae9ec54915))
    - Move travis from 1.1.0 -> stable ([`4673059`](https://github.com/Byron/flate2-rs/commit/46730593b97775c381149fcc6ebc42f6afee01b7))
</details>

## v0.2.13 (2016-01-24)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 5 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.13 ([`317a40e`](https://github.com/Byron/flate2-rs/commit/317a40ed1928a6f3dbf4a5f219c60a70011e83ba))
    - Implement Error for DataError ([`ec5c851`](https://github.com/Byron/flate2-rs/commit/ec5c851f0d031a73e78707763cf9f64bb02137d9))
</details>

## v0.2.12 (2016-01-18)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release over the course of 70 calendar days.
 - 71 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump flate2 to 0.2.12 ([`723cc06`](https://github.com/Byron/flate2-rs/commit/723cc06bdbedd9fa33d99e52ca000b6cc0c5158a))
    - Add zlib to README ([`0496e3e`](https://github.com/Byron/flate2-rs/commit/0496e3e9c784b2985370faf8b788cb745c55dc2b))
    - Allow using zlib as a backend ([`2709c8e`](https://github.com/Byron/flate2-rs/commit/2709c8e921e23aecbfb925bc81b62281c17b273e))
    - Add test asserting encoders/decoders are send/sync ([`9db87d1`](https://github.com/Byron/flate2-rs/commit/9db87d1853739191f37b547896869298d4e2a371))
    - Merge pull request #36 from kali/master ([`1e2507e`](https://github.com/Byron/flate2-rs/commit/1e2507ede4ea9305006b16d6baf4b97a02e3a00d))
    - Make Stream impl Send and Sync ([`a980e70`](https://github.com/Byron/flate2-rs/commit/a980e7067a626c3e9df8d13fd91885dde9220836))
    - Merge pull request #32 from flying-sheep/fix-deps ([`1e01345`](https://github.com/Byron/flate2-rs/commit/1e0134586a45b72712ee583211768ba5240253d7))
</details>

## v0.2.11 (2015-11-08)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.11, fixed miniz dep version ([`51eecb7`](https://github.com/Byron/flate2-rs/commit/51eecb75ccd9adb4e80bc508bc71ebdbd63d6709))
    - Bump to 0.1.7 ([`70d13e2`](https://github.com/Byron/flate2-rs/commit/70d13e26599c1513523d784bbb67daea9c798947))
</details>

## v0.2.10 (2015-11-07)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 10 commits contributed to the release over the course of 8 calendar days.
 - 59 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.10 ([`e013dac`](https://github.com/Byron/flate2-rs/commit/e013dace27c4f4d754681a675c0b5d1b8a30b412))
    - Rename amt to amt_as_u32 ([`14a3e57`](https://github.com/Byron/flate2-rs/commit/14a3e573445ebea7921efcce031ac1235afe2ec1))
    - Bump dep on libc ([`af3c4b9`](https://github.com/Byron/flate2-rs/commit/af3c4b906b89fa5321e45b50ea49bb0772f55d98))
    - Handle 0-length reads a little better ([`2f04abb`](https://github.com/Byron/flate2-rs/commit/2f04abb0776948018813a3748d53598b2e41cf75))
    - Fix compiles ([`93da22d`](https://github.com/Byron/flate2-rs/commit/93da22d795c35b7f5eea255d43512d68e3fdd8ec))
    - Add total_in/total_out to DecoderWriter streams ([`02e5c61`](https://github.com/Byron/flate2-rs/commit/02e5c61065ba3d425dc7924ba1d9f31b693755fe))
    - Merge pull request #29 from joshuawarner32/master ([`3147e02`](https://github.com/Byron/flate2-rs/commit/3147e02ddd991af7d5dcdf812159fd7a6b849d37))
    - Expose total_in and total_out methods in {Zlib,Deflate}Decoder ([`5357b29`](https://github.com/Byron/flate2-rs/commit/5357b296b06a8d30719dd88f33ea370e1cae8204))
    - Run rustfmt over miniz-sys ([`dcb00b6`](https://github.com/Byron/flate2-rs/commit/dcb00b6dff1fd4414a9978d557c3d17d682e04b4))
    - Run rustfmt over the library ([`4b85197`](https://github.com/Byron/flate2-rs/commit/4b8519724c0c18ede5e14b9aa480ffe28f7bed40))
</details>

## v0.2.9 (2015-09-09)

<csr-id-00023b27a249f92e588662a14c740d2d938719e4/>

### Other

 - <csr-id-00023b27a249f92e588662a14c740d2d938719e4/> Add missing flush modes and tweak style

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 7 commits contributed to the release over the course of 6 calendar days.
 - 9 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump miniz-sys to 0.1.6 and flate2 to 0.2.9 ([`c494b5e`](https://github.com/Byron/flate2-rs/commit/c494b5ea1be244df9de43a0aa93f0c848e7cfcef))
    - Add reset methods to decoders as well ([`a930f18`](https://github.com/Byron/flate2-rs/commit/a930f183166bd77a3ee0574f6cd512fb7bbeef73))
    - Expose `reset` on {read,write}::{Zlib,Flate}Encoder ([`0e66bab`](https://github.com/Byron/flate2-rs/commit/0e66bab274837286a69f241fa5b0a2dc0999e6ad))
    - Add raw in-memory streams for compress/decompress ([`4414be4`](https://github.com/Byron/flate2-rs/commit/4414be451621c9345612c273e231e621f8d7aa92))
    - Fix a TODO, use Box<[T]> instead of Vec<T> ([`3f3432b`](https://github.com/Byron/flate2-rs/commit/3f3432bfe61b05ef7db24d5ca980291aa881ed10))
    - Add missing flush modes and tweak style ([`00023b2`](https://github.com/Byron/flate2-rs/commit/00023b27a249f92e588662a14c740d2d938719e4))
    - Test on OSX and Linux on Travis ([`3dac3ce`](https://github.com/Byron/flate2-rs/commit/3dac3ceee4908ce0bbf44f8a361a693f9af1d4b4))
</details>

## v0.2.8 (2015-08-31)

<csr-id-c0c9b4269876e87fa8bd3910499f9e42aa9bd555/>

### Other

 - <csr-id-c0c9b4269876e87fa8bd3910499f9e42aa9bd555/> Provide the build directory path to dependencies

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 14 commits contributed to the release over the course of 123 calendar days.
 - 130 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.8 ([`a239771`](https://github.com/Byron/flate2-rs/commit/a239771f9838cc55e70d2dac55ff3f0d1a6fc8f5))
    - Ensure EOF is returned when gz finishes ([`75b0f9d`](https://github.com/Byron/flate2-rs/commit/75b0f9da00a0c3283df4e3e1022ef92d1cc3c864))
    - Tweak travis build ([`8e36637`](https://github.com/Byron/flate2-rs/commit/8e36637d0bad3d43383d815d1db42015fe686a6d))
    - Don't call vcvarsall manually ([`b80110d`](https://github.com/Byron/flate2-rs/commit/b80110d50c56ab3af960a46ecdbe75d9cdac3c8a))
    - Test on 32-bit msvc ([`83476f4`](https://github.com/Byron/flate2-rs/commit/83476f41a47671fe877386eb1e224224bee10405))
    - Merge pull request #24 from benaryorg/master ([`9fb88b2`](https://github.com/Byron/flate2-rs/commit/9fb88b25da460b3f4b75f96bd948c3f154bb87c7))
    - Fix typo ([`76c1d6c`](https://github.com/Byron/flate2-rs/commit/76c1d6c349ea6069bddecdfddc0f1fe95c00ab10))
    - Use combined installer for targets ([`5a7bca2`](https://github.com/Byron/flate2-rs/commit/5a7bca236226f6895ab4b5caf558d3b9f6927c80))
    - Test on MinGW and MSVC ([`a0df49f`](https://github.com/Byron/flate2-rs/commit/a0df49f0503b993ecf9bebbe920a940f9202e3f5))
    - Test on 1.0.0, beta, nightly ([`411d5c6`](https://github.com/Byron/flate2-rs/commit/411d5c618eee48044402ef59e63e85ecf799e79f))
    - Bump to 0.1.5 ([`91ff005`](https://github.com/Byron/flate2-rs/commit/91ff005a9e437113c89d957eeb7d1cb9010ee127))
    - Merge pull request #22 from kmcallister/root ([`71c5ae6`](https://github.com/Byron/flate2-rs/commit/71c5ae6432f7f10cb28fcb9b5560af33e035e321))
    - Provide the build directory path to dependencies ([`c0c9b42`](https://github.com/Byron/flate2-rs/commit/c0c9b4269876e87fa8bd3910499f9e42aa9bd555))
    - Add appveyor config ([`5a78698`](https://github.com/Byron/flate2-rs/commit/5a786987683b023f658e40487e168ec1765e26a5))
</details>

## v0.2.7 (2015-04-23)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release over the course of 18 calendar days.
 - 20 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.7 ([`10beae7`](https://github.com/Byron/flate2-rs/commit/10beae7641e1a687794b5cafbd03ee734ff98fb4))
    - Stop using PhantomFn ([`a0d9a92`](https://github.com/Byron/flate2-rs/commit/a0d9a92ec7bbf8931e97f8ba4dc3c863f231bee2))
</details>

## v0.2.6 (2015-04-03)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 1 commit contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.6 ([`87123ff`](https://github.com/Byron/flate2-rs/commit/87123ff43455640def6e67075c012fb38df74b65))
</details>

## v0.2.5 (2015-04-02)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release over the course of 1 calendar day.
 - 2 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.5 ([`295ddba`](https://github.com/Byron/flate2-rs/commit/295ddba78f34d7de40d932139a79d358128e4210))
    - Bump dep on rand ([`2e8d3c0`](https://github.com/Byron/flate2-rs/commit/2e8d3c0cb2f055d059aac1322a04afbaff121d45))
</details>

## v0.2.4 (2015-03-31)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 4 commits contributed to the release.
 - 1 day passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.4 ([`beaeb05`](https://github.com/Byron/flate2-rs/commit/beaeb05188c3147cc082f0ae973deca6273897d9))
    - Ensure Ok(0) isn't spuriously returned on write() ([`62d5546`](https://github.com/Byron/flate2-rs/commit/62d5546c6fdb858551ba380ac86ac6a769e8a970))
    - Flush the entire buffer on flush() ([`ad9cdc4`](https://github.com/Byron/flate2-rs/commit/ad9cdc4bbb401e50b961cbbdb60894cd27a0fc4f))
    - Remove unsafe_destructor ([`e2b43cb`](https://github.com/Byron/flate2-rs/commit/e2b43cbbb7bca66f6cb0ae18b88fa92991c0ba55))
</details>

## v0.2.3 (2015-03-29)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 1 commit contributed to the release.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.2.3 ([`8feaf0c`](https://github.com/Byron/flate2-rs/commit/8feaf0c0a994c6001bc75808ed38806033a72fcf))
</details>

## v0.2.2 (2015-03-28)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 24 commits contributed to the release over the course of 51 calendar days.
 - 51 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Fix gzipp'ing small files, write the raw header ([`328a90d`](https://github.com/Byron/flate2-rs/commit/328a90d24ce1de08f97f43699b6164b831526e39))
    - Tweak TOML layout ([`f2ccc8e`](https://github.com/Byron/flate2-rs/commit/f2ccc8e5f7120f7c09aca1d20b25f1e22d7316c6))
    - Fix lint warnings/errors ([`3e2ebd1`](https://github.com/Byron/flate2-rs/commit/3e2ebd1144d682ff9664ceaeb9972c926de9173a))
    - Bump version numbers ([`c59ddd6`](https://github.com/Byron/flate2-rs/commit/c59ddd6d33bef7dc44e115683e90754132ed312b))
    - Bump dep on rand ([`6693281`](https://github.com/Byron/flate2-rs/commit/66932812bbc98242a6ff1e8d21c015147b3523bb))
    - Fix readme examples ([`5be934f`](https://github.com/Byron/flate2-rs/commit/5be934ff6e31bdd024f3bcf519dee3ae54982bb2))
    - Update to rust master ([`3ad1311`](https://github.com/Byron/flate2-rs/commit/3ad131174262a36e10f26807af617a8e55cded89))
    - Merge pull request #18 from o01eg/patch-1 ([`3fdbfac`](https://github.com/Byron/flate2-rs/commit/3fdbfac0aede4480c38d0a0645887b0bcd7da4e8))
    - Upgrade to rustc 1.0.0-dev (68d694156 2015-03-20) ([`51c6ee3`](https://github.com/Byron/flate2-rs/commit/51c6ee3c30ba463f516d6964812cde3f0044145f))
    - Consolidate feature tags ([`f336c68`](https://github.com/Byron/flate2-rs/commit/f336c6815e8d4cfffef84ca50a735dc4be8a36c0))
    - Remove usage of the collections feature ([`931360f`](https://github.com/Byron/flate2-rs/commit/931360fb2bba4656aa944f1a1e91d1ee1c5a5033))
    - Remove usage of the `core` feature ([`04555a3`](https://github.com/Byron/flate2-rs/commit/04555a3178640c7a69cca3541d83802c8d5b18b7))
    - Update to cargo master ([`cf869fa`](https://github.com/Byron/flate2-rs/commit/cf869faaae0bb6f232d90caad076adda9ee8d898))
    - Update to rust master ([`c6dca76`](https://github.com/Byron/flate2-rs/commit/c6dca76ffe4a2636fb7425894ccc9c7dbee12e18))
    - Add doc urls ([`f93b9cc`](https://github.com/Byron/flate2-rs/commit/f93b9cc6ec8dc0f42e1049017d08f02f670b2445))
    - Add back deny(warnings) ([`a53d51f`](https://github.com/Byron/flate2-rs/commit/a53d51f020e1da23adcb9123cefc24e743b82ad5))
    - Refactor to make calls to miniz more explicit ([`6360d3e`](https://github.com/Byron/flate2-rs/commit/6360d3e22011a71092bc0c90725827f504f927c5))
    - Don't call read_to_end to read a block of data ([`2077908`](https://github.com/Byron/flate2-rs/commit/207790870b8639ce41ed609be20b991c3c37e764))
    - Bump miniz-sys to 0.1.3 ([`b7e97dd`](https://github.com/Byron/flate2-rs/commit/b7e97dd490dc42e6ad7d03c1e137c818104fa8b5))
    - Test and update the README ([`09caa5a`](https://github.com/Byron/flate2-rs/commit/09caa5a37cfee8583d8df7f2f807c9b8f6a4821a))
    - Port to new I/O libs ([`88cb8cf`](https://github.com/Byron/flate2-rs/commit/88cb8cfc12bd5990d846851482b83450822f8703))
    - Update to rust master ([`6430b38`](https://github.com/Byron/flate2-rs/commit/6430b38674655039cbe63e5b64320ff9afec45b8))
    - Update to rust master ([`9e8b4b4`](https://github.com/Byron/flate2-rs/commit/9e8b4b432bcb24b048bf22d1b7b87d03e3ed4163))
    - Stop using BytesContainer ([`b45ad35`](https://github.com/Byron/flate2-rs/commit/b45ad35f456539c28dbb77474d34ea4a3deb7d68))
</details>

## v0.1.8 (2015-02-05)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 4 commits contributed to the release over the course of 3 calendar days.
 - 7 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.1.8 ([`4f01cf8`](https://github.com/Byron/flate2-rs/commit/4f01cf872a778509cd2f397a6e69c0af009ba89e))
    - Bump to 0.1.2 ([`4a60929`](https://github.com/Byron/flate2-rs/commit/4a609296eb968976234fb1118d466690f8381a69))
    - Update to rust master ([`4033d52`](https://github.com/Byron/flate2-rs/commit/4033d52f9db6e7c1b39267159d84e9a961f57cf8))
    - Fix examples in the README ([`64361b7`](https://github.com/Byron/flate2-rs/commit/64361b79727e95b245b40a4421a19e5e533e002b))
</details>

## v0.1.7 (2015-01-28)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release.
 - 5 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.1.7 ([`e5127c1`](https://github.com/Byron/flate2-rs/commit/e5127c110b5af0840daea3140fccd92b742084a7))
    - Depend on crates.io libc ([`07edc08`](https://github.com/Byron/flate2-rs/commit/07edc0890be6966179012160834c8d05fddfebfd))
</details>

## v0.1.6 (2015-01-23)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 2 commits contributed to the release over the course of 11 calendar days.
 - 13 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.1.6 ([`216917d`](https://github.com/Byron/flate2-rs/commit/216917d40a2d4649574bb428604bf630d9119714))
    - Tweak travis config ([`d7a235f`](https://github.com/Byron/flate2-rs/commit/d7a235fd9871b15fe5087c26503e07fc3e9afa17))
</details>

## v0.1.5 (2015-01-09)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 1 commit contributed to the release.
 - 2 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.1.5 ([`7ed0ea7`](https://github.com/Byron/flate2-rs/commit/7ed0ea722fb856892466dd5d953b20d31adcb825))
</details>

## v0.1.4 (2015-01-07)

<csr-id-7e0258021b17be2f0800e8c36117b9e1c2f9eb0e/>

### Other

 - <csr-id-7e0258021b17be2f0800e8c36117b9e1c2f9eb0e/> add support for checksumming the header

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 13 commits contributed to the release over the course of 37 calendar days.
 - 40 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump to 0.1.4 ([`deb5db3`](https://github.com/Byron/flate2-rs/commit/deb5db39ea9324cbd0773f36dabb7a34eb51213b))
    - Update to rust master ([`30a238f`](https://github.com/Byron/flate2-rs/commit/30a238f6a95390d81790ccfe21efe9aa942cd97a))
    - Finish update to rust master ([`c5176c2`](https://github.com/Byron/flate2-rs/commit/c5176c2069a03211fdc3cd15ce17c74e68a79f61))
    - Updates to last rust compiler version ([`fbbea68`](https://github.com/Byron/flate2-rs/commit/fbbea6833cba8166a5e0ecdfaf722a87a59fe28a))
    - Update to rust master ([`57afd45`](https://github.com/Byron/flate2-rs/commit/57afd45e49f444e597d91ff9ec4d77fa0964363c))
    - Bump version, tweak deps ([`5c571b5`](https://github.com/Byron/flate2-rs/commit/5c571b5bae2eccc673b06f5e4f503ab30bf5698d))
    - Update to rust master ([`c8ecf7a`](https://github.com/Byron/flate2-rs/commit/c8ecf7a411bc6d43bf885f487c01e536490f2aea))
    - Update miniz and tweak some tests ([`5cf5e4f`](https://github.com/Byron/flate2-rs/commit/5cf5e4f57166053590a7f63507f65da72d4e74eb))
    - Fix checksum of gzip files ([`44a46c3`](https://github.com/Byron/flate2-rs/commit/44a46c32c0f685005232d20daa34196329e3ea6f))
    - Add unit test for extraction of gzip files ([`2497964`](https://github.com/Byron/flate2-rs/commit/24979640a880a054364b4465443c0532bce3d4ff))
    - Tidy up the style throughout ([`861f52e`](https://github.com/Byron/flate2-rs/commit/861f52e4c71681f96605dd65cb7c1c9f6e6ecc3c))
    - Add support for checksumming the header ([`7e02580`](https://github.com/Byron/flate2-rs/commit/7e0258021b17be2f0800e8c36117b9e1c2f9eb0e))
    - Rename unwrap() -> into_inner() ([`bb4d4c4`](https://github.com/Byron/flate2-rs/commit/bb4d4c4836e313a8d0d4976d2f65ad21a4542259))
</details>

## v0.1.0 (2014-11-27)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 57 commits contributed to the release over the course of 133 calendar days.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Bump versions to 0.1.0 ([`f9ab9da`](https://github.com/Byron/flate2-rs/commit/f9ab9da89fec6e18e6d3544be2a93b4f3eaa5dbe))
    - Bump version numbers ([`044e4dc`](https://github.com/Byron/flate2-rs/commit/044e4dc2a766bc32fd5ae76bbe958be6e85be481))
    - Merge pull request #6 from mvdnes/add_ref ([`b0cdf68`](https://github.com/Byron/flate2-rs/commit/b0cdf688c92ebd8bf0ffdb1240718eb09434f658))
    - Add reference operator to arguments as needed ([`d6537a3`](https://github.com/Byron/flate2-rs/commit/d6537a3a2f5b5730aaa21c0dda5683c9b13a82a7))
    - Bump version number ([`cbac14c`](https://github.com/Byron/flate2-rs/commit/cbac14cec83d22a380280a729b5eacc1873e34c9))
    - Update tests to rust master ([`f81e6dc`](https://github.com/Byron/flate2-rs/commit/f81e6dc401b2bf32c8274fa1b8327699fb9345c0))
    - Merge pull request #5 from CraZySacX/master ([`78edf36`](https://github.com/Byron/flate2-rs/commit/78edf365729a7d2953a3bc70b62b453d8327a61f))
    - Fix for rust commit 3dcd2157403163789aaf21a9ab3c4d30a7c6494d 'Switch to purely namespaced enums' ([`dc7747f`](https://github.com/Byron/flate2-rs/commit/dc7747f974bda1b577ee6f4703a7f5a1fc3b6188))
    - Don't test the README for now ([`fc04214`](https://github.com/Byron/flate2-rs/commit/fc04214981c39633eb3859bd28389fc448d0e9fc))
    - Remove an explicit link annotation ([`1acf048`](https://github.com/Byron/flate2-rs/commit/1acf048e5cf989926307ef07b4f866e4170889e3))
    - Merge branch 'build-cmd' ([`7f7de48`](https://github.com/Byron/flate2-rs/commit/7f7de48865e12d6bda0af7d8060dab0a6827c4f0))
    - Update windows triples ([`31d9253`](https://github.com/Byron/flate2-rs/commit/31d9253355ba837cd62487b8766bf2be89f334a3))
    - Update gcc-rs ([`f4cee48`](https://github.com/Byron/flate2-rs/commit/f4cee48bc1e82fc6d8278147e661c8057c7be464))
    - Building with a build command! ([`310eb2b`](https://github.com/Byron/flate2-rs/commit/310eb2b60adbc3aac9841fb69258f0f0d3b1c23e))
    - Update to rust master ([`67be375`](https://github.com/Byron/flate2-rs/commit/67be37548937c059370ce272efe1142fbf198d67))
    - Merge pull request #4 from CraZySacX/master ([`d497f69`](https://github.com/Byron/flate2-rs/commit/d497f699805829f8a6ee62ad7a6b0826de7d55fd))
    - Fix for new BytesReader and AsRefReader traits introduced in recent rust commit ([`39e9769`](https://github.com/Byron/flate2-rs/commit/39e9769d5164267a2d9f954b755ef4e83a9e9496))
    - Merge pull request #3 from steveklabnik/master ([`f74a1f6`](https://github.com/Byron/flate2-rs/commit/f74a1f632c09e4f241ecb638e331c2cec4e955e0))
    - Fail -> panic ([`3820047`](https://github.com/Byron/flate2-rs/commit/38200479e7493cf49b8686026c7e789d4d228035))
    - Don't email on successful travis builds ([`b66f963`](https://github.com/Byron/flate2-rs/commit/b66f9636aa0ccd76fb886d3412984e5185538da4))
    - Specify the readme as well ([`9dbcc58`](https://github.com/Byron/flate2-rs/commit/9dbcc5834fd564a89ebb2b14080caf6d5dad66b6))
    - Add some cargo metadata ([`d6e3c95`](https://github.com/Byron/flate2-rs/commit/d6e3c95809f271ed2552c6a92b22fea460df010e))
    - Prepare for s/static/const ([`68971ae`](https://github.com/Byron/flate2-rs/commit/68971ae77a523c7ec3f19b4bcd195f76291ea390))
    - Tweak build scripts ([`9be9775`](https://github.com/Byron/flate2-rs/commit/9be9775d3f50e961a2f99728b3de8d819f3939be))
    - Update travis config ([`af5bb8b`](https://github.com/Byron/flate2-rs/commit/af5bb8b38f3899a9594934d432f2bcbc1a7a46a0))
    - Merge pull request #2 from ebfe/freebsd ([`801f490`](https://github.com/Byron/flate2-rs/commit/801f4900fe40b8773ebf2e73efa2d9ef827c95be))
    - Fix build on FreeBSD ([`e95b6da`](https://github.com/Byron/flate2-rs/commit/e95b6daa7c5e9b5ee631a9dcc9cc9de9b32ec85a))
    - Merge pull request #1 from japaric/arm ([`210310d`](https://github.com/Byron/flate2-rs/commit/210310d36c5c233d24c616a91e7c7c7b15b04209))
    - Don't pass the -m64 flag to CC when building on ARM ([`56871cd`](https://github.com/Byron/flate2-rs/commit/56871cdf10a7f536472275c438d987b6036b88a1))
    - Upload docs to Rust CI as well ([`2730aea`](https://github.com/Byron/flate2-rs/commit/2730aeaa87c04bef814d1c1fd5ba7bed7cbc5a4d))
    - Test the README as well ([`f62f2f2`](https://github.com/Byron/flate2-rs/commit/f62f2f23956b6c53835a9be1a88f0e1c22c813af))
    - Update to master ([`1551287`](https://github.com/Byron/flate2-rs/commit/1551287b1bd029b21e50c3916c8c51d259851478))
    - Update rustup URL ([`6a182b2`](https://github.com/Byron/flate2-rs/commit/6a182b2c22eb8e067f347837af0e7f2fa60ce3e0))
    - Update to rust master ([`a59b2a1`](https://github.com/Byron/flate2-rs/commit/a59b2a103642550bc1500c302c5031479ec7d9e1))
    - Add a dual Apache/MIT license ([`2ccf4dc`](https://github.com/Byron/flate2-rs/commit/2ccf4dc3cb613446c6f80fbe1d6950875417de29))
    - Fix build on windows ([`12593d1`](https://github.com/Byron/flate2-rs/commit/12593d1b9ccf09c2eabac176a6e233b171eed843))
    - Update travis config ([`9b527e7`](https://github.com/Byron/flate2-rs/commit/9b527e79f35f54b3648f365638cd10278e4944aa))
    - Update the README ([`6e56062`](https://github.com/Byron/flate2-rs/commit/6e56062ddaf7d99d698cb31d12b75ea4827ace79))
    - Restructure the build directories for miniz ([`67f2241`](https://github.com/Byron/flate2-rs/commit/67f22411bc8ea4f7f7a67c5553658997d7cf92ca))
    - Update to master ([`a2d1c2a`](https://github.com/Byron/flate2-rs/commit/a2d1c2aed28ce083e105ee6a13269177829b569d))
    - Crargo now runs doc tests ([`ac04f6e`](https://github.com/Byron/flate2-rs/commit/ac04f6e986369754cbf0a41fb175486948ba2822))
    - Add documentation to travis ([`bd6433a`](https://github.com/Byron/flate2-rs/commit/bd6433aecc4bcd42cc5c2314d1591a15eeeef4f9))
    - Update the README ([`5b5472b`](https://github.com/Byron/flate2-rs/commit/5b5472b5027e05e52715996ff60279f007f4aa39))
    - Reorganize a few internals, expose some helper traits ([`a63f86e`](https://github.com/Byron/flate2-rs/commit/a63f86e5d9a8d759159facb9665d04d452b742ae))
    - Bring the gz module into line with flate/zlib ([`a942a88`](https://github.com/Byron/flate2-rs/commit/a942a88815265cdac7c00fdafbfd3db8e5a6b92c))
    - Add a Builder for gzip encoders ([`9b6f716`](https://github.com/Byron/flate2-rs/commit/9b6f716490193479e2e17a0ef1b32c536f585d26))
    - Implement the other direction of encoder/decoder ([`240b27c`](https://github.com/Byron/flate2-rs/commit/240b27cd972a945f14253dc8a7fb3a6d1d192a56))
    - It would probably help if we build with optimizations ([`3a5e816`](https://github.com/Byron/flate2-rs/commit/3a5e81646aaeeeb4e1c646840c31483ee12fe58d))
    - Finish looping when flusing the stream ([`ec253ba`](https://github.com/Byron/flate2-rs/commit/ec253ba93f7a18bb54fdb2acc07a95a29dff4717))
    - Fix a bug in encoding, truncating output too much ([`d06d94e`](https://github.com/Byron/flate2-rs/commit/d06d94e1a146c74e3950f8febdbebd251dcc1874))
    - Enable cross-compilation to 32-bit ([`7b130d2`](https://github.com/Byron/flate2-rs/commit/7b130d27de0a13297fec8b24533f89d64d6f297b))
    - Separate zlib/deflate modules ([`f64c936`](https://github.com/Byron/flate2-rs/commit/f64c9361d28c0dafaa3d6f26161c2d814a292b2b))
    - Adding some gzip examples ([`47ee727`](https://github.com/Byron/flate2-rs/commit/47ee727ae74d6bf959e69024250b91be66f5e4c1))
    - Gzip streams have no zlib header/footer ([`18ece25`](https://github.com/Byron/flate2-rs/commit/18ece25ea0505a08c86cc8629e096e0705798003))
    - Add gzip compression/decompresion ([`c3a1493`](https://github.com/Byron/flate2-rs/commit/c3a1493ae247edcf8ceb0be6390c142a50596a87))
    - Add a small README ([`8941ddd`](https://github.com/Byron/flate2-rs/commit/8941dddc0e265af54599e3a26132536ff06f56d3))
    - Initial commit ([`01c8e0d`](https://github.com/Byron/flate2-rs/commit/01c8e0dfa6b81d24df54d890deb2a18dbf0ce8e3))
</details>


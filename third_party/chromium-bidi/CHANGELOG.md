# Changelog

All notable changes to this project will be documented in this file.

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

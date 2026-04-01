# AI Prompt API Legacy Params Virtual Test Suite

This virtual test suite enables `AIPromptAPILegacyParams` and disables `AIPromptAPIParams` to verify that legacy API parameter surfaces for Extensions (i.e. `params()`, `topK`, and `temperature`) trigger developer warnings over the inspector protocol.

## Runtime Flags

- `--disable-blink-features=AIPromptAPIParams`
- `--enable-blink-features=AIPromptAPILegacyParams`

## Owners

- msw@chromium.org
- iahouma@google.com

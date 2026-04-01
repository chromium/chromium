# AI Prompt API Legacy Params Disabled Virtual Test Suite

This virtual test suite disables experimental `AIPromptAPILegacyParams` and `AIPromptAPIParams` features to ensure that legacy API parameter surfaces for Extensions (i.e. `params()`, `topK`, and `temperature`) are undefined and options are safely ignored.

## Runtime Flags

- `--disable-blink-features=AIPromptAPILegacyParams,AIPromptAPIParams`

## Owners

- msw@chromium.org
- iahouma@google.com

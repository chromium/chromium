> [!NOTE]
> If you are viewing this on GitHub, the source was moved to Chromium. Please contribute there.

# How to Contribute

The `chromium-bidi` source code lives in the Chromium repository under `third_party/chromium-bidi`.
All active development and contributions take place in Chromium.

## Contributing to Chromium

Please follow the [Chromium Contributing Guide](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md) to set up your environment, create changes, and upload code reviews (CLs).

## Contributor License Agreement

Contributions to Chromium must be accompanied by a Contributor License Agreement (CLA). Head over to <https://cla.developers.google.com/> to see your current agreements on file or to sign a new one.

## Code Reviews

All submissions require review via Chromium's code review system (Gerrit). Use `git cl upload` to submit patches for review.

## Community Guidelines

This project follows
[Google's Open Source Community Guidelines](https://opensource.google/conduct/).

## Adding commands

The BiDi commands are processed in `src/bidiMapper/CommandProcessor.ts`.
To add a new command, add it to `_processCommand`, write and call the module processor for it.

# Symphonia RIFF (AIFF, AVI, WAVE) Demuxer

[<img alt="Docs.rs" src="https://img.shields.io/badge/docs.rs-symphonia_format_riff-brightgreen?style=for-the-badge" height="22"/>](https://docs.rs/symphonia-format-riff)

AIFF/AVI/WAVE demuxer for Project Symphonia.

> [!NOTE]
> This crate is part of Symphonia. Please use the [`symphonia`](https://crates.io/crates/symphonia) crate instead of this one directly.

## Support

This crate supports demuxing media containers based off the Resource Interchange File Format (RIFF). Specific format support may be enabled or disabled using feature flags. However, by default, all formats are enabled.

| Format | Feature Flag | Default |
|--------|--------------|---------|
| AIFF   | `aiff`       | Yes     |
| WAVE   | `wav`        | Yes     |

## License

Symphonia is provided under the MPL v2.0 license. Please refer to the LICENSE file for more details.

## Contributing

Symphonia is a free and open-source project that welcomes contributions! To get started, please read our [Contribution Guidelines](https://github.com/pdeljanov/Symphonia/blob/main/CONTRIBUTING.md).

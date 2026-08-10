# media-caption-settings-button-disabled

This directory contains media controls tests that run with the
MediaCaptionSettingsButton runtime feature disabled.

The feature adds a "Caption settings" entry to the text track list, which also
changes the closed captions button to always open the text track list instead of
directly toggling a lone text track. Web tests run with experimental web
platform features enabled by default, so the base tests cover the enabled case
and this suite covers the disabled case.

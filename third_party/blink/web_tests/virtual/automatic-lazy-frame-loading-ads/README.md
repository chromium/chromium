This virtual suite runs automatic lazy frame loading tests. When the flag
`AutomaticLazyFrameLoadingToAds` is enabled, third party frames which urls are matched by [subresource_filter](https://chromium.googlesource.com/chromium/src.git/+/main/components/subresource_filter/README.md) is lazily loaded automatically.

Bug: crbug.com/1265343

# Virtual Test Suite: jxl-disabled

This virtual test suite runs tests with the JXL (JPEG XL) image format feature
disabled via `--disable-features=JXLImageFormat`.

This covers the kill switch configuration, where `image/jxl` is omitted from
image and navigation request Accept headers and JXL images are not decoded.

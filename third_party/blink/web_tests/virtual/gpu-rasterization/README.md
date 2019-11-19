# Test suite to run images/ with GPU rasterization and with the flag 
# --enable-features=DecodeLossyWebPImagesToYUV until it is enabled by
# default.
# 
# This allows YUV decoding and rasterization for images that would support it:
# currently just lossy WebP images without alpha that have been fully loaded at
# decode time. Because incremental YUV decoding hasn't been implemented
# (crbug.com/943519), the tests that go down this path also use JavaScript to
# ensure all image data has been received before we attempt decoding.

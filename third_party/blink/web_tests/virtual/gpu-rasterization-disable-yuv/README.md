# Test suite to run images/yuv-decode-eligible with GPU rasterization
# and with the DecodeLossyWebPImagesToYUV feature disabled. This is because
# YUV decoding will eventually become the default for many image decoding
# cases and receives test coverage through virtual/gpu-rasterization. However
# there are some cases where images might not go through YUV decoding, so this
# test suite maintains coverage of those.

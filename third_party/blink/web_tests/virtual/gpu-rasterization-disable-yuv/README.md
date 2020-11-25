Test suite to run images/yuv-decode-eligible with --enable-gpu-rasterization
--disable-blink-features=DecodeLossyWebPImagesToYUV and
--disable-blink-features=DecodeJpeg420ImagesToYUV. 

YUV decoding is the default for many image decoding cases and receives test
coverage through virtual/gpu-rasterization. This suite maintains coverage for
images that do not use YUV decoding.

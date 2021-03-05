Test suite to run images/ with --enable-gpu-rasterization.

This suite tests YUV decoding and rasterization for images that have been fully
loaded at decode time and formats that support it, e.g.
* lossy WebP images without alpha and
* JPEG YUV images with a 4:4:4, 4:2:2 or 4:2:0 subsampling.

Because incremental YUV decoding hasn't been implemented (crbug.com/943519), the
WebP tests use JavaScript to ensure all image data has been received before we
attempt decoding. TODO(crbug.com/730267): Verify JPEG tests always go through
YUV decoding.

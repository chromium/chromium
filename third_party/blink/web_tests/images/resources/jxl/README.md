# JPEG XL Test files

## How to generate the test set

We assume to have a the following pngs (from `third_party/blink/web_tests/images/resources/`) available:
```
red-10.png
green-10.png
blue-10.png
png_per_row_alpha.png
icc-v2-gbr.jpg
```
Then we run:
```
cjxl red-10.png red-10-default.jxl
cjxl --container red-10.png red-10-container.jxl
cjxl -d 0 red-10.png red-10-lossless.jxl
cjxl -d 0 green-10.png green-10-lossless.jxl
cjxl -d 0 blue-10.png blue-10-lossless.jxl
cjxl -d 0 png_per_row_alpha.png alpha-lossless.jxl
cjxl icc-v2-gbr.jpg icc-v2-gbr.jxl
```

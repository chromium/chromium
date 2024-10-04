# AVIF Test Files

[TOC]

## Instructions

To add, update or remove a test file, please update the list below.

Please provide full reference and steps to generate the test file so that
any people can regenerate or update the file in the future.

## List of Test Files

### red-(full|limited)-range-(420|422|444)-(8|10|12)bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

Limited:
```
avifenc -r l -d  8 -y 420 -s 0 red.png red-limited-range-420-8bpc.avif
avifenc -r l -d 10 -y 420 -s 0 red.png red-limited-range-420-10bpc.avif
avifenc -r l -d 12 -y 420 -s 0 red.png red-limited-range-420-12bpc.avif
avifenc -r l -d  8 -y 422 -s 0 red.png red-limited-range-422-8bpc.avif
avifenc -r l -d 10 -y 422 -s 0 red.png red-limited-range-422-10bpc.avif
avifenc -r l -d 12 -y 422 -s 0 red.png red-limited-range-422-12bpc.avif
avifenc -r l -d  8 -y 444 -s 0 red.png red-limited-range-444-8bpc.avif
avifenc -r l -d 10 -y 444 -s 0 red.png red-limited-range-444-10bpc.avif
avifenc -r l -d 12 -y 444 -s 0 red.png red-limited-range-444-12bpc.avif
```

Full:
```
avifenc -r f -d  8 -y 420 -s 0 red.png red-full-range-420-8bpc.avif
avifenc -r f -d 10 -y 420 -s 0 red.png red-full-range-420-10bpc.avif
avifenc -r f -d 12 -y 420 -s 0 red.png red-full-range-420-12bpc.avif
avifenc -r f -d  8 -y 422 -s 0 red.png red-full-range-422-8bpc.avif
avifenc -r f -d 10 -y 422 -s 0 red.png red-full-range-422-10bpc.avif
avifenc -r f -d 12 -y 422 -s 0 red.png red-full-range-422-12bpc.avif
avifenc -r f -d  8 -y 444 -s 0 red.png red-full-range-444-8bpc.avif
avifenc -r f -d 10 -y 444 -s 0 red.png red-full-range-444-10bpc.avif
avifenc -r f -d 12 -y 444 -s 0 red.png red-full-range-444-12bpc.avif
```

### red-full-range-(bt709|bt2020)-(pq|hlg)-444-(10|12)bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

BT.709:
```
avifenc -r f -d  8 -y 444 -s 0 --nclx 1/1/1 red.png red-full-range-bt709-444-8bpc.avif
```

PQ:
```
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/16/9 red.png red-full-range-bt2020-pq-444-10bpc.avif
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/16/9 red.png red-full-range-bt2020-pq-444-12bpc.avif
```

HLG:
```
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/18/9 red.png red-full-range-bt2020-hlg-444-10bpc.avif
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/18/9 red.png red-full-range-bt2020-hlg-444-12bpc.avif
```

Unspecified color primaries, transfer characteristics, and matrix coefficients:
```
avifenc -r f -d  8 -y 420 -s 0 --nclx 2/2/2 red.png red-full-range-unspecified-420-8bpc.avif
```

### silver-full-range-srgb-420-8bpc.avif
This is generated from silver.png (3x3 rgb(192, 192, 192)) with the appropriate
avifenc command line:

```
avifenc -r f -d  8 -y 420 -s 0 --nclx 1/13/1 silver.png silver-full-range-srgb-420-8bpc.avif
```

### silver-400-matrix-6.avif
This is generated from silver.png (3x3 rgb(192, 192, 192)) with the appropriate
avifenc command line:

```
avifenc -s 0 -q 99 -y 400 silver.png silver-400-matrix-6.avif
```

### silver-400-matrix-0.avif
This is generated from silver.png (3x3 rgb(192, 192, 192)) with the appropriate
avifenc command line:

```
avifenc -s 0 -q 99 -y 400 --cicp 1/13/0 silver.png silver-400-matrix-0.avif
```

### red-full-range-angle-(0|1|2|3)-mode-(0|1)-420-8bpc.avif
These are all generated from red.png with the appropriate avifenc command line:

```
avifenc -r f -d  8 -y 420 -s 0 --irot 1 red.png red-full-range-angle-1-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --imir 0 red.png red-full-range-mode-0-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --imir 1 red.png red-full-range-mode-1-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --irot 2 --imir 0 red.png red-full-range-angle-2-mode-0-420-8bpc.avif
avifenc -r f -d  8 -y 420 -s 0 --irot 3 --imir 1 red.png red-full-range-angle-3-mode-1-420-8bpc.avif
```

### tiger_3layer_1res.avif and tiger_3layer_3res.avif
These are copied from https://github.com/AOMediaCodec/av1-avif/tree/master/testFiles/Xiph
under the CC BY-SA 3.0 license.

### tiger_420_8b_grid1x13.avif
This is generated from `tiger_3layer_1res.avif` (CC BY-SA 3.0 license) with the
appropriate avifdec and avifenc command line:

```
avifdec tiger_3layer_1res.avif tiger.y4m && avifenc --grid 1x13 tiger.y4m tiger_420_8b_grid1x13.avif
```

### gracehopper_422_12b_grid2x4.avif
This is generated from `gracehopper.png` with the appropriate avifenc command
line:

```
avifenc --yuv 422 --depth 12 --grid 2x4 ../gracehopper.png gracehopper_422_12b_grid2x4.avif
```

### dice_444_10b_grid4x3.avif
This is generated from `dice.png` with the appropriate avifenc command line:

```
avifenc --yuv 444 --depth 10 --grid 4x3 ../dice.png dice_444_10b_grid4x3.avif
```

### green-no-alpha-ispe.avif
The alpha auxiliary image item in this file does not have a mandatory ispe
property. This is generated from green-alpha.png with a patched cavif encoder.
The following commit should be reverted in avif-serialize:
https://github.com/kornelski/avif-serialize/commit/451ff3568fa2de2fad1185654576f8f4a2db13c5

The cavif command line is:

```
cavif ../../../fast/box-shadow/resources/green-alpha.png -o green-no-alpha-ispe.avif
```

### red-unsupported-transfer.avif
The 'colr' property in this file has an unsupported transfer function (11) in
'nclx'. This is generated from red.png with the appropriate avifenc command
line:
```
avifenc -r f -d 8 -y 420 -s 0 --nclx 1/11/1 red.png red-unsupported-transfer.avif
```

### blue-and-magenta-crop.avif
This image uses a 'clap' (clean aperture) image property to crop the image to
contain the blue rectangle only (with a magenta rectangle inside).

### blue-and-magenta-crop-invalid.avif
This image is the same as blue-and-magenta-crop.avif except that the fractions
horizOff and vertOff have positive numerators and negative denominators (30/-1
and 10/-1 instead of -30/1 and -10/1). Changed with a hex editor.

### red-and-purple-crop.avif
The input PNG image (300x100) has a red, purple, and blue square (100x100
each). It was created with ImageMagick's 'convert' command:

  convert -size 3x1 xc:red -matte -fill '#0000FF80' \
          -draw 'point 1,0' \
          -draw 'color 2,0 point' -scale 300x300 draw_points_300x100.png

This image (200x50) uses a 'clap' (clean aperture) image property to crop the
input image to contain half of the red and purple squares.

  avifenc -l --crop 0,0,200,50 draw_points_300x100.png red-and-purple-crop.avif

Note that the origin of the crop rectangle is at (0, 0).

### red-and-purple-and-blue.avif
The uncropped version (300x100) of red-and-purple-crop.avif:

  avifenc -l draw_points_300x100.png red-and-purple-and-blue.avif

### red-with-alpha-8bpc.avif
3x3, 8-bit, YUV 4:2:0.

Y: all 63. U: all 102. V: all 240. This is red in YUV, limited range, with
MatrixCoefficients=1.

A:   0,   0, 127
     0, 127, 255
   127, 255, 255

### small-with-gainmap.avif
A small image with a gainmap as an auxiliary item and XMP metadata, as in the
spec from Adobe https://helpx.adobe.com/camera-raw/using/gain-map.html

Source: the image was first created as a png with alpha, then encoded with a
tweaked avifenc that stores the alpha aux image with the gainmap identifier.
Then the file was edited with a hex editor to associate the xmp to the aux
image.

### small-with-gainmap-iso.avif
A small image with a gainmap as a "tmap" derived item, where the base is SDR.
Has use_base_color_space set to true. Both the base image and alternate image
are associated with the same ICC profile (P3).

The image was generated by converting
blink/web_tests/images/resources/gainmap-trattore0.jpg with `avifenc` built with
`AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP` enabled and libxml2 available (installed on
the system or with `AVIF_LOCAL_LIBXML2`).

```
avifenc  gainmap-trattore0.jpg --qgain-map 80 --qcolor 80 \
  small-with-gainmap-iso.avif
```

### small-with-gainmap-iso-gammazero.avif
A small image with a gainmap where the gain map gamma values are zero (invalid).

The image was generated the same way as `small-with-gainmap.avif` with an
avifenc modified to write 0 as the gamma numerator.

### small-with-gainmap-iso-hdrbase.avif
A small image with a gainmap as a "tmap" derived item, where the base is HDR.
Has use_base_color_space set to false. Has no ICC profile.

This image was generated with the following commands using avifgainmaputil
from libavif and the small-with-gainmap-iso.avif test file:

```
$ avifgainmaputil swapbase small-with-gainmap-iso.avif \
  small-with-gainmap-iso-hdrbase.avif --qgain-map 80 --qcolor 80 \
  --ignore-profile
```

### small-with-gainmap-iso-usealtcolorspace.avif

Image with a gainmap as a "tmap" derived item, where the base is SDR.
Has use_base_color_space set to false. Both the base image and alternate image
are associated with the same ICC profile (P3).

Generated by converting blink/web_tests/images/resources/gainmap-trattore0.jpg
with a modified avifenc at
https://github.com/AOMediaCodec/libavif/compare/main...maryla-uc:libavif:gainmaptestimages

```
$ avifenc  gainmap-trattore0.jpg --qgain-map 80 --qcolor 80 \
  --no-use-base-colorspace  small-with-gainmap-iso-usealtcolorspace.avif
```

### small-with-gainmap-iso-usealtcolorspace-differenticc.avif

Image with a gainmap as a "tmap" derived item., where the base is SDR.
Has use_base_color_space set to false. Both the base image and alternate image
are associated with different ICC profiles: sRGB for the base, P3 for the
alternate image.

Generated by converting blink/web_tests/images/resources/gainmap-trattore0.jpg
with a modified avifenc at
https://github.com/AOMediaCodec/libavif/compare/main...maryla-uc:libavif:gainmaptestimages

Also uses a P3 ICC profile extracted from gainmap-trattore0.jpg and another
profile (sRGB but could be any profile).

```
$ exiftool -icc_profile -b gainmap-trattore0.jpg > p3.icc
$ avifenc  gainmap-trattore0.jpg --qgain-map 80 --qcolor 80 \
  --icc libavif/tests/data/sRGB2014.icc  \
  --icc-alt p3.icc --no-use-base-colorspace \
  small-with-gainmap-iso-usealtcolorspace-differenticc.avif
```

### gainmap-sdr-srgb-to-hdr-wcg-rec2020.avif

Image with a gainmap as a "tmap" derived item. Has use_base_color_space set to
false. There is no ICC profile but the base and alternate images have different
CICP values.

Generated with `avifgainmaputil` and test images from the libavif repository:

```
$ avifgainmaputil combine  libavif/tests/data/colors_text_sdr_srgb.avif \
  libavif/tests/data/colors_text_wcg_hdr_rec2020.avif \
  gainmap-sdr-srgb-to-hdr-wcg-rec2020.avif -q 90 --qgain-map 90
```

### TODO(crbug.com/960620): Figure out how the rest of files were generated.

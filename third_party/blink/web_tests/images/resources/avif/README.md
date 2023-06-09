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

### TODO(crbug.com/960620): Figure out how the rest of files were generated.

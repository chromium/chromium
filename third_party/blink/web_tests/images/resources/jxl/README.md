# JPEG XL Test files

## How to generate the test set

We assume to have a the following images (from
`third_party/blink/web_tests/images/resources/`) available:
```
red-10.png
green-10.png
blue-10.png
png_per_row_alpha.png
icc-v2-gbr.jpg
dice.png
animated.gif
jxl/3x3.png
jxl/3x3a.png
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
cjxl -d 0 dice.png alpha-large-dice.jxl

cjxl 3x3.png temp.jxl -d 0
djxl temp.jxl 3x3_srgb.png
cjxl 3x3_srgb.png 3x3_srgb_lossy.jxl -d 0.1 -s 3
cjxl 3x3_srgb.png 3x3_srgb_lossless.jxl -d 0

cjxl 3x3a.png temp.jxl -d 0
djxl temp.jxl 3x3a_srgb.png
cjxl 3x3a_srgb.png 3x3a_srgb_lossy.jxl -d 0.1 -s 3
cjxl 3x3a_srgb.png 3x3a_srgb_lossless.jxl -d 0

cjxl 3x3.png temp.jxl -x color_space=RGB_D65_202_Rel_PeQ -d 0
djxl temp.jxl 3x3_pq.png
cjxl 3x3_pq.png 3x3_pq_lossy.jxl -d 0.1 -s 3
cjxl 3x3_pq.png 3x3_pq_lossless.jxl -d 0

cjxl 3x3a.png temp.jxl -x color_space=RGB_D65_202_Rel_PeQ -d 0
djxl temp.jxl 3x3a_pq.png
cjxl 3x3a_pq.png 3x3a_pq_lossy.jxl -d 0.1 -s 3
cjxl 3x3a_pq.png 3x3a_pq_lossless.jxl -d 0

cjxl 3x3.png temp.jxl -x color_space=RGB_D65_202_Rel_HLG -d 0
djxl temp.jxl 3x3_hlg.png
cjxl 3x3_hlg.png 3x3_hlg_lossy.jxl -d 0.1 -s 3
cjxl 3x3_hlg.png 3x3_hlg_lossless.jxl -d 0

cjxl 3x3a.png temp.jxl -x color_space=RGB_D65_202_Rel_HLG -d 0
djxl temp.jxl 3x3a_hlg.png
cjxl 3x3a_hlg.png 3x3a_hlg_lossy.jxl -d 0.1 -s 3
cjxl 3x3a_hlg.png 3x3a_hlg_lossless.jxl -d 0

convert icc-v2-gbr.jpg icc-v2-gbr.icc
cjxl 3x3.png temp.jxl -x icc_pathname=icc-v2-gbr.icc -d 0
djxl temp.jxl 3x3_gbr.png
cjxl 3x3_gbr.png 3x3_gbr_lossy.jxl -d 0.1 -s 3
cjxl 3x3_gbr.png 3x3_gbr_lossless.jxl -d 0

cjxl 3x3a.png temp.jxl -x icc_pathname=icc-v2-gbr.icc -d 0
djxl temp.jxl 3x3a_gbr.png
cjxl 3x3a_gbr.png 3x3a_gbr_lossy.jxl -d 0.1 -s 3
cjxl 3x3a_gbr.png 3x3a_gbr_lossless.jxl -d 0

cjxl animated.gif animated.jxl

for i in $(seq 0 9); do J=$(printf '%03d' $i); convert -fill black -size 500x500 -font 'Courier' -pointsize 72 -gravity center label:$J $J.png; done
convert -delay 20 *.png count.gif
cjxl count.gif count.jxl
```

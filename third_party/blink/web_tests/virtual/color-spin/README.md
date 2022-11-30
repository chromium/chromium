# color-spin

This directory contains tests that run with a color profile that spin the primaries mode.
All tests in this directory run with the flag --force-color-profile=color-spin-gamma24

This color profile "spins" the colors:
 Red -> x chromacity of 0, y chromacity of 1, looking bluish
 Green -> x chromacity of 0, y chromacity of 0, looking redish
 Blue -> x chromacity of 1, y chromacity of 0, looking greenish.
 The White Point is not changed as the usual chromacity.

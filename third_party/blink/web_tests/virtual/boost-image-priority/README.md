This virtual suite runs tests with the `BoostImagePriority` expirement
disabled. The default config through fieldtrial_testing_config.json
turns on the feature which increases the priority of the first 5 images
to kMedium (breaking some test expectations). The virtual test suite is to
make sure the code path with the experiment disabled is still tested until
a ship decision is made.

Bug: crbug.com/1431169

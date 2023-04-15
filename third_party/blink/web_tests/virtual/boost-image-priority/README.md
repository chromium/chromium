This virtual suite runs tests with a boosted image priority. The flag
`BoostImagePriority` will be enabled which boosts the priority of the
first 5 not-small images to Medium (instead of Low) and updates the
load scheduler to always allow 1 Medium request to be in-flight in
tight mode.

Bug: crbug.com/1431169

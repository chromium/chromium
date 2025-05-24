Currently on the mac x86 macmini bots(mac_rel, mac*-blink-rel), some of the tests have floating point precision issue and are failing with error message like:
```
actual 47.25 should be close enough to expected 47.26926803588867 by ULP distance: expected a number less than or equal to 27n but got 5051n
```

This is not reproducible on intel macbooks. It's suspected to be an intel driver bug or a coreml bug on the particular hardware. Since it's only lossing some precision on a particular macmini configuration, we decide to leave the behavior as is. See more detail in crbug.com/331631226.

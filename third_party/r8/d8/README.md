Extra Copy of R8
================

In this directory we keep an extra copy of r8 that is infrequently updated.
This copy of r8 is used exclusively for dexing, since dexing (i.e. d8) code
does not change as frequently, it does not need to be rolled as often as the
rest of the code in r8. Every time it is rolled, all incremental dexing on dev
machines as well as bots is invalidated, which means that the next build will
need to do a full dex run, significantly slowing down bots that are already
slow (e.g. android-internal-rel and android-internal-dbg).

This extra copy of r8 can be updated by updating its entry in //DEPS with the
CIPD instance ID of the current //third_party/r8 entry.

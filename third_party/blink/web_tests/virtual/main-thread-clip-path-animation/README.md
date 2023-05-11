This test suite contains ALL tests that have compositable clip path animations,
even incidentally. This suite ensures that *main thread* clip path animations
do not lose coverage even if composited clip path animations are enabled by
default.
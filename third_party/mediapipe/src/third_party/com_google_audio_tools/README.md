# Multichannel Audio Tools

(This is not an official Google product!)

Multichannel Audio Tools contains common signal processing building blocks,
vectorized for multichannel processing using
[Eigen](http://www.eigen.tuxfamily.org/).

A non-exhaustive list of libraries in this repo:
- biquad filters
- ladder filters (with time-varying coefficients and enforced stability)
- filter design libraries
  - lowpass, highpass, etc.
  - 2 way crossover, N-way crossover
  - auditory cascade filterbank
  - parametric equalizer
  - perceptual loudness filters for implementing ITU standards
- dynamic range control
  - compression
  - limiter
  - noise gate
  - expanders
  - multiband dynamic range control
- envelope detectors
- gmock matchers for vector/Eigen types
- some other single-channel tools:
  - a fast rational factor resampler
  - spectrograms
  - a mel-frequency cepstral coefficient calculator

Contact multichannel-audio-tools-maintainers@google.com with questions/issues.

This library is intended to be built with [Bazel](https://bazel.build/). See
command below regarding running tests and building with the proper flags.
```
bazel test -c opt --cxxopt="-fext-numeric-literals" \
                  --cxxopt="-Wno-sign-compare" \
                  --cxxopt="-fpermissive" \
                  --cxxopt="-std=c++17" \
                  audio/... \
```

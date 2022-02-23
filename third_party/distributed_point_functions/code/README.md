# An Implementation of Incremental Distributed Point Functions in C++ [![Build status](https://badge.buildkite.com/64bb7c0fcc8c11d630517356b2c3932d7e14850801a5f22c48.svg?branch=master)](https://buildkite.com/bazel/google-distributed-point-functions)

This library contains an implementation of incremental distributed point
functions, based on the following paper:
> Boneh, D., Boyle, E., Corrigan-Gibbs, H., Gilboa, N., & Ishai, Y. (2020).
Lightweight Techniques for Private Heavy Hitters. arXiv preprint
> arXiv:2012.14884. https://arxiv.org/abs/2012.14884

## About Incremental Distributed Point Functions

A distributed point function (DPF) is parameterized by an index `alpha` and a
value `beta`. It consists of two algorithms: key generation and evaluation.
The key generation procedure produces two keys `k_a` and `k_b`, given `alpha`
and `beta`. Evaluating each key on any point `x` in the DPF domain results in an
additive secret share of `beta`, if `x == alpha`, and a share of 0 otherwise.

Incremental DPFs additionally can be evaluated on prefixes of the index domain.
More precisely, an incremental DPF is parameterized by a hierarchy of index
domains, each a power of two larger than the previous. Key generation now takes
a vector `beta`, one value `beta[i]` for each hierarchy level.
When evaluated on a `b`-bit prefix of `alpha`, where b is the log domain size of
the `i`-th hierarchy level, the incremental DPF returns a secret share of
`beta[i]`, otherwise a share of 0.

For more details, see the above paper, as well as the
[`DistributedPointFunction` class documentation](dpf/distributed_point_function.h).


## Building/Running Tests

This repository requires Bazel. You can install Bazel by
following the instructions for your platform on the
[Bazel website](https://docs.bazel.build/versions/master/install.html).

Once you have installed Bazel you can clone this repository and run all tests
that are included by navigating into the root folder and running:

```bash
bazel test //...
```

## Security
To report a security issue, please read [SECURITY.md](SECURITY.md).

## Disclaimer

This is not an officially supported Google product. The code is provided as-is,
with no guarantees of correctness or security.

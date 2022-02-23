
Same idea as (but implementation not directly based on) the Python shlex
module. However, this implementation does not support any of the Python
module's customization because it makes parsing slower and is fairly useless.
You only get the default settings of shlex.split, which mimic the POSIX shell:
<https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html>

This implementation also deviates from the Python version in not treating \r
specially, which I believe is more compliant.

The algorithms in this crate are oblivious to UTF-8 high bytes, so they iterate
over the bytes directly as a micro-optimization.

Disabling the `std` feature (which is enabled by default) will allow the crate
to work in `no_std` environments, where the `alloc` crate, and a global
allocator, are available.

# LICENSE

The source code in this repository is Licensed under either of
- Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
  https://www.apache.org/licenses/LICENSE-2.0)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or
  https://opensource.org/licenses/MIT)

at your option.

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.

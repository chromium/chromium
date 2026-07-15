# Where are the actual `third_party/crubit/support/...` headers?

Chromium clients can include and use Crubit support library
through the `#include "third_party/crubit/support/...` paths:

* Support library examples:
    - `#include "third_party/crubit/support/rs_std/option.h`
    - `#include "third_party/crubit/support/rs_std/slice_ref.h`
* Rust standard library bindings examples:
    - `#include "third_party/crubit/support/rs_std/rs_core.h"`
    - `#include "third_party/crubit/support/rs_std/rs_std.h"`

These `#include` paths work, even though Chromium repository
root doesn't contain `third_party/crubit/support` directory.
If you are trying to find the actual headers, then you need
to look in:

* Crubit sources distributed with the Rust toolchain:
  `third_party/rust-toolchain/lib/third_party/crubit/support/...`.
  (See also `include_dirs` defined in `build/rust/crubit/BUILD.gn`.)
* Build-time generated Crubit bindings for the Rust standard library:
  `out/.../gen/third_party/crubit/support/...`.
  (See also how `cpp_api_from_rust` and `h_out` attributes are used by
  `build/rust/std/rules/BUILD.gn` and `build/rust/std/BUILD.gn.hbs`).

Enabling `#include` paths that are unified across Chromium and
other Crubit clients (e.g. google3) is desirable, because it
promotes code portability and hides details of how Crubit
is distributed and integrated into Chromium.

# Where is Crubit's `BUILD.gn`?

Chromium integration of Crubit is provided by `//build/rust/*.gni`
and Crubit's `BUILD.gn` file lives in `//build/rust/crubit/BUILD.gn`.
Moving the `BUILD.gn` file to `//third_party/crubit/BUILD.gn` is
undesirable, because it would make it more difficult to use Crubit
in projects like PDFium or V8 (these projects already depend on
Chromium's `//build` directory but we want to avoid depending
on additional directories).

# Where can I find additional information about Crubit?

For more information about using Crubit in Chromium please see
`//docs/rust/crubit.md`.

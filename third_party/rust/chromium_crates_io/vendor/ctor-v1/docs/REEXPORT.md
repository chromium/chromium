# Re-exporting from another crate

The macros assume this crate is available as a direct dependency, resolving their
support paths through the crate's own name. If you re-export this crate's items as
part of your own crate (so that downstream users don't need to depend on it
directly), you have two options:

- (preferred) use the declarative macro form. It resolves its support paths
  relative to your re-export, so no extra configuration is required.
- Alternatively, pass the `crate_path` attribute to redirect the macro's
  generated output to the path where this crate has been re-exported.

See the `crate_path` entry in the *Macro Attributes* section below for the exact
syntax for this crate.

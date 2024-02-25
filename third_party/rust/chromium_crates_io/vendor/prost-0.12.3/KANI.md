# Kani
This document describes how to **locally** install and use Kani, along
with its experimental PropProof feature. Because of instability in
Kani internals, the GitHub action is the recommended option if you are
running in CI.

Kani is a software verification tool that complements testing by
proving the absence of certain classes of bugs like unwrap exceptions,
overflows, and assertion failures. See the [Kani
book](https://model-checking.github.io/kani/) for a full list of
capabilities and limitations.

## Installing Kani and PropProof
-  The install instructions for Kani can be [found
   here](https://model-checking.github.io/kani/install-guide.html). Once
   Kani is installed, you can run with `cargo kani` for projects or
   `kani` for individual Rust files.
- **[UNSTABLE]** To use PropProof, first download the source code
  from the Kani repository.
  ```bash
  git clone https://github.com/model-checking/kani.git --branch features/proptest propproof
  cd propproof; git submodule update --init
  ```

  Then, use `.cargo/config.toml` enable it in the local directory you
  want to run Kani in. This will override the `proptest` import in
  your repo.

  ```bash
  cd $YOUR_REPO_LOCAL_PATH
  mkdir '.cargo'
  echo "paths =[\"$PATH_TO_PROPPROOF\"]" > .cargo/config.toml
  ```

**Please Note**:
- `features/proptest` branch under Kani is likely not the final
  location for this code. If these instructions stop working, please
  consult the Kani documentation and file an issue on [the Kani
  repo](https://github.com/model-checking/kani.git).
- The cargo config file will force cargo to always use PropProof. To
  use `proptest`, delete the file.

## Running Kani
After installing Kani and PropProof, `cargo kani --tests` should
automatically run `proptest!` harnesses inside your crate. Use
`--harness` to run a specific harness, and `-p` for a specific
sub-crate.

If Kani returns with an error, you can use the concrete playback
feature using `--enable-unstable --concrete-playback print` and paste
in the code to your repository. Running this harness with `cargo test`
will replay the input found by Kani that produced this crash. Please
note that this feature is unstable and using `--concrete-playback
inplace` to automatically inject a replay harness is not supported
when using PropProof.

## Debugging CI Failure
```yaml
      - name: Verify with Kani
        uses: model-checking/kani-github-action@v0.xx
        with:
          enable-propproof: true
          args: |
            $KANI_ARGUMENTS
```

The above GitHub CI workflow is equivalent to `cargo kani
$KANI_ARGUMENTS` with PropProof installed. To replicate issues
locally, run `cargo kani` with the same arguments.

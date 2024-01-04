# Fuzzing

Prost ships a few fuzz tests, using both libfuzzer and aflfuzz.


## afl

To run the afl fuzz tests, first install cargo-afl:

    cargo install -f afl

Then build a fuzz target and run afl on it:

    cd fuzz/afl/<target>/
    cargo afl build --bin fuzz-target
    cargo afl fuzz -i in -o out target/debug/fuzz-target

To reproduce a crash:

    cd fuzz/afl/<target>/
    cargo build --bin reproduce
    cargo run --bin reproduce -- out/crashes/<crashfile>


## libfuzzer

TODO

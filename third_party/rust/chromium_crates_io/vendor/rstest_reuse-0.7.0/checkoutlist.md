# TODO list

- [ ] Update rustup
- [ ] Update dependency `cargo upgrade`
- [ ] Run all test
  - [ ] Stable: `RSTEST_TEST_CHANNEL=stable cargo +${RSTEST_TEST_CHANNEL} test`
  - [ ] Beta: `RSTEST_TEST_CHANNEL=beta cargo +${RSTEST_TEST_CHANNEL} test`
  - [ ] Nightly: `RSTEST_TEST_CHANNEL=nightly cargo +${RSTEST_TEST_CHANNEL} test`
- [ ] Check Cargo.toml version
- [ ] Check README
- [ ] prepare deploy `cargo publish --dry-run`
- [ ] deploy `cargo publish`
- [ ] Update `rstest` dependency
- [ ] Change next version
  - [ ] `Cargo.toml`

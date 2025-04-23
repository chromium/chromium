# Derivative based regex matcher

For basic introduction see
[Regular-expression derivatives reexamined](https://www.khoury.northeastern.edu/home/turon/re-deriv.pdf).

For extensions, see
[Derivative Based Nonbacktracking Real-World Regex Matching with Backtracking Semantics](https://www.microsoft.com/en-us/research/uploads/prod/2023/04/pldi23main-p249-final.pdf)
and
[Derivative Based Extended Regular Expression Matching Supporting Intersection, Complement and Lookarounds](https://arxiv.org/pdf/2309.14401)
and the [sbre](https://github.com/ieviev/sbre/) implementation of it.

## Usage

This library uses [regex-syntax](https://docs.rs/regex-syntax/latest/regex_syntax/)
for regular expression parsing.
This means that currently there is no surface syntax for `&` and `~` operators
(but the library supports it).

The library only checks if the regex matches the string from the beginning
(it doesn't search for it, or in other words there's an implied `\A` at the beginning).

```rust
let mut rx = Regex::new("[ab]c").unwrap();
assert!(rx.is_match("ac"));
assert!(rx.is_match("bc"));
assert!(!rx.is_match("xxac"));
assert!(!rx.is_match("acxx"));
```

The library supports a single look-ahead at the end of the regex,
written as `A(?P<stop>B)` where `A` and `B` are regexes without any look-arounds.
You can get the length of the string matching `B` upon successful match.

```rust
// the syntax in other libraries would be: r"\A[abx]*(?=[xq]*y)"
let mut rx = Regex::new("[abx]*(?P<stop>[xq]*y)").unwrap();
assert!(rx.lookahead_len("axxxxxy") == Some(1));
assert!(rx.lookahead_len("axxxxxqqqy") == Some(4));
assert!(rx.lookahead_len("axxxxxqqq") == None);
assert!(rx.lookahead_len("ccqy") == None);
```

## Code map

In recommended reading order:

- [simplify.rs](./src/simplify.rs) - simplification (rewrite rules) of AST
- [deriv.rs](./src/deriv.rs) - derivative computation
- [bytecompress.rs](./src/bytecompress.rs) - alphabet compression
- [syntax.rs](./src/syntax.rs) - uses `regex-syntax` crate for parsing
- [regexc.rs](./src/regex.rs) - top-level memoization of state transition and user interface

The rest:

- [ast.rs](./src/ast.rs) - AST for regexes
- [hashcons.rs](./src/hashcons.rs) - hash-consing of u32 vectors (used by AST)

## TODO

- [ ] more simplification rules from sbre
- [ ] benchmarks
- [ ] extend regex-syntax for `&` and `~` operators
- [ ] add `& valid-utf8` if there is negation somewhere 
- [x] either make `derivative()` non-recursive (`mk_*()` already are?) or limit the regex depth
- [ ] implement relevance check for `&` and `~` operators; see [symbolic derivatives](https://easychair.org/publications/open/cgnn)
- [x] add `.forced_byte()` method on state descriptor


## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.


A crate that allows you to mostly-safely cast one type into another type.

This is mostly useful for generic functions, e.g.

```rs
pub fn foo<S>(s: S) {
    if let Ok(a) = unsafe { unty::<S, u8>(s) } {
        println!("It is an u8 with value {a}");
    } else {
        println!("it is not an u8");
    }
}
foo(10u8); // will print "it is an u8"
foo("test"); // will print "it is not an u8"
```

This operation is still unsafe because it allows you to extend lifetimes. There currently is not a way to prevent this

```rs
if let Ok(str) = unsafe { unty::<&'a str, &'static str>(input) } {
    // the compiler may now light your PC on fire
}
```

# License

This crate is dual licenced MIT and Apache-2.0, at your own leisure

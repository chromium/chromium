#[test]
fn test_create_map() {
    let _m = indexmap::indexmap! {
        1 => 2,
        7 => 1,
        2 => 2,
        3 => 3,
    };
}

#[test]
fn test_create_set() {
    let _s = indexmap::indexset! {
        1,
        7,
        2,
        3,
    };
}

#[test]
fn test_map_shadow() {
    // The macro used to have its own `const CAP` which would shadow this, because items are not
    // protected by macro hygiene. Now we avoid any items in the macro, and its local `map` *is*
    // hygienic vs. the local `map` here.
    const CAP: usize = 42;
    let map = -1;
    let m = indexmap::indexmap! {
        map => CAP,
    };
    assert_eq!(m[&map], CAP);
    assert_eq!(m[0], CAP);
}

#[test]
fn test_map_shadow_default() {
    const CAP: usize = 42;
    let map = -1;
    let m = indexmap::indexmap_with_default! {
        fnv::FnvHasher;
        map => CAP,
    };
    assert_eq!(m[&map], CAP);
    assert_eq!(m[0], CAP);
}

#[test]
fn test_set_shadow() {
    const CAP: usize = 42;
    let s = indexmap::indexset!(CAP);
    assert_eq!(s[0], CAP);
}

#[test]
fn test_set_shadow_default() {
    const CAP: usize = 42;
    let s = indexmap::indexset_with_default!(fnv::FnvHasher; CAP);
    assert_eq!(s[0], CAP);
}

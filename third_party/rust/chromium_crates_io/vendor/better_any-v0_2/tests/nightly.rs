use better_any::nightly::{downcast_any, DowncastExt};
use std::any::Any;
use std::cell::RefCell;

#[test]
fn test() {
    use std::fmt::Debug;
    let a = 5i32;
    let any = &a as &dyn Any;
    let result: &i32 = downcast_any(any).unwrap();
    // assert_eq!(result)
}

#[test]
fn test2() {
    use std::cell::RefCell;
    use std::fmt::Debug;
    let a = RefCell::new(5i32);
    let any = &a as &RefCell<dyn Any>;
    let result: &RefCell<i32> = downcast_any(any).unwrap();
    assert_eq!(*a.borrow(), *result.borrow());
}

//should fail to compile

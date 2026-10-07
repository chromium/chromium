use link_section::declarative::{in_section, section};
use link_section::TypedReferenceSection;

section! {
    #[section(unsafe, type = reference)]
    static FOO: TypedReferenceSection<u32>;
}

in_section! {
    #[in_section(unsafe, type = reference, name = FOO)]
    const ITEM: u32 = 42;
}

fn main() {}

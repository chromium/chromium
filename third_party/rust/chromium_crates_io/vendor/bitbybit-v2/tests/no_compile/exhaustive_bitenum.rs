use bitbybit::bitenum;

#[bitenum(u1, exhaustive = false)]
pub enum ExhaustiveWhenItShouldNotBe {
    A = 0,
    B = 1,
}

fn main() {}

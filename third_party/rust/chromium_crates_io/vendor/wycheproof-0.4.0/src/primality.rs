//! Primality checking tests

use super::*;

define_test_set!("Primality", "primality_test_schema.json");

define_test_set_names!(Primality => "primality");

define_algorithm_map!("PrimalityTest" => Primality);

define_test_flags!(CarmichaelNumber, NegativeOfPrime, WorstCaseMillerRabin);

define_typeid!(TestGroupTypeId => "PrimalityTest");

define_test_group!();

define_test!(value: Vec<u8>);

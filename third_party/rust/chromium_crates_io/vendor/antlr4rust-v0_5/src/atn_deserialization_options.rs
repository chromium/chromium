#[derive(Debug)]
pub struct ATNDeserializationOptions {
    verify_atn: bool,
}

impl ATNDeserializationOptions {
    pub fn is_verify(&self) -> bool {
        self.verify_atn
    }
}

impl Default for ATNDeserializationOptions {
    fn default() -> Self {
        ATNDeserializationOptions {
            verify_atn: true,
        }
    }
}

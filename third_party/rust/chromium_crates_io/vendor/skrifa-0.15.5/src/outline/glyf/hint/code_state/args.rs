//! Inline instruction arguments.

/// Support for decoding a sequence of bytes or words from the
/// instruction stream.
#[derive(Copy, Clone, Default, Debug)]
pub struct Args<'a> {
    bytes: &'a [u8],
    is_words: bool,
}

impl<'a> Args<'a> {
    pub(crate) fn new(bytes: &'a [u8], is_words: bool) -> Self {
        Self { bytes, is_words }
    }

    /// Returns the number of arguments in the list.
    pub fn len(&self) -> usize {
        if self.is_words {
            self.bytes.len() / 2
        } else {
            self.bytes.len()
        }
    }

    /// Returns true if the argument list is empty.
    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }

    /// Returns an iterator over the argument values.
    pub fn values(&self) -> impl Iterator<Item = i32> + 'a + Clone {
        let bytes = if self.is_words { &[] } else { self.bytes };
        let words = if self.is_words { self.bytes } else { &[] };
        bytes
            .iter()
            .map(|byte| *byte as u32 as i32)
            .chain(words.chunks_exact(2).map(|chunk| {
                let word = ((chunk[0] as u16) << 8) | chunk[1] as u16;
                // Double cast to ensure sign extension
                word as i16 as i32
            }))
    }
}

/// Mock for testing arguments.
#[cfg(test)]
pub(crate) struct MockArgs {
    bytes: Vec<u8>,
    is_words: bool,
}

#[cfg(test)]
impl MockArgs {
    pub fn from_bytes(bytes: &[u8]) -> Self {
        Self {
            bytes: bytes.into(),
            is_words: false,
        }
    }

    pub fn from_words(words: &[i16]) -> Self {
        Self {
            bytes: words
                .iter()
                .map(|word| *word as u16)
                .flat_map(|word| vec![(word >> 8) as u8, word as u8])
                .collect(),
            is_words: true,
        }
    }

    pub fn args(&self) -> Args {
        Args {
            bytes: &self.bytes,
            is_words: self.is_words,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::MockArgs;

    #[test]
    fn byte_args() {
        let values = [5, 2, 85, 92, 26, 42, u8::MIN, u8::MAX];
        let mock = MockArgs::from_bytes(&values);
        let decoded = mock.args().values().collect::<Vec<_>>();
        assert!(values.iter().map(|x| *x as i32).eq(decoded.iter().copied()));
    }

    #[test]
    fn word_args() {
        let values = [-5, 2, 2845, 92, -26, 42, i16::MIN, i16::MAX];
        let mock = MockArgs::from_words(&values);
        let decoded = mock.args().values().collect::<Vec<_>>();
        assert!(values.iter().map(|x| *x as i32).eq(decoded.iter().copied()));
    }
}

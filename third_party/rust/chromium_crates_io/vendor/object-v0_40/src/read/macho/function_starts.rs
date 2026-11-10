use crate::read::{Bytes, ReadError, Result};

/// Iterator over the function starts in a `LC_FUNCTION_STARTS` load command.
#[derive(Debug, Default, Clone, Copy)]
pub struct FunctionStartsIterator<'data> {
    data: Bytes<'data>,
    addr: u64,
}

impl<'data> FunctionStartsIterator<'data> {
    pub(super) fn new(data: &'data [u8], addr: u64) -> Self {
        FunctionStartsIterator {
            data: Bytes(data),
            addr,
        }
    }
}

impl<'data> FunctionStartsIterator<'data> {
    /// Returns the next function start address.
    pub fn next(&mut self) -> Result<Option<u64>> {
        if self.data.is_empty() {
            return Ok(None);
        }

        let result = self.parse();
        if result.is_err() {
            self.data = Bytes(&[]);
        }
        result
    }

    fn parse(&mut self) -> Result<Option<u64>> {
        let delta = self
            .data
            .read_uleb128()
            .read_error("Invalid ULEB128 in LC_FUNCTION_STARTS")?;
        if delta == 0 {
            self.data = Bytes(&[]);
            return Ok(None);
        }

        self.addr = self
            .addr
            .checked_add(delta)
            .read_error("Address overflow in LC_FUNCTION_STARTS")?;
        Ok(Some(self.addr))
    }
}

impl<'data> Iterator for FunctionStartsIterator<'data> {
    type Item = Result<u64>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

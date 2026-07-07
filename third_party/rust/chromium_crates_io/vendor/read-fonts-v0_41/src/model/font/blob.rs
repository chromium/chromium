//! Blobs of font bytes.

use alloc::{sync::Arc, vec::Vec};
use core::ops::Deref;

/// Font data as a blob of bytes.
#[derive(Clone)]
pub enum FontBlob {
    /// A borrowed static reference to a slice of bytes.
    Static(&'static [u8]),
    /// An `Arc` containing anything that can be viewed as a contiguous slice
    /// of bytes. Typically a `Vec` or a memory mapped buffer.
    Shared(Arc<dyn AsRef<[u8]> + Send + Sync>),
}

impl AsRef<[u8]> for FontBlob {
    fn as_ref(&self) -> &[u8] {
        match self {
            Self::Static(bytes) => bytes,
            Self::Shared(arc) => arc.as_ref().as_ref(),
        }
    }
}

impl Deref for FontBlob {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        self.as_ref()
    }
}

impl From<&'static [u8]> for FontBlob {
    fn from(value: &'static [u8]) -> Self {
        Self::Static(value)
    }
}

impl From<Arc<dyn AsRef<[u8]> + Send + Sync>> for FontBlob {
    fn from(value: Arc<dyn AsRef<[u8]> + Send + Sync>) -> Self {
        Self::Shared(value)
    }
}

impl From<Vec<u8>> for FontBlob {
    fn from(value: Vec<u8>) -> Self {
        Self::Shared(Arc::new(value))
    }
}

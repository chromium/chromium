use core::fmt::Debug;

use crate::elf;
use crate::endian;
use crate::pod::Pod;
use crate::read;

/// A trait for generic access to [`elf::CompressionHeader32`] and [`elf::CompressionHeader64`].
#[allow(missing_docs)]
pub trait CompressionHeader: Debug + Pod + read::private::Sealed {
    type Word: Into<u64>;
    type Endian: endian::Endian;

    fn ch_type(&self, endian: Self::Endian) -> elf::CompressionType;
    fn ch_size(&self, endian: Self::Endian) -> Self::Word;
    fn ch_addralign(&self, endian: Self::Endian) -> Self::Word;
}

impl<Endian: endian::Endian> read::private::Sealed for elf::CompressionHeader32<Endian> {}

impl<Endian: endian::Endian> CompressionHeader for elf::CompressionHeader32<Endian> {
    type Word = u32;
    type Endian = Endian;

    #[inline]
    fn ch_type(&self, endian: Self::Endian) -> elf::CompressionType {
        self.ch_type.get(endian)
    }

    #[inline]
    fn ch_size(&self, endian: Self::Endian) -> Self::Word {
        self.ch_size.get(endian)
    }

    #[inline]
    fn ch_addralign(&self, endian: Self::Endian) -> Self::Word {
        self.ch_addralign.get(endian)
    }
}

impl<Endian: endian::Endian> read::private::Sealed for elf::CompressionHeader64<Endian> {}

impl<Endian: endian::Endian> CompressionHeader for elf::CompressionHeader64<Endian> {
    type Word = u64;
    type Endian = Endian;

    #[inline]
    fn ch_type(&self, endian: Self::Endian) -> elf::CompressionType {
        self.ch_type.get(endian)
    }

    #[inline]
    fn ch_size(&self, endian: Self::Endian) -> Self::Word {
        self.ch_size.get(endian)
    }

    #[inline]
    fn ch_addralign(&self, endian: Self::Endian) -> Self::Word {
        self.ch_addralign.get(endian)
    }
}

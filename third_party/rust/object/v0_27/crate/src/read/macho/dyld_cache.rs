use core::slice;

use crate::read::{Error, File, ReadError, ReadRef, Result};
use crate::{macho, Architecture, Endian, Endianness};

/// A parsed representation of the dyld shared cache.
#[derive(Debug)]
pub struct DyldCache<'data, E = Endianness, R = &'data [u8]>
where
    E: Endian,
    R: ReadRef<'data>,
{
    endian: E,
    data: R,
    header: &'data macho::DyldCacheHeader<E>,
    mappings: &'data [macho::DyldCacheMappingInfo<E>],
    images: &'data [macho::DyldCacheImageInfo<E>],
    arch: Architecture,
}

impl<'data, E, R> DyldCache<'data, E, R>
where
    E: Endian,
    R: ReadRef<'data>,
{
    /// Parse the raw dyld shared cache data.
    pub fn parse(data: R) -> Result<Self> {
        let header = macho::DyldCacheHeader::parse(data)?;
        let (arch, endian) = header.parse_magic()?;
        let mappings = header.mappings(endian, data)?;
        let images = header.images(endian, data)?;
        Ok(DyldCache {
            endian,
            data,
            header,
            mappings,
            images,
            arch,
        })
    }

    /// Get the architecture type of the file.
    pub fn architecture(&self) -> Architecture {
        self.arch
    }

    /// Get the endianness of the file.
    #[inline]
    pub fn endianness(&self) -> Endianness {
        if self.is_little_endian() {
            Endianness::Little
        } else {
            Endianness::Big
        }
    }

    /// Return true if the file is little endian, false if it is big endian.
    pub fn is_little_endian(&self) -> bool {
        self.endian.is_little_endian()
    }

    /// Iterate over the images in this cache.
    pub fn images<'cache>(&'cache self) -> DyldCacheImageIterator<'data, 'cache, E, R> {
        DyldCacheImageIterator {
            cache: self,
            iter: self.images.iter(),
        }
    }
}

/// An iterator over all the images (dylibs) in the dyld shared cache.
#[derive(Debug)]
pub struct DyldCacheImageIterator<'data, 'cache, E = Endianness, R = &'data [u8]>
where
    E: Endian,
    R: ReadRef<'data>,
{
    cache: &'cache DyldCache<'data, E, R>,
    iter: slice::Iter<'data, macho::DyldCacheImageInfo<E>>,
}

impl<'data, 'cache, E, R> Iterator for DyldCacheImageIterator<'data, 'cache, E, R>
where
    E: Endian,
    R: ReadRef<'data>,
{
    type Item = DyldCacheImage<'data, E, R>;

    fn next(&mut self) -> Option<DyldCacheImage<'data, E, R>> {
        let image_info = self.iter.next()?;
        Some(DyldCacheImage {
            endian: self.cache.endian,
            data: self.cache.data,
            mappings: self.cache.mappings,
            image_info,
        })
    }
}

/// One image (dylib) from inside the dyld shared cache.
#[derive(Debug)]
pub struct DyldCacheImage<'data, E = Endianness, R = &'data [u8]>
where
    E: Endian,
    R: ReadRef<'data>,
{
    endian: E,
    data: R,
    mappings: &'data [macho::DyldCacheMappingInfo<E>],
    image_info: &'data macho::DyldCacheImageInfo<E>,
}

impl<'data, E, R> DyldCacheImage<'data, E, R>
where
    E: Endian,
    R: ReadRef<'data>,
{
    /// The file system path of this image.
    pub fn path(&self) -> Result<&'data str> {
        let path = self.image_info.path(self.endian, self.data)?;
        // The path should always be ascii, so from_utf8 should alway succeed.
        let path = core::str::from_utf8(path).map_err(|_| Error("Path string not valid utf-8"))?;
        Ok(path)
    }

    /// The offset in the dyld cache file where this image starts.
    pub fn file_offset(&self) -> Result<u64> {
        self.image_info.file_offset(self.endian, self.mappings)
    }

    /// Parse this image into an Object.
    pub fn parse_object(&self) -> Result<File<'data, R>> {
        File::parse_at(self.data, self.file_offset()?)
    }
}

impl<E: Endian> macho::DyldCacheHeader<E> {
    /// Read the dyld cache header.
    pub fn parse<'data, R: ReadRef<'data>>(data: R) -> Result<&'data Self> {
        data.read_at::<macho::DyldCacheHeader<E>>(0)
            .read_error("Invalid dyld cache header size or alignment")
    }

    /// Returns (arch, endian) based on the magic string.
    pub fn parse_magic(&self) -> Result<(Architecture, E)> {
        let (arch, is_big_endian) = match &self.magic {
            b"dyld_v1    i386\0" => (Architecture::I386, false),
            b"dyld_v1  x86_64\0" => (Architecture::X86_64, false),
            b"dyld_v1 x86_64h\0" => (Architecture::X86_64, false),
            b"dyld_v1     ppc\0" => (Architecture::PowerPc, true),
            b"dyld_v1   armv6\0" => (Architecture::Arm, false),
            b"dyld_v1   armv7\0" => (Architecture::Arm, false),
            b"dyld_v1  armv7f\0" => (Architecture::Arm, false),
            b"dyld_v1  armv7s\0" => (Architecture::Arm, false),
            b"dyld_v1  armv7k\0" => (Architecture::Arm, false),
            b"dyld_v1   arm64\0" => (Architecture::Aarch64, false),
            b"dyld_v1  arm64e\0" => (Architecture::Aarch64, false),
            _ => return Err(Error("Unrecognized dyld cache magic")),
        };
        let endian =
            E::from_big_endian(is_big_endian).read_error("Unsupported dyld cache endian")?;
        Ok((arch, endian))
    }

    /// Return the mapping information table.
    pub fn mappings<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<&'data [macho::DyldCacheMappingInfo<E>]> {
        data.read_slice_at::<macho::DyldCacheMappingInfo<E>>(
            self.mapping_offset.get(endian).into(),
            self.mapping_count.get(endian) as usize,
        )
        .read_error("Invalid dyld cache mapping size or alignment")
    }

    /// Return the image information table.
    pub fn images<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<&'data [macho::DyldCacheImageInfo<E>]> {
        data.read_slice_at::<macho::DyldCacheImageInfo<E>>(
            self.images_offset.get(endian).into(),
            self.images_count.get(endian) as usize,
        )
        .read_error("Invalid dyld cache image size or alignment")
    }
}

impl<E: Endian> macho::DyldCacheImageInfo<E> {
    /// The file system path of this image.
    pub fn path<'data, R: ReadRef<'data>>(&self, endian: E, data: R) -> Result<&'data [u8]> {
        let r_start = self.path_file_offset.get(endian).into();
        let r_end = data.len().read_error("Couldn't get data len()")?;
        data.read_bytes_at_until(r_start..r_end, 0)
            .read_error("Couldn't read dyld cache image path")
    }

    /// Find the file offset of the image by looking up its address in the mappings.
    pub fn file_offset(
        &self,
        endian: E,
        mappings: &[macho::DyldCacheMappingInfo<E>],
    ) -> Result<u64> {
        let address = self.address.get(endian);
        for mapping in mappings {
            let mapping_address = mapping.address.get(endian);
            if address >= mapping_address
                && address < mapping_address.wrapping_add(mapping.size.get(endian))
            {
                return Ok(address - mapping_address + mapping.file_offset.get(endian));
            }
        }
        Err(Error("Invalid dyld cache image address"))
    }
}

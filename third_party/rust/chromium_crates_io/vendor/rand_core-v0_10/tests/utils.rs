//! Tests for the `utils` module items
use rand_core::{Infallible, Rng, TryRng, utils};

struct DummyRng(u32);

impl TryRng for DummyRng {
    type Error = Infallible;
    fn try_next_u32(&mut self) -> Result<u32, Self::Error> {
        self.0 = self.0.wrapping_mul(3);
        Ok(self.0)
    }

    fn try_next_u64(&mut self) -> Result<u64, Self::Error> {
        utils::next_u64_via_u32(self)
    }

    fn try_fill_bytes(&mut self, dst: &mut [u8]) -> Result<(), Self::Error> {
        utils::fill_bytes_via_next_word(dst, || self.try_next_u32())
    }
}

#[test]
fn test_next_u64_via_u32() {
    let mut rng = DummyRng(0xF973_F2EC);

    assert_eq!(rng.next_u64(), 0xC513_8A4C_EC5B_D8C4);
    assert_eq!(rng.next_u64(), 0xEDAF_DCAC_4F3A_9EE4);
    assert_eq!(rng.next_u64(), 0x5B2E_C20C_C90F_9604);
}

#[test]
fn test_fill_bytes_via_next_word() {
    let mut rng = DummyRng(0xF973_F2EC);

    let mut buf = [0u8; 8];

    let dst = &mut buf[..3];
    rng.fill_bytes(dst);
    assert_eq!(dst, &[196, 216, 91]);

    let dst = &mut buf[..5];
    rng.fill_bytes(dst);
    assert_eq!(dst, &[76, 138, 19, 197, 228]);

    let dst = &mut buf[..];
    rng.fill_bytes(dst);
    assert_eq!(dst, &[172, 220, 175, 237, 4, 150, 15, 201]);
}

#[test]
fn test_read_words() {
    use utils::read_words;

    let bytes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

    let buf: [u32; 4] = read_words(&bytes);
    assert_eq!(buf[0], 0x0403_0201);
    assert_eq!(buf[3], 0x100F_0E0D);

    let buf: [u32; 3] = read_words(&bytes[1..13]); // unaligned
    assert_eq!(buf[0], 0x0504_0302);
    assert_eq!(buf[2], 0x0D0C_0B0A);

    let buf: [u64; 2] = read_words(&bytes);
    assert_eq!(buf[0], 0x0807_0605_0403_0201);
    assert_eq!(buf[1], 0x100F_0E0D_0C0B_0A09);

    let buf: [u64; 1] = read_words(&bytes[7..15]); // unaligned
    assert_eq!(buf[0], 0x0F0E_0D0C_0B0A_0908);
}

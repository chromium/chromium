#![cfg(feature = "derive")]

/// HID-IO Packet Buffer Struct
///
/// # Remarks
/// Used to store HID-IO data chunks. Will be chunked into individual packets on transmission.
#[repr(C)]
#[derive(PartialEq, Eq, Clone, Debug, bincode::Encode)]
pub struct HidIoPacketBuffer<const H: usize> {
    /// Type of packet (Continued is automatically set if needed)
    pub ptype: u32,
    /// Packet Id
    pub id: u32,
    /// Packet length for serialization (in bytes)
    pub max_len: u32,
    /// Payload data, chunking is done automatically by serializer
    pub data: [u8; H],
    /// Set False if buffer is not complete, True if it is
    pub done: bool,
}

#[repr(u32)]
#[derive(PartialEq, Eq, Clone, Copy, Debug, bincode::Encode)]
#[allow(dead_code)]
/// Requests for to perform a specific action
pub enum HidIoCommandId {
    SupportedIds = 0x00,
    GetInfo = 0x01,
    TestPacket = 0x02,
    ResetHidIo = 0x03,
    Reserved = 0x04, // ... 0x0F

    GetProperties = 0x10,
    KeyState = 0x11,
    KeyboardLayout = 0x12,
    KeyLayout = 0x13,
    KeyShapes = 0x14,
    LedLayout = 0x15,
    FlashMode = 0x16,
    UnicodeText = 0x17,
    UnicodeState = 0x18,
    HostMacro = 0x19,
    SleepMode = 0x1A,

    KllState = 0x20,
    PixelSetting = 0x21,
    PixelSet1c8b = 0x22,
    PixelSet3c8b = 0x23,
    PixelSet1c16b = 0x24,
    PixelSet3c16b = 0x25,

    OpenUrl = 0x30,
    TerminalCmd = 0x31,
    GetInputLayout = 0x32,
    SetInputLayout = 0x33,
    TerminalOut = 0x34,

    HidKeyboard = 0x40,
    HidKeyboardLed = 0x41,
    HidMouse = 0x42,
    HidJoystick = 0x43,
    HidSystemCtrl = 0x44,
    HidConsumerCtrl = 0x45,

    ManufacturingTest = 0x50,
    ManufacturingResult = 0x51,

    Unused = 0xFFFF,
}

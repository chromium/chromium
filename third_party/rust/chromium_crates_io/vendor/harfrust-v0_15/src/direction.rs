use core::str::FromStr;

use crate::Script;

/// Defines the direction in which text is to be read.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum Direction {
    /// Initial, unset direction.
    Invalid,
    /// Text is set horizontally from left to right.
    LeftToRight,
    /// Text is set horizontally from right to left.
    RightToLeft,
    /// Text is set vertically from top to bottom.
    TopToBottom,
    /// Text is set vertically from bottom to top.
    BottomToTop,
}

impl Direction {
    #[inline]
    pub(crate) fn is_horizontal(self) -> bool {
        match self {
            Direction::Invalid => false,
            Direction::LeftToRight => true,
            Direction::RightToLeft => true,
            Direction::TopToBottom => false,
            Direction::BottomToTop => false,
        }
    }

    #[inline]
    pub(crate) fn is_vertical(self) -> bool {
        !self.is_horizontal()
    }

    #[inline]
    pub(crate) fn is_forward(self) -> bool {
        match self {
            Direction::Invalid => false,
            Direction::LeftToRight => true,
            Direction::RightToLeft => false,
            Direction::TopToBottom => true,
            Direction::BottomToTop => false,
        }
    }

    #[inline]
    pub(crate) fn is_backward(self) -> bool {
        !self.is_forward()
    }

    #[inline]
    pub(crate) fn reverse(self) -> Self {
        match self {
            Direction::Invalid => Direction::Invalid,
            Direction::LeftToRight => Direction::RightToLeft,
            Direction::RightToLeft => Direction::LeftToRight,
            Direction::TopToBottom => Direction::BottomToTop,
            Direction::BottomToTop => Direction::TopToBottom,
        }
    }

    pub(crate) fn from_script(script: Script) -> Option<Self> {
        // https://docs.google.com/spreadsheets/d/1Y90M0Ie3MUJ6UVCRDOypOtijlMDLNNyyLk36T6iMu0o

        match script {
            // Unicode-1.1 additions
            Script::ARABIC |
            Script::HEBREW |

            // Unicode-3.0 additions
            Script::SYRIAC |
            Script::THAANA |

            // Unicode-4.0 additions
            Script::CYPRIOT |

            // Unicode-4.1 additions
            Script::KHAROSHTHI |

            // Unicode-5.0 additions
            Script::PHOENICIAN |
            Script::NKO |

            // Unicode-5.1 additions
            Script::LYDIAN |

            // Unicode-5.2 additions
            Script::AVESTAN |
            Script::IMPERIAL_ARAMAIC |
            Script::INSCRIPTIONAL_PAHLAVI |
            Script::INSCRIPTIONAL_PARTHIAN |
            Script::OLD_SOUTH_ARABIAN |
            Script::OLD_TURKIC |
            Script::SAMARITAN |

            // Unicode-6.0 additions
            Script::MANDAIC |

            // Unicode-6.1 additions
            Script::MEROITIC_CURSIVE |
            Script::MEROITIC_HIEROGLYPHS |

            // Unicode-7.0 additions
            Script::MANICHAEAN |
            Script::MENDE_KIKAKUI |
            Script::NABATAEAN |
            Script::OLD_NORTH_ARABIAN |
            Script::PALMYRENE |
            Script::PSALTER_PAHLAVI |

            // Unicode-8.0 additions
            Script::HATRAN |

            // Unicode-9.0 additions
            Script::ADLAM |

            // Unicode-11.0 additions
            Script::HANIFI_ROHINGYA |
            Script::OLD_SOGDIAN |
            Script::SOGDIAN |

            // Unicode-12.0 additions
            Script::ELYMAIC |

            // Unicode-13.0 additions
            Script::CHORASMIAN |
            Script::YEZIDI |

            // Unicode-14.0 additions
            Script::OLD_UYGHUR |

            // Unicode-16.0 additions
            Script::GARAY |

            // Unicode-17.0 additions
            Script::SIDETIC => {
                Some(Direction::RightToLeft)
            }

            // https://github.com/harfbuzz/harfbuzz/issues/1000
            Script::OLD_HUNGARIAN |
            Script::OLD_ITALIC |
            Script::RUNIC |
            Script::TIFINAGH => {
                None
            }

            _ => Some(Direction::LeftToRight),
        }
    }
}

impl Default for Direction {
    #[inline]
    fn default() -> Self {
        Direction::Invalid
    }
}

impl FromStr for Direction {
    type Err = &'static str;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s.is_empty() {
            return Err("invalid direction");
        }

        // harfbuzz also matches only the first letter.
        match s.as_bytes()[0].to_ascii_lowercase() {
            b'l' => Ok(Direction::LeftToRight),
            b'r' => Ok(Direction::RightToLeft),
            b't' => Ok(Direction::TopToBottom),
            b'b' => Ok(Direction::BottomToTop),
            _ => Err("invalid direction"),
        }
    }
}

// Std
use std::ops::BitOr;

// Third party
use bitflags::bitflags;

#[allow(unused)]
use crate::Arg;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct ArgFlags(Flags);

impl Default for ArgFlags {
    fn default() -> Self {
        Self::empty()
    }
}

/// Various settings that apply to arguments and may be set, unset, and checked via getter/setter
/// methods [`Arg::setting`], [`Arg::unset_setting`], and [`Arg::is_set`]. This is what the
/// [`Arg`] methods which accept a `bool` use internally.
///
/// [`Arg`]: crate::Arg
/// [`Arg::setting`]: crate::Arg::setting()
/// [`Arg::unset_setting`]: crate::Arg::unset_setting()
/// [`Arg::is_set`]: crate::Arg::is_set()
#[derive(Debug, PartialEq, Copy, Clone)]
#[non_exhaustive]
pub(crate) enum ArgSettings {
    Required,
    Global,
    Hidden,
    NextLineHelp,
    HidePossibleValues,
    AllowHyphenValues,
    AllowNegativeNumbers,
    RequireEquals,
    Last,
    TrailingVarArg,
    HideDefaultValue,
    IgnoreCase,
    #[cfg(feature = "env")]
    HideEnv,
    #[cfg(feature = "env")]
    HideEnvValues,
    HiddenShortHelp,
    HiddenLongHelp,
    Exclusive,
}

bitflags! {
    struct Flags: u32 {
        const REQUIRED         = 1;
        const GLOBAL           = 1 << 3;
        const HIDDEN           = 1 << 4;
        const TRAILING_VARARG  = 1 << 5;
        const ALLOW_NEG_NUMS   = 1 << 6;
        const NEXT_LINE_HELP   = 1 << 7;
        const DELIM_NOT_SET    = 1 << 10;
        const HIDE_POS_VALS    = 1 << 11;
        const ALLOW_TAC_VALS   = 1 << 12;
        const REQUIRE_EQUALS   = 1 << 13;
        const LAST             = 1 << 14;
        const HIDE_DEFAULT_VAL = 1 << 15;
        const CASE_INSENSITIVE = 1 << 16;
        #[cfg(feature = "env")]
        const HIDE_ENV_VALS    = 1 << 17;
        const HIDDEN_SHORT_H   = 1 << 18;
        const HIDDEN_LONG_H    = 1 << 19;
        #[cfg(feature = "env")]
        const HIDE_ENV         = 1 << 21;
        const EXCLUSIVE        = 1 << 23;
        const NO_OP            = 0;
    }
}

impl_settings! { ArgSettings, ArgFlags,
    Required => Flags::REQUIRED,
    Global => Flags::GLOBAL,
    Hidden => Flags::HIDDEN,
    NextLineHelp => Flags::NEXT_LINE_HELP,
    HidePossibleValues => Flags::HIDE_POS_VALS,
    AllowHyphenValues => Flags::ALLOW_TAC_VALS,
    AllowNegativeNumbers => Flags::ALLOW_NEG_NUMS,
    RequireEquals => Flags::REQUIRE_EQUALS,
    Last => Flags::LAST,
    TrailingVarArg => Flags::TRAILING_VARARG,
    IgnoreCase => Flags::CASE_INSENSITIVE,
    #[cfg(feature = "env")]
    HideEnv => Flags::HIDE_ENV,
    #[cfg(feature = "env")]
    HideEnvValues => Flags::HIDE_ENV_VALS,
    HideDefaultValue => Flags::HIDE_DEFAULT_VAL,
    HiddenShortHelp => Flags::HIDDEN_SHORT_H,
    HiddenLongHelp => Flags::HIDDEN_LONG_H,
    Exclusive => Flags::EXCLUSIVE
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::Arg;

    #[test]
    fn setting() {
        let m = Arg::new("setting").setting(ArgSettings::Required);
        assert!(m.is_required_set());
    }

    #[test]
    fn unset_setting() {
        let m = Arg::new("unset_setting").setting(ArgSettings::Required);
        assert!(m.is_required_set());

        let m = m.unset_setting(ArgSettings::Required);
        assert!(!m.is_required_set(), "{:#?}", m);
    }

    #[test]
    fn setting_bitor() {
        let m = Arg::new("setting_bitor")
            .setting(ArgSettings::Required | ArgSettings::Hidden | ArgSettings::Last);

        assert!(m.is_required_set());
        assert!(m.is_hide_set());
        assert!(m.is_last_set());
    }

    #[test]
    fn unset_setting_bitor() {
        let m = Arg::new("unset_setting_bitor")
            .setting(ArgSettings::Required)
            .setting(ArgSettings::Hidden)
            .setting(ArgSettings::Last);

        assert!(m.is_required_set());
        assert!(m.is_hide_set());
        assert!(m.is_last_set());

        let m = m.unset_setting(ArgSettings::Required | ArgSettings::Hidden | ArgSettings::Last);
        assert!(!m.is_required_set(), "{:#?}", m);
        assert!(!m.is_hide_set(), "{:#?}", m);
        assert!(!m.is_last_set(), "{:#?}", m);
    }
}

// Std
use std::{
    borrow::Cow,
    ffi::{OsStr, OsString},
    fmt::{Debug, Display},
    iter::{Cloned, Flatten, Map},
    slice::Iter,
    str::FromStr,
};

// Third Party
use indexmap::IndexMap;

// Internal
use crate::parse::MatchedArg;
use crate::parse::ValueSource;
use crate::util::{Id, Key};
use crate::{Error, INVALID_UTF8};

/// Container for parse results.
///
/// Used to get information about the arguments that were supplied to the program at runtime by
/// the user. New instances of this struct are obtained by using the [`Command::get_matches`] family of
/// methods.
///
/// # Examples
///
/// ```no_run
/// # use clap::{Command, Arg};
/// let matches = Command::new("MyApp")
///     .arg(Arg::new("out")
///         .long("output")
///         .required(true)
///         .takes_value(true))
///     .arg(Arg::new("debug")
///         .short('d')
///         .multiple_occurrences(true))
///     .arg(Arg::new("cfg")
///         .short('c')
///         .takes_value(true))
///     .get_matches(); // builds the instance of ArgMatches
///
/// // to get information about the "cfg" argument we created, such as the value supplied we use
/// // various ArgMatches methods, such as ArgMatches::value_of
/// if let Some(c) = matches.value_of("cfg") {
///     println!("Value for -c: {}", c);
/// }
///
/// // The ArgMatches::value_of method returns an Option because the user may not have supplied
/// // that argument at runtime. But if we specified that the argument was "required" as we did
/// // with the "out" argument, we can safely unwrap because `clap` verifies that was actually
/// // used at runtime.
/// println!("Value for --output: {}", matches.value_of("out").unwrap());
///
/// // You can check the presence of an argument
/// if matches.is_present("out") {
///     // Another way to check if an argument was present, or if it occurred multiple times is to
///     // use occurrences_of() which returns 0 if an argument isn't found at runtime, or the
///     // number of times that it occurred, if it was. To allow an argument to appear more than
///     // once, you must use the .multiple_occurrences(true) method, otherwise it will only return 1 or 0.
///     if matches.occurrences_of("debug") > 2 {
///         println!("Debug mode is REALLY on, don't be crazy");
///     } else {
///         println!("Debug mode kind of on");
///     }
/// }
/// ```
/// [`Command::get_matches`]: crate::Command::get_matches()
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ArgMatches {
    #[cfg(debug_assertions)]
    pub(crate) valid_args: Vec<Id>,
    #[cfg(debug_assertions)]
    pub(crate) valid_subcommands: Vec<Id>,
    #[cfg(debug_assertions)]
    pub(crate) disable_asserts: bool,
    pub(crate) args: IndexMap<Id, MatchedArg>,
    pub(crate) subcommand: Option<Box<SubCommand>>,
}

impl ArgMatches {
    /// Check if any args were present on the command line
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let mut cmd = Command::new("myapp")
    ///     .arg(Arg::new("output")
    ///         .takes_value(true));
    ///
    /// let m = cmd
    ///     .try_get_matches_from_mut(vec!["myapp", "something"])
    ///     .unwrap();
    /// assert!(m.args_present());
    ///
    /// let m = cmd
    ///     .try_get_matches_from_mut(vec!["myapp"])
    ///     .unwrap();
    /// assert!(! m.args_present());
    pub fn args_present(&self) -> bool {
        !self.args.is_empty()
    }

    /// Gets the value of a specific option or positional argument.
    ///
    /// i.e. an argument that [takes an additional value][crate::Arg::takes_value] at runtime.
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// *NOTE:* Prefer [`ArgMatches::values_of`] if getting a value for an option or positional
    /// argument that allows multiples as `ArgMatches::value_of` will only return the *first*
    /// value.
    ///
    /// *NOTE:* This will always return `Some(value)` if [`default_value`] has been set.
    /// [`occurrences_of`] can be used to check if a value is present at runtime.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("output")
    ///         .takes_value(true))
    ///     .get_matches_from(vec!["myapp", "something"]);
    ///
    /// assert_eq!(m.value_of("output"), Some("something"));
    /// ```
    /// [option]: crate::Arg::takes_value()
    /// [positional]: crate::Arg::index()
    /// [`ArgMatches::values_of`]: ArgMatches::values_of()
    /// [`default_value`]: crate::Arg::default_value()
    /// [`occurrences_of`]: crate::ArgMatches::occurrences_of()
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn value_of<T: Key>(&self, id: T) -> Option<&str> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_utf8_validation(arg, &id);
        let v = arg.first()?;
        Some(v.to_str().expect(INVALID_UTF8))
    }

    /// Gets the lossy value of a specific option or positional argument.
    ///
    /// i.e. an argument that [takes an additional value][crate::Arg::takes_value] at runtime.
    ///
    /// A lossy value is one which contains invalid UTF-8, those invalid points will be replaced
    /// with `\u{FFFD}`
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// *NOTE:* Recommend having set [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// *NOTE:* Prefer [`ArgMatches::values_of_lossy`] if getting a value for an option or positional
    /// argument that allows multiples as `ArgMatches::value_of_lossy` will only return the *first*
    /// value.
    ///
    /// *NOTE:* This will always return `Some(value)` if [`default_value`] has been set.
    /// [`occurrences_of`] can be used to check if a value is present at runtime.
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    #[cfg_attr(not(unix), doc = " ```ignore")]
    #[cfg_attr(unix, doc = " ```")]
    /// # use clap::{Command, arg};
    /// use std::ffi::OsString;
    /// use std::os::unix::ffi::{OsStrExt,OsStringExt};
    ///
    /// let m = Command::new("utf8")
    ///     .arg(arg!(<arg> "some arg")
    ///         .allow_invalid_utf8(true))
    ///     .get_matches_from(vec![OsString::from("myprog"),
    ///                             // "Hi {0xe9}!"
    ///                             OsString::from_vec(vec![b'H', b'i', b' ', 0xe9, b'!'])]);
    /// assert_eq!(&*m.value_of_lossy("arg").unwrap(), "Hi \u{FFFD}!");
    /// ```
    /// [`default_value`]: crate::Arg::default_value()
    /// [`occurrences_of`]: ArgMatches::occurrences_of()
    /// [`Arg::values_of_lossy`]: ArgMatches::values_of_lossy()
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn value_of_lossy<T: Key>(&self, id: T) -> Option<Cow<'_, str>> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_no_utf8_validation(arg, &id);
        let v = arg.first()?;
        Some(v.to_string_lossy())
    }

    /// Get the `OsStr` value of a specific option or positional argument.
    ///
    /// i.e. an argument that [takes an additional value][crate::Arg::takes_value] at runtime.
    ///
    /// An `OsStr` on Unix-like systems is any series of bytes, regardless of whether or not they
    /// contain valid UTF-8. Since [`String`]s in Rust are guaranteed to be valid UTF-8, a valid
    /// filename on a Unix system as an argument value may contain invalid UTF-8.
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// *NOTE:* Recommend having set [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// *NOTE:* Prefer [`ArgMatches::values_of_os`] if getting a value for an option or positional
    /// argument that allows multiples as `ArgMatches::value_of_os` will only return the *first*
    /// value.
    ///
    /// *NOTE:* This will always return `Some(value)` if [`default_value`] has been set.
    /// [`occurrences_of`] can be used to check if a value is present at runtime.
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    #[cfg_attr(not(unix), doc = " ```ignore")]
    #[cfg_attr(unix, doc = " ```")]
    /// # use clap::{Command, arg};
    /// use std::ffi::OsString;
    /// use std::os::unix::ffi::{OsStrExt,OsStringExt};
    ///
    /// let m = Command::new("utf8")
    ///     .arg(arg!(<arg> "some arg")
    ///         .allow_invalid_utf8(true))
    ///     .get_matches_from(vec![OsString::from("myprog"),
    ///                             // "Hi {0xe9}!"
    ///                             OsString::from_vec(vec![b'H', b'i', b' ', 0xe9, b'!'])]);
    /// assert_eq!(&*m.value_of_os("arg").unwrap().as_bytes(), [b'H', b'i', b' ', 0xe9, b'!']);
    /// ```
    /// [`default_value`]: crate::Arg::default_value()
    /// [`occurrences_of`]: ArgMatches::occurrences_of()
    /// [`ArgMatches::values_of_os`]: ArgMatches::values_of_os()
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn value_of_os<T: Key>(&self, id: T) -> Option<&OsStr> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_no_utf8_validation(arg, &id);
        let v = arg.first()?;
        Some(v.as_os_str())
    }

    /// Get an [`Iterator`] over [values] of a specific option or positional argument.
    ///
    /// i.e. an argument that takes multiple values at runtime.
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("output")
    ///         .multiple_occurrences(true)
    ///         .short('o')
    ///         .takes_value(true))
    ///     .get_matches_from(vec![
    ///         "myprog", "-o", "val1", "-o", "val2", "-o", "val3"
    ///     ]);
    /// let vals: Vec<&str> = m.values_of("output").unwrap().collect();
    /// assert_eq!(vals, ["val1", "val2", "val3"]);
    /// ```
    /// [values]: Values
    /// [`Iterator`]: std::iter::Iterator
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn values_of<T: Key>(&self, id: T) -> Option<Values> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_utf8_validation(arg, &id);
        fn to_str_slice(o: &OsString) -> &str {
            o.to_str().expect(INVALID_UTF8)
        }
        let v = Values {
            iter: arg.vals_flatten().map(to_str_slice),
            len: arg.num_vals(),
        };
        Some(v)
    }

    /// Get an [`Iterator`] over groups of values of a specific option.
    ///
    /// specifically grouped by the occurrences of the options.
    ///
    /// Each group is a `Vec<&str>` containing the arguments passed to a single occurrence
    /// of the option.
    ///
    /// If the option doesn't support multiple occurrences, or there was only a single occurrence,
    /// the iterator will only contain a single item.
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.
    ///
    /// If `id` is not a valid argument or group name.
    ///
    /// # Examples
    /// ```rust
    /// # use clap::{Command,Arg};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("exec")
    ///         .short('x')
    ///         .min_values(1)
    ///         .multiple_occurrences(true)
    ///         .value_terminator(";"))
    ///     .get_matches_from(vec![
    ///         "myprog", "-x", "echo", "hi", ";", "-x", "echo", "bye"]);
    /// let vals: Vec<Vec<&str>> = m.grouped_values_of("exec").unwrap().collect();
    /// assert_eq!(vals, [["echo", "hi"], ["echo", "bye"]]);
    /// ```
    /// [`Iterator`]: std::iter::Iterator
    #[cfg(feature = "unstable-grouped")]
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn grouped_values_of<T: Key>(&self, id: T) -> Option<GroupedValues> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_utf8_validation(arg, &id);
        let v = GroupedValues {
            iter: arg
                .vals()
                .map(|g| g.iter().map(|x| x.to_str().expect(INVALID_UTF8)).collect()),
            len: arg.vals().len(),
        };
        Some(v)
    }

    /// Get the lossy values of a specific option or positional argument.
    ///
    /// i.e. an argument that takes multiple values at runtime.
    ///
    /// A lossy value is one which contains invalid UTF-8, those invalid points will be replaced
    /// with `\u{FFFD}`
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// *NOTE:* Recommend having set [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    #[cfg_attr(not(unix), doc = " ```ignore")]
    #[cfg_attr(unix, doc = " ```")]
    /// # use clap::{Command, arg};
    /// use std::ffi::OsString;
    /// use std::os::unix::ffi::OsStringExt;
    ///
    /// let m = Command::new("utf8")
    ///     .arg(arg!(<arg> ... "some arg")
    ///         .allow_invalid_utf8(true))
    ///     .get_matches_from(vec![OsString::from("myprog"),
    ///                             // "Hi"
    ///                             OsString::from_vec(vec![b'H', b'i']),
    ///                             // "{0xe9}!"
    ///                             OsString::from_vec(vec![0xe9, b'!'])]);
    /// let mut itr = m.values_of_lossy("arg").unwrap().into_iter();
    /// assert_eq!(&itr.next().unwrap()[..], "Hi");
    /// assert_eq!(&itr.next().unwrap()[..], "\u{FFFD}!");
    /// assert_eq!(itr.next(), None);
    /// ```
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn values_of_lossy<T: Key>(&self, id: T) -> Option<Vec<String>> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_no_utf8_validation(arg, &id);
        let v = arg
            .vals_flatten()
            .map(|v| v.to_string_lossy().into_owned())
            .collect();
        Some(v)
    }

    /// Get an [`Iterator`] over [`OsStr`] [values] of a specific option or positional argument.
    ///
    /// i.e. an argument that takes multiple values at runtime.
    ///
    /// An `OsStr` on Unix-like systems is any series of bytes, regardless of whether or not they
    /// contain valid UTF-8. Since [`String`]s in Rust are guaranteed to be valid UTF-8, a valid
    /// filename on a Unix system as an argument value may contain invalid UTF-8.
    ///
    /// Returns `None` if the option wasn't present.
    ///
    /// *NOTE:* Recommend having set [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    #[cfg_attr(not(unix), doc = " ```ignore")]
    #[cfg_attr(unix, doc = " ```")]
    /// # use clap::{Command, arg};
    /// use std::ffi::{OsStr,OsString};
    /// use std::os::unix::ffi::{OsStrExt,OsStringExt};
    ///
    /// let m = Command::new("utf8")
    ///     .arg(arg!(<arg> ... "some arg")
    ///         .allow_invalid_utf8(true))
    ///     .get_matches_from(vec![OsString::from("myprog"),
    ///                                 // "Hi"
    ///                                 OsString::from_vec(vec![b'H', b'i']),
    ///                                 // "{0xe9}!"
    ///                                 OsString::from_vec(vec![0xe9, b'!'])]);
    ///
    /// let mut itr = m.values_of_os("arg").unwrap().into_iter();
    /// assert_eq!(itr.next(), Some(OsStr::new("Hi")));
    /// assert_eq!(itr.next(), Some(OsStr::from_bytes(&[0xe9, b'!'])));
    /// assert_eq!(itr.next(), None);
    /// ```
    /// [`Iterator`]: std::iter::Iterator
    /// [`OsSt`]: std::ffi::OsStr
    /// [values]: OsValues
    /// [`String`]: std::string::String
    #[cfg_attr(debug_assertions, track_caller)]
    pub fn values_of_os<T: Key>(&self, id: T) -> Option<OsValues> {
        let id = Id::from(id);
        let arg = self.get_arg(&id)?;
        assert_no_utf8_validation(arg, &id);
        fn to_str_slice(o: &OsString) -> &OsStr {
            o
        }
        let v = OsValues {
            iter: arg.vals_flatten().map(to_str_slice),
            len: arg.num_vals(),
        };
        Some(v)
    }

    /// Parse the value (with [`FromStr`]) of a specific option or positional argument.
    ///
    /// There are two types of errors, parse failures and those where the argument wasn't present
    /// (such as a non-required argument). Check [`ErrorKind`] to distinguish them.
    ///
    /// *NOTE:* If getting a value for an option or positional argument that allows multiples,
    /// prefer [`ArgMatches::values_of_t`] as this method will only return the *first*
    /// value.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```
    /// # use clap::{Command, arg};
    /// let matches = Command::new("myapp")
    ///               .arg(arg!([length] "Set the length to use as a pos whole num i.e. 20"))
    ///               .get_matches_from(&["test", "12"]);
    ///
    /// // Specify the type explicitly (or use turbofish)
    /// let len: u32 = matches.value_of_t("length").unwrap_or_else(|e| e.exit());
    /// assert_eq!(len, 12);
    ///
    /// // You can often leave the type for rustc to figure out
    /// let also_len = matches.value_of_t("length").unwrap_or_else(|e| e.exit());
    /// // Something that expects u32
    /// let _: u32 = also_len;
    /// ```
    ///
    /// [`FromStr]: std::str::FromStr
    /// [`ArgMatches::values_of_t`]: ArgMatches::values_of_t()
    /// [`ErrorKind`]: crate::ErrorKind
    pub fn value_of_t<R>(&self, name: &str) -> Result<R, Error>
    where
        R: FromStr,
        <R as FromStr>::Err: Display,
    {
        let v = self
            .value_of(name)
            .ok_or_else(|| Error::argument_not_found_auto(name.to_string()))?;
        v.parse::<R>().map_err(|e| {
            let message = format!(
                "The argument '{}' isn't a valid value for '{}': {}",
                v, name, e
            );

            Error::value_validation(name.to_string(), v.to_string(), message.into())
        })
    }

    /// Parse the value (with [`FromStr`]) of a specific option or positional argument.
    ///
    /// If either the value is not present or parsing failed, exits the program.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```
    /// # use clap::{Command, arg};
    /// let matches = Command::new("myapp")
    ///               .arg(arg!([length] "Set the length to use as a pos whole num i.e. 20"))
    ///               .get_matches_from(&["test", "12"]);
    ///
    /// // Specify the type explicitly (or use turbofish)
    /// let len: u32 = matches.value_of_t_or_exit("length");
    /// assert_eq!(len, 12);
    ///
    /// // You can often leave the type for rustc to figure out
    /// let also_len = matches.value_of_t_or_exit("length");
    /// // Something that expects u32
    /// let _: u32 = also_len;
    /// ```
    ///
    /// [`FromStr][std::str::FromStr]
    pub fn value_of_t_or_exit<R>(&self, name: &str) -> R
    where
        R: FromStr,
        <R as FromStr>::Err: Display,
    {
        self.value_of_t(name).unwrap_or_else(|e| e.exit())
    }

    /// Parse the values (with [`FromStr`]) of a specific option or positional argument.
    ///
    /// There are two types of errors, parse failures and those where the argument wasn't present
    /// (such as a non-required argument). Check [`ErrorKind`] to distinguish them.
    ///
    /// *NOTE:* If getting a value for an option or positional argument that allows multiples,
    /// prefer [`ArgMatches::values_of_t`] as this method will only return the *first*
    /// value.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```
    /// # use clap::{Command, arg};
    /// let matches = Command::new("myapp")
    ///               .arg(arg!([length] ... "A sequence of integers because integers are neat!"))
    ///               .get_matches_from(&["test", "12", "77", "40"]);
    ///
    /// // Specify the type explicitly (or use turbofish)
    /// let len: Vec<u32> = matches.values_of_t("length").unwrap_or_else(|e| e.exit());
    /// assert_eq!(len, vec![12, 77, 40]);
    ///
    /// // You can often leave the type for rustc to figure out
    /// let also_len = matches.values_of_t("length").unwrap_or_else(|e| e.exit());
    /// // Something that expects Vec<u32>
    /// let _: Vec<u32> = also_len;
    /// ```
    /// [`ErrorKind`]: crate::ErrorKind
    pub fn values_of_t<R>(&self, name: &str) -> Result<Vec<R>, Error>
    where
        R: FromStr,
        <R as FromStr>::Err: Display,
    {
        let v = self
            .values_of(name)
            .ok_or_else(|| Error::argument_not_found_auto(name.to_string()))?;
        v.map(|v| {
            v.parse::<R>().map_err(|e| {
                let message = format!("The argument '{}' isn't a valid value: {}", v, e);

                Error::value_validation(name.to_string(), v.to_string(), message.into())
            })
        })
        .collect()
    }

    /// Parse the values (with [`FromStr`]) of a specific option or positional argument.
    ///
    /// If parsing (of any value) has failed, exits the program.
    ///
    /// # Panics
    ///
    /// If the value is invalid UTF-8.  See
    /// [`Arg::allow_invalid_utf8`][crate::Arg::allow_invalid_utf8].
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```
    /// # use clap::{Command, arg};
    /// let matches = Command::new("myapp")
    ///               .arg(arg!([length] ... "A sequence of integers because integers are neat!"))
    ///               .get_matches_from(&["test", "12", "77", "40"]);
    ///
    /// // Specify the type explicitly (or use turbofish)
    /// let len: Vec<u32> = matches.values_of_t_or_exit("length");
    /// assert_eq!(len, vec![12, 77, 40]);
    ///
    /// // You can often leave the type for rustc to figure out
    /// let also_len = matches.values_of_t_or_exit("length");
    /// // Something that expects Vec<u32>
    /// let _: Vec<u32> = also_len;
    /// ```
    pub fn values_of_t_or_exit<R>(&self, name: &str) -> Vec<R>
    where
        R: FromStr,
        <R as FromStr>::Err: Display,
    {
        self.values_of_t(name).unwrap_or_else(|e| e.exit())
    }

    /// Check if an argument was present at runtime.
    ///
    /// *NOTE:* This will always return `true` if [`default_value`] has been set.
    /// [`occurrences_of`] can be used to check if a value is present at runtime.
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("debug")
    ///         .short('d'))
    ///     .get_matches_from(vec![
    ///         "myprog", "-d"
    ///     ]);
    ///
    /// assert!(m.is_present("debug"));
    /// ```
    ///
    /// [`default_value`]: crate::Arg::default_value()
    /// [`occurrences_of`]: ArgMatches::occurrences_of()
    pub fn is_present<T: Key>(&self, id: T) -> bool {
        let id = Id::from(id);

        #[cfg(debug_assertions)]
        self.get_arg(&id);

        self.args.contains_key(&id)
    }

    /// Report where argument value came from
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg, ValueSource};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("debug")
    ///         .short('d'))
    ///     .get_matches_from(vec![
    ///         "myprog", "-d"
    ///     ]);
    ///
    /// assert_eq!(m.value_source("debug"), Some(ValueSource::CommandLine));
    /// ```
    ///
    /// [`default_value`]: crate::Arg::default_value()
    /// [`occurrences_of`]: ArgMatches::occurrences_of()
    pub fn value_source<T: Key>(&self, id: T) -> Option<ValueSource> {
        let id = Id::from(id);

        let value = self.get_arg(&id);

        value.and_then(MatchedArg::source)
    }

    /// The number of times an argument was used at runtime.
    ///
    /// If an argument isn't present it will return `0`.
    ///
    /// **NOTE:** This returns the number of times the argument was used, *not* the number of
    /// values. For example, `-o val1 val2 val3 -o val4` would return `2` (2 occurrences, but 4
    /// values).  See [Arg::multiple_occurrences][crate::Arg::multiple_occurrences].
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("debug")
    ///         .short('d')
    ///         .multiple_occurrences(true))
    ///     .get_matches_from(vec![
    ///         "myprog", "-d", "-d", "-d"
    ///     ]);
    ///
    /// assert_eq!(m.occurrences_of("debug"), 3);
    /// ```
    ///
    /// This next example shows that counts actual uses of the argument, not just `-`'s
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myprog")
    ///     .arg(Arg::new("debug")
    ///         .short('d')
    ///         .multiple_occurrences(true))
    ///     .arg(Arg::new("flag")
    ///         .short('f'))
    ///     .get_matches_from(vec![
    ///         "myprog", "-ddfd"
    ///     ]);
    ///
    /// assert_eq!(m.occurrences_of("debug"), 3);
    /// assert_eq!(m.occurrences_of("flag"), 1);
    /// ```
    pub fn occurrences_of<T: Key>(&self, id: T) -> u64 {
        self.get_arg(&Id::from(id))
            .map_or(0, |a| a.get_occurrences())
    }

    /// The first index of that an argument showed up.
    ///
    /// Indices are similar to argv indices, but are not exactly 1:1.
    ///
    /// For flags (i.e. those arguments which don't have an associated value), indices refer
    /// to occurrence of the switch, such as `-f`, or `--flag`. However, for options the indices
    /// refer to the *values* `-o val` would therefore not represent two distinct indices, only the
    /// index for `val` would be recorded. This is by design.
    ///
    /// Besides the flag/option discrepancy, the primary difference between an argv index and clap
    /// index, is that clap continues counting once all arguments have properly separated, whereas
    /// an argv index does not.
    ///
    /// The examples should clear this up.
    ///
    /// *NOTE:* If an argument is allowed multiple times, this method will only give the *first*
    /// index.  See [`ArgMatches::indices_of`].
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// The argv indices are listed in the comments below. See how they correspond to the clap
    /// indices. Note that if it's not listed in a clap index, this is because it's not saved in
    /// in an `ArgMatches` struct for querying.
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("flag")
    ///         .short('f'))
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true))
    ///     .get_matches_from(vec!["myapp", "-f", "-o", "val"]);
    ///            // ARGV indices: ^0       ^1    ^2    ^3
    ///            // clap indices:          ^1          ^3
    ///
    /// assert_eq!(m.index_of("flag"), Some(1));
    /// assert_eq!(m.index_of("option"), Some(3));
    /// ```
    ///
    /// Now notice, if we use one of the other styles of options:
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("flag")
    ///         .short('f'))
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true))
    ///     .get_matches_from(vec!["myapp", "-f", "-o=val"]);
    ///            // ARGV indices: ^0       ^1    ^2
    ///            // clap indices:          ^1       ^3
    ///
    /// assert_eq!(m.index_of("flag"), Some(1));
    /// assert_eq!(m.index_of("option"), Some(3));
    /// ```
    ///
    /// Things become much more complicated, or clear if we look at a more complex combination of
    /// flags. Let's also throw in the final option style for good measure.
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("flag")
    ///         .short('f'))
    ///     .arg(Arg::new("flag2")
    ///         .short('F'))
    ///     .arg(Arg::new("flag3")
    ///         .short('z'))
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true))
    ///     .get_matches_from(vec!["myapp", "-fzF", "-oval"]);
    ///            // ARGV indices: ^0      ^1       ^2
    ///            // clap indices:         ^1,2,3    ^5
    ///            //
    ///            // clap sees the above as 'myapp -f -z -F -o val'
    ///            //                         ^0    ^1 ^2 ^3 ^4 ^5
    /// assert_eq!(m.index_of("flag"), Some(1));
    /// assert_eq!(m.index_of("flag2"), Some(3));
    /// assert_eq!(m.index_of("flag3"), Some(2));
    /// assert_eq!(m.index_of("option"), Some(5));
    /// ```
    ///
    /// One final combination of flags/options to see how they combine:
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("flag")
    ///         .short('f'))
    ///     .arg(Arg::new("flag2")
    ///         .short('F'))
    ///     .arg(Arg::new("flag3")
    ///         .short('z'))
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true))
    ///     .get_matches_from(vec!["myapp", "-fzFoval"]);
    ///            // ARGV indices: ^0       ^1
    ///            // clap indices:          ^1,2,3^5
    ///            //
    ///            // clap sees the above as 'myapp -f -z -F -o val'
    ///            //                         ^0    ^1 ^2 ^3 ^4 ^5
    /// assert_eq!(m.index_of("flag"), Some(1));
    /// assert_eq!(m.index_of("flag2"), Some(3));
    /// assert_eq!(m.index_of("flag3"), Some(2));
    /// assert_eq!(m.index_of("option"), Some(5));
    /// ```
    ///
    /// The last part to mention is when values are sent in multiple groups with a [delimiter].
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .use_value_delimiter(true)
    ///         .multiple_values(true))
    ///     .get_matches_from(vec!["myapp", "-o=val1,val2,val3"]);
    ///            // ARGV indices: ^0       ^1
    ///            // clap indices:             ^2   ^3   ^4
    ///            //
    ///            // clap sees the above as 'myapp -o val1 val2 val3'
    ///            //                         ^0    ^1 ^2   ^3   ^4
    /// assert_eq!(m.index_of("option"), Some(2));
    /// assert_eq!(m.indices_of("option").unwrap().collect::<Vec<_>>(), &[2, 3, 4]);
    /// ```
    /// [delimiter]: crate::Arg::value_delimiter()
    pub fn index_of<T: Key>(&self, id: T) -> Option<usize> {
        let arg = self.get_arg(&Id::from(id))?;
        let i = arg.get_index(0)?;
        Some(i)
    }

    /// All indices an argument appeared at when parsing.
    ///
    /// Indices are similar to argv indices, but are not exactly 1:1.
    ///
    /// For flags (i.e. those arguments which don't have an associated value), indices refer
    /// to occurrence of the switch, such as `-f`, or `--flag`. However, for options the indices
    /// refer to the *values* `-o val` would therefore not represent two distinct indices, only the
    /// index for `val` would be recorded. This is by design.
    ///
    /// *NOTE:* For more information about how clap indices compared to argv indices, see
    /// [`ArgMatches::index_of`]
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid argument or group name.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .use_value_delimiter(true)
    ///         .multiple_values(true))
    ///     .get_matches_from(vec!["myapp", "-o=val1,val2,val3"]);
    ///            // ARGV indices: ^0       ^1
    ///            // clap indices:             ^2   ^3   ^4
    ///            //
    ///            // clap sees the above as 'myapp -o val1 val2 val3'
    ///            //                         ^0    ^1 ^2   ^3   ^4
    /// assert_eq!(m.indices_of("option").unwrap().collect::<Vec<_>>(), &[2, 3, 4]);
    /// ```
    ///
    /// Another quick example is when flags and options are used together
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true)
    ///         .multiple_occurrences(true))
    ///     .arg(Arg::new("flag")
    ///         .short('f')
    ///         .multiple_occurrences(true))
    ///     .get_matches_from(vec!["myapp", "-o", "val1", "-f", "-o", "val2", "-f"]);
    ///            // ARGV indices: ^0       ^1    ^2      ^3    ^4    ^5      ^6
    ///            // clap indices:                ^2      ^3          ^5      ^6
    ///
    /// assert_eq!(m.indices_of("option").unwrap().collect::<Vec<_>>(), &[2, 5]);
    /// assert_eq!(m.indices_of("flag").unwrap().collect::<Vec<_>>(), &[3, 6]);
    /// ```
    ///
    /// One final example, which is an odd case; if we *don't* use  value delimiter as we did with
    /// the first example above instead of `val1`, `val2` and `val3` all being distinc values, they
    /// would all be a single value of `val1,val2,val3`, in which case they'd only receive a single
    /// index.
    ///
    /// ```rust
    /// # use clap::{Command, Arg};
    /// let m = Command::new("myapp")
    ///     .arg(Arg::new("option")
    ///         .short('o')
    ///         .takes_value(true)
    ///         .multiple_values(true))
    ///     .get_matches_from(vec!["myapp", "-o=val1,val2,val3"]);
    ///            // ARGV indices: ^0       ^1
    ///            // clap indices:             ^2
    ///            //
    ///            // clap sees the above as 'myapp -o "val1,val2,val3"'
    ///            //                         ^0    ^1  ^2
    /// assert_eq!(m.indices_of("option").unwrap().collect::<Vec<_>>(), &[2]);
    /// ```
    /// [`ArgMatches::index_of`]: ArgMatches::index_of()
    /// [delimiter]: Arg::value_delimiter()
    pub fn indices_of<T: Key>(&self, id: T) -> Option<Indices<'_>> {
        let arg = self.get_arg(&Id::from(id))?;
        let i = Indices {
            iter: arg.indices(),
            len: arg.num_vals(),
        };
        Some(i)
    }

    /// The name and `ArgMatches` of the current [subcommand].
    ///
    /// Subcommand values are put in a child [`ArgMatches`]
    ///
    /// Returns `None` if the subcommand wasn't present at runtime,
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use clap::{Command, Arg, };
    ///  let app_m = Command::new("git")
    ///      .subcommand(Command::new("clone"))
    ///      .subcommand(Command::new("push"))
    ///      .subcommand(Command::new("commit"))
    ///      .get_matches();
    ///
    /// match app_m.subcommand() {
    ///     Some(("clone",  sub_m)) => {}, // clone was used
    ///     Some(("push",   sub_m)) => {}, // push was used
    ///     Some(("commit", sub_m)) => {}, // commit was used
    ///     _                       => {}, // Either no subcommand or one not tested for...
    /// }
    /// ```
    ///
    /// Another useful scenario is when you want to support third party, or external, subcommands.
    /// In these cases you can't know the subcommand name ahead of time, so use a variable instead
    /// with pattern matching!
    ///
    /// ```rust
    /// # use clap::Command;
    /// // Assume there is an external subcommand named "subcmd"
    /// let app_m = Command::new("myprog")
    ///     .allow_external_subcommands(true)
    ///     .get_matches_from(vec![
    ///         "myprog", "subcmd", "--option", "value", "-fff", "--flag"
    ///     ]);
    ///
    /// // All trailing arguments will be stored under the subcommand's sub-matches using an empty
    /// // string argument name
    /// match app_m.subcommand() {
    ///     Some((external, sub_m)) => {
    ///          let ext_args: Vec<&str> = sub_m.values_of("").unwrap().collect();
    ///          assert_eq!(external, "subcmd");
    ///          assert_eq!(ext_args, ["--option", "value", "-fff", "--flag"]);
    ///     },
    ///     _ => {},
    /// }
    /// ```
    /// [subcommand]: crate::Command::subcommand
    #[inline]
    pub fn subcommand(&self) -> Option<(&str, &ArgMatches)> {
        self.subcommand.as_ref().map(|sc| (&*sc.name, &sc.matches))
    }

    /// The `ArgMatches` for the current [subcommand].
    ///
    /// Subcommand values are put in a child [`ArgMatches`]
    ///
    /// Returns `None` if the subcommand wasn't present at runtime,
    ///
    /// # Panics
    ///
    /// If `id` is is not a valid subcommand.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # use clap::{Command, Arg, };
    /// let app_m = Command::new("myprog")
    ///     .arg(Arg::new("debug")
    ///         .short('d'))
    ///     .subcommand(Command::new("test")
    ///         .arg(Arg::new("opt")
    ///             .long("option")
    ///             .takes_value(true)))
    ///     .get_matches_from(vec![
    ///         "myprog", "-d", "test", "--option", "val"
    ///     ]);
    ///
    /// // Both parent commands, and child subcommands can have arguments present at the same times
    /// assert!(app_m.is_present("debug"));
    ///
    /// // Get the subcommand's ArgMatches instance
    /// if let Some(sub_m) = app_m.subcommand_matches("test") {
    ///     // Use the struct like normal
    ///     assert_eq!(sub_m.value_of("opt"), Some("val"));
    /// }
    /// ```
    ///
    /// [subcommand]: crate::Command::subcommand
    /// [`Command`]: crate::Command
    pub fn subcommand_matches<T: Key>(&self, id: T) -> Option<&ArgMatches> {
        self.get_subcommand(&id.into()).map(|sc| &sc.matches)
    }

    /// The name of the current [subcommand].
    ///
    /// Returns `None` if the subcommand wasn't present at runtime,
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use clap::{Command, Arg, };
    ///  let app_m = Command::new("git")
    ///      .subcommand(Command::new("clone"))
    ///      .subcommand(Command::new("push"))
    ///      .subcommand(Command::new("commit"))
    ///      .get_matches();
    ///
    /// match app_m.subcommand_name() {
    ///     Some("clone")  => {}, // clone was used
    ///     Some("push")   => {}, // push was used
    ///     Some("commit") => {}, // commit was used
    ///     _              => {}, // Either no subcommand or one not tested for...
    /// }
    /// ```
    /// [subcommand]: crate::Command::subcommand
    /// [`Command`]: crate::Command
    #[inline]
    pub fn subcommand_name(&self) -> Option<&str> {
        self.subcommand.as_ref().map(|sc| &*sc.name)
    }

    /// Check if an arg can be queried
    ///
    /// By default, `ArgMatches` functions assert on undefined `Id`s to help catch programmer
    /// mistakes.  In some context, this doesn't work, so users can use this function to check
    /// before they do a query on `ArgMatches`.
    #[inline]
    #[doc(hidden)]
    pub fn is_valid_arg(&self, _id: impl Key) -> bool {
        #[cfg(debug_assertions)]
        {
            let id = Id::from(_id);
            self.disable_asserts || id == Id::empty_hash() || self.valid_args.contains(&id)
        }
        #[cfg(not(debug_assertions))]
        {
            true
        }
    }

    /// Check if a subcommand can be queried
    ///
    /// By default, `ArgMatches` functions assert on undefined `Id`s to help catch programmer
    /// mistakes.  In some context, this doesn't work, so users can use this function to check
    /// before they do a query on `ArgMatches`.
    #[inline]
    #[doc(hidden)]
    pub fn is_valid_subcommand(&self, _id: impl Key) -> bool {
        #[cfg(debug_assertions)]
        {
            let id = Id::from(_id);
            self.disable_asserts || id == Id::empty_hash() || self.valid_subcommands.contains(&id)
        }
        #[cfg(not(debug_assertions))]
        {
            true
        }
    }
}

// Private methods
impl ArgMatches {
    #[inline]
    #[cfg_attr(debug_assertions, track_caller)]
    fn get_arg(&self, arg: &Id) -> Option<&MatchedArg> {
        #[cfg(debug_assertions)]
        {
            if self.disable_asserts || *arg == Id::empty_hash() || self.valid_args.contains(arg) {
            } else if self.valid_subcommands.contains(arg) {
                panic!(
                    "Subcommand `{:?}` used where an argument or group name was expected.",
                    arg
                );
            } else {
                panic!(
                    "`{:?}` is not a name of an argument or a group.\n\
                     Make sure you're using the name of the argument itself \
                     and not the name of short or long flags.",
                    arg
                );
            }
        }

        self.args.get(arg)
    }

    #[inline]
    #[cfg_attr(debug_assertions, track_caller)]
    fn get_subcommand(&self, id: &Id) -> Option<&SubCommand> {
        #[cfg(debug_assertions)]
        {
            if self.disable_asserts
                || *id == Id::empty_hash()
                || self.valid_subcommands.contains(id)
            {
            } else if self.valid_args.contains(id) {
                panic!(
                    "Argument or group `{:?}` used where a subcommand name was expected.",
                    id
                );
            } else {
                panic!("`{:?}` is not a name of a subcommand.", id);
            }
        }

        if let Some(ref sc) = self.subcommand {
            if sc.id == *id {
                return Some(sc);
            }
        }

        None
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct SubCommand {
    pub(crate) id: Id,
    pub(crate) name: String,
    pub(crate) matches: ArgMatches,
}

// The following were taken and adapted from vec_map source
// repo: https://github.com/contain-rs/vec-map
// commit: be5e1fa3c26e351761b33010ddbdaf5f05dbcc33
// license: MIT - Copyright (c) 2015 The Rust Project Developers

/// Iterate over multiple values for an argument via [`ArgMatches::values_of`].
///
/// # Examples
///
/// ```rust
/// # use clap::{Command, Arg};
/// let m = Command::new("myapp")
///     .arg(Arg::new("output")
///         .short('o')
///         .multiple_occurrences(true)
///         .takes_value(true))
///     .get_matches_from(vec!["myapp", "-o", "val1", "-o", "val2"]);
///
/// let mut values = m.values_of("output").unwrap();
///
/// assert_eq!(values.next(), Some("val1"));
/// assert_eq!(values.next(), Some("val2"));
/// assert_eq!(values.next(), None);
/// ```
/// [`ArgMatches::values_of`]: ArgMatches::values_of()
#[derive(Clone, Debug)]
pub struct Values<'a> {
    #[allow(clippy::type_complexity)]
    iter: Map<Flatten<Iter<'a, Vec<OsString>>>, for<'r> fn(&'r OsString) -> &'r str>,
    len: usize,
}

impl<'a> Iterator for Values<'a> {
    type Item = &'a str;

    fn next(&mut self) -> Option<&'a str> {
        self.iter.next()
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len, Some(self.len))
    }
}

impl<'a> DoubleEndedIterator for Values<'a> {
    fn next_back(&mut self) -> Option<&'a str> {
        self.iter.next_back()
    }
}

impl<'a> ExactSizeIterator for Values<'a> {}

/// Creates an empty iterator.
impl<'a> Default for Values<'a> {
    fn default() -> Self {
        static EMPTY: [Vec<OsString>; 0] = [];
        Values {
            iter: EMPTY[..].iter().flatten().map(|_| unreachable!()),
            len: 0,
        }
    }
}

#[derive(Clone)]
#[allow(missing_debug_implementations)]
pub struct GroupedValues<'a> {
    #[allow(clippy::type_complexity)]
    iter: Map<Iter<'a, Vec<OsString>>, fn(&Vec<OsString>) -> Vec<&str>>,
    len: usize,
}

impl<'a> Iterator for GroupedValues<'a> {
    type Item = Vec<&'a str>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next()
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len, Some(self.len))
    }
}

impl<'a> DoubleEndedIterator for GroupedValues<'a> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.next_back()
    }
}

impl<'a> ExactSizeIterator for GroupedValues<'a> {}

/// Creates an empty iterator. Used for `unwrap_or_default()`.
impl<'a> Default for GroupedValues<'a> {
    fn default() -> Self {
        static EMPTY: [Vec<OsString>; 0] = [];
        GroupedValues {
            iter: EMPTY[..].iter().map(|_| unreachable!()),
            len: 0,
        }
    }
}

/// Iterate over multiple values for an argument via [`ArgMatches::values_of_os`].
///
/// # Examples
///
#[cfg_attr(not(unix), doc = " ```ignore")]
#[cfg_attr(unix, doc = " ```")]
/// # use clap::{Command, arg};
/// use std::ffi::OsString;
/// use std::os::unix::ffi::{OsStrExt,OsStringExt};
///
/// let m = Command::new("utf8")
///     .arg(arg!(<arg> "some arg")
///         .allow_invalid_utf8(true))
///     .get_matches_from(vec![OsString::from("myprog"),
///                             // "Hi {0xe9}!"
///                             OsString::from_vec(vec![b'H', b'i', b' ', 0xe9, b'!'])]);
/// assert_eq!(&*m.value_of_os("arg").unwrap().as_bytes(), [b'H', b'i', b' ', 0xe9, b'!']);
/// ```
/// [`ArgMatches::values_of_os`]: ArgMatches::values_of_os()
#[derive(Clone, Debug)]
pub struct OsValues<'a> {
    #[allow(clippy::type_complexity)]
    iter: Map<Flatten<Iter<'a, Vec<OsString>>>, fn(&OsString) -> &OsStr>,
    len: usize,
}

impl<'a> Iterator for OsValues<'a> {
    type Item = &'a OsStr;

    fn next(&mut self) -> Option<&'a OsStr> {
        self.iter.next()
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len, Some(self.len))
    }
}

impl<'a> DoubleEndedIterator for OsValues<'a> {
    fn next_back(&mut self) -> Option<&'a OsStr> {
        self.iter.next_back()
    }
}

impl<'a> ExactSizeIterator for OsValues<'a> {}

/// Creates an empty iterator.
impl Default for OsValues<'_> {
    fn default() -> Self {
        static EMPTY: [Vec<OsString>; 0] = [];
        OsValues {
            iter: EMPTY[..].iter().flatten().map(|_| unreachable!()),
            len: 0,
        }
    }
}

/// Iterate over indices for where an argument appeared when parsing, via [`ArgMatches::indices_of`]
///
/// # Examples
///
/// ```rust
/// # use clap::{Command, Arg};
/// let m = Command::new("myapp")
///     .arg(Arg::new("output")
///         .short('o')
///         .multiple_values(true)
///         .takes_value(true))
///     .get_matches_from(vec!["myapp", "-o", "val1", "val2"]);
///
/// let mut indices = m.indices_of("output").unwrap();
///
/// assert_eq!(indices.next(), Some(2));
/// assert_eq!(indices.next(), Some(3));
/// assert_eq!(indices.next(), None);
/// ```
/// [`ArgMatches::indices_of`]: ArgMatches::indices_of()
#[derive(Clone, Debug)]
pub struct Indices<'a> {
    iter: Cloned<Iter<'a, usize>>,
    len: usize,
}

impl<'a> Iterator for Indices<'a> {
    type Item = usize;

    fn next(&mut self) -> Option<usize> {
        self.iter.next()
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len, Some(self.len))
    }
}

impl<'a> DoubleEndedIterator for Indices<'a> {
    fn next_back(&mut self) -> Option<usize> {
        self.iter.next_back()
    }
}

impl<'a> ExactSizeIterator for Indices<'a> {}

/// Creates an empty iterator.
impl<'a> Default for Indices<'a> {
    fn default() -> Self {
        static EMPTY: [usize; 0] = [];
        // This is never called because the iterator is empty:
        Indices {
            iter: EMPTY[..].iter().cloned(),
            len: 0,
        }
    }
}

#[cfg_attr(debug_assertions, track_caller)]
#[inline]
fn assert_utf8_validation(arg: &MatchedArg, id: &Id) {
    debug_assert!(
        matches!(arg.is_invalid_utf8_allowed(), None | Some(false)),
        "Must use `_os` lookups with `Arg::allow_invalid_utf8` at `{:?}`",
        id
    );
}

#[cfg_attr(debug_assertions, track_caller)]
#[inline]
fn assert_no_utf8_validation(arg: &MatchedArg, id: &Id) {
    debug_assert!(
        matches!(arg.is_invalid_utf8_allowed(), None | Some(true)),
        "Must use `Arg::allow_invalid_utf8` with `_os` lookups at `{:?}`",
        id
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_values() {
        let mut values: Values = Values::default();
        assert_eq!(values.next(), None);
    }

    #[test]
    fn test_default_values_with_shorter_lifetime() {
        let matches = ArgMatches::default();
        let mut values = matches.values_of("").unwrap_or_default();
        assert_eq!(values.next(), None);
    }

    #[test]
    fn test_default_osvalues() {
        let mut values: OsValues = OsValues::default();
        assert_eq!(values.next(), None);
    }

    #[test]
    fn test_default_osvalues_with_shorter_lifetime() {
        let matches = ArgMatches::default();
        let mut values = matches.values_of_os("").unwrap_or_default();
        assert_eq!(values.next(), None);
    }

    #[test]
    fn test_default_indices() {
        let mut indices: Indices = Indices::default();
        assert_eq!(indices.next(), None);
    }

    #[test]
    fn test_default_indices_with_shorter_lifetime() {
        let matches = ArgMatches::default();
        let mut indices = matches.indices_of("").unwrap_or_default();
        assert_eq!(indices.next(), None);
    }

    #[test]
    fn values_exact_size() {
        let l = crate::Command::new("test")
            .arg(
                crate::Arg::new("POTATO")
                    .takes_value(true)
                    .multiple_values(true)
                    .required(true),
            )
            .try_get_matches_from(["test", "one"])
            .unwrap()
            .values_of("POTATO")
            .expect("present")
            .len();
        assert_eq!(l, 1);
    }

    #[test]
    fn os_values_exact_size() {
        let l = crate::Command::new("test")
            .arg(
                crate::Arg::new("POTATO")
                    .takes_value(true)
                    .multiple_values(true)
                    .allow_invalid_utf8(true)
                    .required(true),
            )
            .try_get_matches_from(["test", "one"])
            .unwrap()
            .values_of_os("POTATO")
            .expect("present")
            .len();
        assert_eq!(l, 1);
    }

    #[test]
    fn indices_exact_size() {
        let l = crate::Command::new("test")
            .arg(
                crate::Arg::new("POTATO")
                    .takes_value(true)
                    .multiple_values(true)
                    .required(true),
            )
            .try_get_matches_from(["test", "one"])
            .unwrap()
            .indices_of("POTATO")
            .expect("present")
            .len();
        assert_eq!(l, 1);
    }
}

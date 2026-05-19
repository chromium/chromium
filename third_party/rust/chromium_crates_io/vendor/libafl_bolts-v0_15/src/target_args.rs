//! Shared implementation of afl style arguments

use alloc::{borrow::ToOwned, vec::Vec};
use std::{
    ffi::{OsStr, OsString},
    path::Path,
};

use crate::fs::{InputFile, get_unique_std_input_file};

/// How to deliver input to an external program
/// `StdIn`: The target reads from stdin
/// `File`: The target reads from the specified [`InputFile`]
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InputLocation {
    /// Mutate a commandline argument to deliver an input
    Arg {
        /// The offset of the argument to mutate
        argnum: usize,
    },
    /// Deliver input via `StdIn`
    StdIn {
        /// The alternative input file
        input_file: Option<InputFile>,
    },
    /// Deliver the input via the specified [`InputFile`]
    /// You can use [`InputFile::create`] with [`crate::fs::INPUTFILE_STD`] to use a default filename.
    File {
        /// The file to write input to. The target should read input from this location.
        out_file: InputFile,
    },
}

impl Default for InputLocation {
    fn default() -> Self {
        Self::StdIn { input_file: None }
    }
}

/// The shared inner structs of trait [`StdTargetArgs`]
#[derive(Debug, Clone, Default)]
pub struct StdTargetArgsInner {
    /// Program arguments
    pub arguments: Vec<OsString>,
    /// Program main program
    pub program: Option<OsString>,
    /// Input location, might be stdin or file or cli arg
    pub input_location: InputLocation,
    /// Program environments
    pub envs: Vec<(OsString, OsString)>,
}

/// The main implementation trait of afl style arguments handling
pub trait StdTargetArgs: Sized {
    /// Get inner common arguments
    fn inner(&self) -> &StdTargetArgsInner;

    /// Get mutable inner common arguments
    fn inner_mut(&mut self) -> &mut StdTargetArgsInner;

    /// Adds an environmental var to the harness's commandline
    #[must_use]
    fn env<K, V>(mut self, key: K, val: V) -> Self
    where
        K: AsRef<OsStr>,
        V: AsRef<OsStr>,
    {
        self.inner_mut()
            .envs
            .push((key.as_ref().to_owned(), val.as_ref().to_owned()));
        self
    }

    /// Adds environmental vars to the harness's commandline
    #[must_use]
    fn envs<IT, K, V>(mut self, vars: IT) -> Self
    where
        IT: IntoIterator<Item = (K, V)>,
        K: AsRef<OsStr>,
        V: AsRef<OsStr>,
    {
        let mut res = vec![];
        for (ref key, ref val) in vars {
            res.push((key.as_ref().to_owned(), val.as_ref().to_owned()));
        }
        self.inner_mut().envs.append(&mut res);
        self
    }

    /// If use stdin
    #[must_use]
    fn use_stdin(&self) -> bool {
        matches!(
            &self.inner().input_location,
            InputLocation::StdIn { input_file: _ }
        )
    }

    /// Set input
    #[must_use]
    fn input(mut self, input: InputLocation) -> Self {
        self.inner_mut().input_location = input;
        self
    }

    /// Sets the input mode to [`InputLocation::Arg`] and uses the current arg offset as `argnum`.
    /// During execution, at input will be provided _as argument_ at this position.
    /// Use [`Self::arg_input_file_std`] if you want to provide the input as a file instead.
    #[must_use]
    fn arg_input_arg(mut self) -> Self {
        let argnum = self.inner().arguments.len();
        self = self.input(InputLocation::Arg { argnum });
        // Placeholder arg that gets replaced with the input name later.
        self = self.arg("PLACEHOLDER");
        self
    }

    /// Place the input at this position and set the filename for the input.
    ///
    /// Note: If you use this, you should ensure that there is only one instance using this
    /// file at any given time.
    #[must_use]
    fn arg_input_file<P: AsRef<Path>>(self, path: P) -> Self {
        let mut moved = self.arg(path.as_ref());
        assert!(
            match &moved.inner().input_location {
                InputLocation::File { out_file } => out_file.path.as_path() == path.as_ref(),
                InputLocation::StdIn { input_file } => input_file
                    .as_ref()
                    .is_none_or(|of| of.path.as_path() == path.as_ref()),
                InputLocation::Arg { argnum: _ } => false,
            },
            "Already specified an input file under a different name. This is not supported"
        );
        let out_file = InputFile::create(path).unwrap();
        moved = moved.input(InputLocation::File { out_file });
        moved
    }

    /// Place the input at this position and set the default filename for the input.
    #[must_use]
    /// The filename includes the PID of the fuzzer to ensure that no two fuzzers write to the same file
    fn arg_input_file_std(self) -> Self {
        self.arg_input_file(get_unique_std_input_file())
    }

    /// The harness
    #[must_use]
    fn program<O>(mut self, program: O) -> Self
    where
        O: AsRef<OsStr>,
    {
        self.inner_mut().program = Some(program.as_ref().to_owned());
        self
    }

    /// Adds an argument to the harness's commandline
    ///
    /// You may want to use `parse_afl_cmdline` if you're going to pass `@@`
    /// represents the input file generated by the fuzzer (similar to the `afl-fuzz` command line).
    #[must_use]
    fn arg<O>(mut self, arg: O) -> Self
    where
        O: AsRef<OsStr>,
    {
        self.inner_mut().arguments.push(arg.as_ref().to_owned());
        self
    }

    /// Adds arguments to the harness's commandline
    ///
    /// You may want to use `parse_afl_cmdline` if you're going to pass `@@`
    /// represents the input file generated by the fuzzer (similar to the `afl-fuzz` command line).
    #[must_use]
    fn args<IT, O>(mut self, args: IT) -> Self
    where
        IT: IntoIterator<Item = O>,
        O: AsRef<OsStr>,
    {
        let mut res = vec![];
        for arg in args {
            res.push(arg.as_ref().to_owned());
        }
        self.inner_mut().arguments.append(&mut res);
        self
    }

    #[must_use]
    /// Parse afl style command line
    ///
    /// Replaces `@@` with the path to the input file generated by the fuzzer. If `@@` is omitted,
    /// `stdin` is used to pass the test case instead.
    ///
    /// Interprets the first argument as the path to the program as long as it is not set yet.
    /// You have to omit the program path in case you have set it already. Otherwise
    /// it will be interpreted as a regular argument, leading to probably unintended results.
    fn parse_afl_cmdline<IT, O>(self, args: IT) -> Self
    where
        IT: IntoIterator<Item = O>,
        O: AsRef<OsStr>,
    {
        let mut moved = self;

        let mut use_arg_0_as_program = false;
        if moved.inner().program.is_none() {
            use_arg_0_as_program = true;
        }

        for item in args {
            if use_arg_0_as_program {
                moved = moved.program(item);
                // After the program has been set, unset `use_arg_0_as_program` to treat all
                // subsequent arguments as regular arguments
                use_arg_0_as_program = false;
            } else if item.as_ref() == "@@" {
                match moved.inner().input_location.clone() {
                    InputLocation::File { out_file } => {
                        // If the input file name has been modified, use this one
                        moved = moved.arg_input_file(&out_file.path);
                    }
                    _ => {
                        moved = moved.arg_input_file_std();
                    }
                }
            } else {
                moved = moved.arg(item);
            }
        }

        // If we have not set an input file, use stdin as it is AFLs default
        moved
    }
}

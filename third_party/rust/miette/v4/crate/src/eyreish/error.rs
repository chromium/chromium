use core::any::TypeId;
use core::fmt::{self, Debug, Display};
use core::mem::{self, ManuallyDrop};
use core::ptr::{self, NonNull};
use std::error::Error as StdError;

use super::Report;
use super::ReportHandler;
use crate::chain::Chain;
use crate::eyreish::wrapper::WithSourceCode;
use crate::{Diagnostic, SourceCode};
use core::ops::{Deref, DerefMut};

impl Report {
    /// Create a new error object from any error type.
    ///
    /// The error type must be thread safe and `'static`, so that the `Report`
    /// will be as well.
    ///
    /// If the error type does not provide a backtrace, a backtrace will be
    /// created here to ensure that a backtrace exists.
    #[cfg_attr(track_caller, track_caller)]
    pub fn new<E>(error: E) -> Self
    where
        E: Diagnostic + Send + Sync + 'static,
    {
        Report::from_std(error)
    }

    /// Create a new error object from a printable error message.
    ///
    /// If the argument implements std::error::Error, prefer `Report::new`
    /// instead which preserves the underlying error's cause chain and
    /// backtrace. If the argument may or may not implement std::error::Error
    /// now or in the future, use `miette!(err)` which handles either way
    /// correctly.
    ///
    /// `Report::msg("...")` is equivalent to `miette!("...")` but occasionally
    /// convenient in places where a function is preferable over a macro, such
    /// as iterator or stream combinators:
    ///
    /// ```
    /// # mod ffi {
    /// #     pub struct Input;
    /// #     pub struct Output;
    /// #     pub async fn do_some_work(_: Input) -> Result<Output, &'static str> {
    /// #         unimplemented!()
    /// #     }
    /// # }
    /// #
    /// # use ffi::{Input, Output};
    /// #
    /// use futures::stream::{Stream, StreamExt, TryStreamExt};
    /// use miette::{Report, Result};
    ///
    /// async fn demo<S>(stream: S) -> Result<Vec<Output>>
    /// where
    ///     S: Stream<Item = Input>,
    /// {
    ///     stream
    ///         .then(ffi::do_some_work) // returns Result<Output, &str>
    ///         .map_err(Report::msg)
    ///         .try_collect()
    ///         .await
    /// }
    /// ```
    #[cfg_attr(track_caller, track_caller)]
    pub fn msg<M>(message: M) -> Self
    where
        M: Display + Debug + Send + Sync + 'static,
    {
        Report::from_adhoc(message)
    }

    #[cfg_attr(track_caller, track_caller)]
    pub(crate) fn from_std<E>(error: E) -> Self
    where
        E: Diagnostic + Send + Sync + 'static,
    {
        let vtable = &ErrorVTable {
            object_drop: object_drop::<E>,
            object_ref: object_ref::<E>,
            object_mut: object_mut::<E>,
            object_ref_stderr: object_ref_stderr::<E>,
            object_boxed: object_boxed::<E>,
            object_downcast: object_downcast::<E>,
            object_drop_rest: object_drop_front::<E>,
        };

        // Safety: passing vtable that operates on the right type E.
        let handler = Some(super::capture_handler(&error));

        unsafe { Report::construct(error, vtable, handler) }
    }

    #[cfg_attr(track_caller, track_caller)]
    pub(crate) fn from_adhoc<M>(message: M) -> Self
    where
        M: Display + Debug + Send + Sync + 'static,
    {
        use super::wrapper::MessageError;
        let error: MessageError<M> = MessageError(message);
        let vtable = &ErrorVTable {
            object_drop: object_drop::<MessageError<M>>,
            object_ref: object_ref::<MessageError<M>>,
            object_mut: object_mut::<MessageError<M>>,
            object_ref_stderr: object_ref_stderr::<MessageError<M>>,
            object_boxed: object_boxed::<MessageError<M>>,
            object_downcast: object_downcast::<M>,
            object_drop_rest: object_drop_front::<M>,
        };

        // Safety: MessageError is repr(transparent) so it is okay for the
        // vtable to allow casting the MessageError<M> to M.
        let handler = Some(super::capture_handler(&error));

        unsafe { Report::construct(error, vtable, handler) }
    }

    #[cfg_attr(track_caller, track_caller)]
    pub(crate) fn from_msg<D, E>(msg: D, error: E) -> Self
    where
        D: Display + Send + Sync + 'static,
        E: Diagnostic + Send + Sync + 'static,
    {
        let error: ContextError<D, E> = ContextError { msg, error };

        let vtable = &ErrorVTable {
            object_drop: object_drop::<ContextError<D, E>>,
            object_ref: object_ref::<ContextError<D, E>>,
            object_mut: object_mut::<ContextError<D, E>>,
            object_ref_stderr: object_ref_stderr::<ContextError<D, E>>,
            object_boxed: object_boxed::<ContextError<D, E>>,
            object_downcast: context_downcast::<D, E>,
            object_drop_rest: context_drop_rest::<D, E>,
        };

        // Safety: passing vtable that operates on the right type.
        let handler = Some(super::capture_handler(&error));

        unsafe { Report::construct(error, vtable, handler) }
    }

    #[cfg_attr(track_caller, track_caller)]
    pub(crate) fn from_boxed(error: Box<dyn Diagnostic + Send + Sync>) -> Self {
        use super::wrapper::BoxedError;
        let error = BoxedError(error);
        let handler = Some(super::capture_handler(&error));

        let vtable = &ErrorVTable {
            object_drop: object_drop::<BoxedError>,
            object_ref: object_ref::<BoxedError>,
            object_mut: object_mut::<BoxedError>,
            object_ref_stderr: object_ref_stderr::<BoxedError>,
            object_boxed: object_boxed::<BoxedError>,
            object_downcast: object_downcast::<Box<dyn Diagnostic + Send + Sync>>,
            object_drop_rest: object_drop_front::<Box<dyn Diagnostic + Send + Sync>>,
        };

        // Safety: BoxedError is repr(transparent) so it is okay for the vtable
        // to allow casting to Box<dyn StdError + Send + Sync>.
        unsafe { Report::construct(error, vtable, handler) }
    }

    // Takes backtrace as argument rather than capturing it here so that the
    // user sees one fewer layer of wrapping noise in the backtrace.
    //
    // Unsafe because the given vtable must have sensible behavior on the error
    // value of type E.
    unsafe fn construct<E>(
        error: E,
        vtable: &'static ErrorVTable,
        handler: Option<Box<dyn ReportHandler>>,
    ) -> Self
    where
        E: Diagnostic + Send + Sync + 'static,
    {
        let inner = Box::new(ErrorImpl {
            vtable,
            handler,
            _object: error,
        });
        // Erase the concrete type of E from the compile-time type system. This
        // is equivalent to the safe unsize coercion from Box<ErrorImpl<E>> to
        // Box<ErrorImpl<dyn StdError + Send + Sync + 'static>> except that the
        // result is a thin pointer. The necessary behavior for manipulating the
        // underlying ErrorImpl<E> is preserved in the vtable provided by the
        // caller rather than a builtin fat pointer vtable.
        let erased = mem::transmute::<Box<ErrorImpl<E>>, Box<ErrorImpl<()>>>(inner);
        let inner = ManuallyDrop::new(erased);
        Report { inner }
    }

    /// Create a new error from an error message to wrap the existing error.
    ///
    /// For attaching a higher level error message to a `Result` as it is
    /// propagated, the [crate::WrapErr] extension trait may be more
    /// convenient than this function.
    ///
    /// The primary reason to use `error.wrap_err(...)` instead of
    /// `result.wrap_err(...)` via the `WrapErr` trait would be if the
    /// message needs to depend on some data held by the underlying error:
    pub fn wrap_err<D>(mut self, msg: D) -> Self
    where
        D: Display + Send + Sync + 'static,
    {
        let handler = self.inner.handler.take();
        let error: ContextError<D, Report> = ContextError { msg, error: self };

        let vtable = &ErrorVTable {
            object_drop: object_drop::<ContextError<D, Report>>,
            object_ref: object_ref::<ContextError<D, Report>>,
            object_mut: object_mut::<ContextError<D, Report>>,
            object_ref_stderr: object_ref_stderr::<ContextError<D, Report>>,
            object_boxed: object_boxed::<ContextError<D, Report>>,
            object_downcast: context_chain_downcast::<D>,
            object_drop_rest: context_chain_drop_rest::<D>,
        };

        // Safety: passing vtable that operates on the right type.
        unsafe { Report::construct(error, vtable, handler) }
    }

    /// Compatibility re-export of wrap_err for interop with `anyhow`
    pub fn context<D>(self, msg: D) -> Self
    where
        D: Display + Send + Sync + 'static,
    {
        self.wrap_err(msg)
    }

    /// An iterator of the chain of source errors contained by this Report.
    ///
    /// This iterator will visit every error in the cause chain of this error
    /// object, beginning with the error that this error object was created
    /// from.
    ///
    /// # Example
    ///
    /// ```
    /// use miette::Report;
    /// use std::io;
    ///
    /// pub fn underlying_io_error_kind(error: &Report) -> Option<io::ErrorKind> {
    ///     for cause in error.chain() {
    ///         if let Some(io_error) = cause.downcast_ref::<io::Error>() {
    ///             return Some(io_error.kind());
    ///         }
    ///     }
    ///     None
    /// }
    /// ```
    pub fn chain(&self) -> Chain<'_> {
        self.inner.chain()
    }

    /// The lowest level cause of this error &mdash; this error's cause's
    /// cause's cause etc.
    ///
    /// The root cause is the last error in the iterator produced by
    /// [`chain()`](Report::chain).
    pub fn root_cause(&self) -> &(dyn StdError + 'static) {
        let mut chain = self.chain();
        let mut root_cause = chain.next().unwrap();
        for cause in chain {
            root_cause = cause;
        }
        root_cause
    }

    /// Returns true if `E` is the type held by this error object.
    ///
    /// For errors constructed from messages, this method returns true if `E`
    /// matches the type of the message `D` **or** the type of the error on
    /// which the message has been attached. For details about the
    /// interaction between message and downcasting, [see here].
    ///
    /// [see here]: trait.WrapErr.html#effect-on-downcasting
    pub fn is<E>(&self) -> bool
    where
        E: Display + Debug + Send + Sync + 'static,
    {
        self.downcast_ref::<E>().is_some()
    }

    /// Attempt to downcast the error object to a concrete type.
    pub fn downcast<E>(self) -> Result<E, Self>
    where
        E: Display + Debug + Send + Sync + 'static,
    {
        let target = TypeId::of::<E>();
        unsafe {
            // Use vtable to find NonNull<()> which points to a value of type E
            // somewhere inside the data structure.
            let addr = match (self.inner.vtable.object_downcast)(&self.inner, target) {
                Some(addr) => addr,
                None => return Err(self),
            };

            // Prepare to read E out of the data structure. We'll drop the rest
            // of the data structure separately so that E is not dropped.
            let outer = ManuallyDrop::new(self);

            // Read E from where the vtable found it.
            let error = ptr::read(addr.cast::<E>().as_ptr());

            // Read Box<ErrorImpl<()>> from self. Can't move it out because
            // Report has a Drop impl which we want to not run.
            let inner = ptr::read(&outer.inner);
            let erased = ManuallyDrop::into_inner(inner);

            // Drop rest of the data structure outside of E.
            (erased.vtable.object_drop_rest)(erased, target);

            Ok(error)
        }
    }

    /// Downcast this error object by reference.
    ///
    /// # Example
    ///
    /// ```
    /// # use miette::{Report, miette};
    /// # use std::fmt::{self, Display};
    /// # use std::task::Poll;
    /// #
    /// # #[derive(Debug)]
    /// # enum DataStoreError {
    /// #     Censored(()),
    /// # }
    /// #
    /// # impl Display for DataStoreError {
    /// #     fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
    /// #         unimplemented!()
    /// #     }
    /// # }
    /// #
    /// # impl std::error::Error for DataStoreError {}
    /// #
    /// # const REDACTED_CONTENT: () = ();
    /// #
    /// # let error: Report = miette!("...");
    /// # let root_cause = &error;
    /// #
    /// # let ret =
    /// // If the error was caused by redaction, then return a tombstone instead
    /// // of the content.
    /// match root_cause.downcast_ref::<DataStoreError>() {
    ///     Some(DataStoreError::Censored(_)) => Ok(Poll::Ready(REDACTED_CONTENT)),
    ///     None => Err(error),
    /// }
    /// # ;
    /// ```
    pub fn downcast_ref<E>(&self) -> Option<&E>
    where
        E: Display + Debug + Send + Sync + 'static,
    {
        let target = TypeId::of::<E>();
        unsafe {
            // Use vtable to find NonNull<()> which points to a value of type E
            // somewhere inside the data structure.
            let addr = (self.inner.vtable.object_downcast)(&self.inner, target)?;
            Some(&*addr.cast::<E>().as_ptr())
        }
    }

    /// Downcast this error object by mutable reference.
    pub fn downcast_mut<E>(&mut self) -> Option<&mut E>
    where
        E: Display + Debug + Send + Sync + 'static,
    {
        let target = TypeId::of::<E>();
        unsafe {
            // Use vtable to find NonNull<()> which points to a value of type E
            // somewhere inside the data structure.
            let addr = (self.inner.vtable.object_downcast)(&self.inner, target)?;
            Some(&mut *addr.cast::<E>().as_ptr())
        }
    }

    /// Get a reference to the Handler for this Report.
    pub fn handler(&self) -> &dyn ReportHandler {
        self.inner.handler.as_ref().unwrap().as_ref()
    }

    /// Get a mutable reference to the Handler for this Report.
    pub fn handler_mut(&mut self) -> &mut dyn ReportHandler {
        self.inner.handler.as_mut().unwrap().as_mut()
    }

    /// Provide source code for this error
    pub fn with_source_code(self, source_code: impl SourceCode + Send + Sync + 'static) -> Report {
        WithSourceCode {
            source_code,
            error: self,
        }
        .into()
    }
}

impl<E> From<E> for Report
where
    E: Diagnostic + Send + Sync + 'static,
{
    #[cfg_attr(track_caller, track_caller)]
    fn from(error: E) -> Self {
        Report::from_std(error)
    }
}

impl Deref for Report {
    type Target = dyn Diagnostic + Send + Sync + 'static;

    fn deref(&self) -> &Self::Target {
        self.inner.diagnostic()
    }
}

impl DerefMut for Report {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.inner.diagnostic_mut()
    }
}

impl Display for Report {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.inner.display(formatter)
    }
}

impl Debug for Report {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.inner.debug(formatter)
    }
}

impl Drop for Report {
    fn drop(&mut self) {
        unsafe {
            // Read Box<ErrorImpl<()>> from self.
            let inner = ptr::read(&self.inner);
            let erased = ManuallyDrop::into_inner(inner);

            // Invoke the vtable's drop behavior.
            (erased.vtable.object_drop)(erased);
        }
    }
}

struct ErrorVTable {
    object_drop: unsafe fn(Box<ErrorImpl<()>>),
    object_ref: unsafe fn(&ErrorImpl<()>) -> &(dyn Diagnostic + Send + Sync + 'static),
    object_mut: unsafe fn(&mut ErrorImpl<()>) -> &mut (dyn Diagnostic + Send + Sync + 'static),
    object_ref_stderr: unsafe fn(&ErrorImpl<()>) -> &(dyn StdError + Send + Sync + 'static),
    #[allow(clippy::type_complexity)]
    object_boxed: unsafe fn(Box<ErrorImpl<()>>) -> Box<dyn Diagnostic + Send + Sync + 'static>,
    object_downcast: unsafe fn(&ErrorImpl<()>, TypeId) -> Option<NonNull<()>>,
    object_drop_rest: unsafe fn(Box<ErrorImpl<()>>, TypeId),
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_drop<E>(e: Box<ErrorImpl<()>>) {
    // Cast back to ErrorImpl<E> so that the allocator receives the correct
    // Layout to deallocate the Box's memory.
    let unerased = mem::transmute::<Box<ErrorImpl<()>>, Box<ErrorImpl<E>>>(e);
    drop(unerased);
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_drop_front<E>(e: Box<ErrorImpl<()>>, target: TypeId) {
    // Drop the fields of ErrorImpl other than E as well as the Box allocation,
    // without dropping E itself. This is used by downcast after doing a
    // ptr::read to take ownership of the E.
    let _ = target;
    let unerased = mem::transmute::<Box<ErrorImpl<()>>, Box<ErrorImpl<ManuallyDrop<E>>>>(e);
    drop(unerased);
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_ref<E>(e: &ErrorImpl<()>) -> &(dyn Diagnostic + Send + Sync + 'static)
where
    E: Diagnostic + Send + Sync + 'static,
{
    // Attach E's native StdError vtable onto a pointer to self._object.
    &(*(e as *const ErrorImpl<()> as *const ErrorImpl<E>))._object
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_mut<E>(e: &mut ErrorImpl<()>) -> &mut (dyn Diagnostic + Send + Sync + 'static)
where
    E: Diagnostic + Send + Sync + 'static,
{
    // Attach E's native StdError vtable onto a pointer to self._object.
    &mut (*(e as *mut ErrorImpl<()> as *mut ErrorImpl<E>))._object
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_ref_stderr<E>(e: &ErrorImpl<()>) -> &(dyn StdError + Send + Sync + 'static)
where
    E: StdError + Send + Sync + 'static,
{
    // Attach E's native StdError vtable onto a pointer to self._object.
    &(*(e as *const ErrorImpl<()> as *const ErrorImpl<E>))._object
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_boxed<E>(e: Box<ErrorImpl<()>>) -> Box<dyn Diagnostic + Send + Sync + 'static>
where
    E: Diagnostic + Send + Sync + 'static,
{
    // Attach ErrorImpl<E>'s native StdError vtable. The StdError impl is below.
    mem::transmute::<Box<ErrorImpl<()>>, Box<ErrorImpl<E>>>(e)
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_downcast<E>(e: &ErrorImpl<()>, target: TypeId) -> Option<NonNull<()>>
where
    E: 'static,
{
    if TypeId::of::<E>() == target {
        // Caller is looking for an E pointer and e is ErrorImpl<E>, take a
        // pointer to its E field.
        let unerased = e as *const ErrorImpl<()> as *const ErrorImpl<E>;
        let addr = &(*unerased)._object as *const E as *mut ();
        Some(NonNull::new_unchecked(addr))
    } else {
        None
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, E>>.
unsafe fn context_downcast<D, E>(e: &ErrorImpl<()>, target: TypeId) -> Option<NonNull<()>>
where
    D: 'static,
    E: 'static,
{
    if TypeId::of::<D>() == target {
        let unerased = e as *const ErrorImpl<()> as *const ErrorImpl<ContextError<D, E>>;
        let addr = &(*unerased)._object.msg as *const D as *mut ();
        Some(NonNull::new_unchecked(addr))
    } else if TypeId::of::<E>() == target {
        let unerased = e as *const ErrorImpl<()> as *const ErrorImpl<ContextError<D, E>>;
        let addr = &(*unerased)._object.error as *const E as *mut ();
        Some(NonNull::new_unchecked(addr))
    } else {
        None
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, E>>.
unsafe fn context_drop_rest<D, E>(e: Box<ErrorImpl<()>>, target: TypeId)
where
    D: 'static,
    E: 'static,
{
    // Called after downcasting by value to either the D or the E and doing a
    // ptr::read to take ownership of that value.
    if TypeId::of::<D>() == target {
        let unerased = mem::transmute::<
            Box<ErrorImpl<()>>,
            Box<ErrorImpl<ContextError<ManuallyDrop<D>, E>>>,
        >(e);
        drop(unerased);
    } else {
        let unerased = mem::transmute::<
            Box<ErrorImpl<()>>,
            Box<ErrorImpl<ContextError<D, ManuallyDrop<E>>>>,
        >(e);
        drop(unerased);
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, Report>>.
unsafe fn context_chain_downcast<D>(e: &ErrorImpl<()>, target: TypeId) -> Option<NonNull<()>>
where
    D: 'static,
{
    let unerased = e as *const ErrorImpl<()> as *const ErrorImpl<ContextError<D, Report>>;
    if TypeId::of::<D>() == target {
        let addr = &(*unerased)._object.msg as *const D as *mut ();
        Some(NonNull::new_unchecked(addr))
    } else {
        // Recurse down the context chain per the inner error's vtable.
        let source = &(*unerased)._object.error;
        (source.inner.vtable.object_downcast)(&source.inner, target)
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, Report>>.
unsafe fn context_chain_drop_rest<D>(e: Box<ErrorImpl<()>>, target: TypeId)
where
    D: 'static,
{
    // Called after downcasting by value to either the D or one of the causes
    // and doing a ptr::read to take ownership of that value.
    if TypeId::of::<D>() == target {
        let unerased = mem::transmute::<
            Box<ErrorImpl<()>>,
            Box<ErrorImpl<ContextError<ManuallyDrop<D>, Report>>>,
        >(e);
        // Drop the entire rest of the data structure rooted in the next Report.
        drop(unerased);
    } else {
        let unerased = mem::transmute::<
            Box<ErrorImpl<()>>,
            Box<ErrorImpl<ContextError<D, ManuallyDrop<Report>>>>,
        >(e);
        // Read out a ManuallyDrop<Box<ErrorImpl<()>>> from the next error.
        let inner = ptr::read(&unerased._object.error.inner);
        drop(unerased);
        let erased = ManuallyDrop::into_inner(inner);
        // Recursively drop the next error using the same target typeid.
        (erased.vtable.object_drop_rest)(erased, target);
    }
}

// repr C to ensure that E remains in the final position.
#[repr(C)]
pub(crate) struct ErrorImpl<E> {
    vtable: &'static ErrorVTable,
    pub(crate) handler: Option<Box<dyn ReportHandler>>,
    // NOTE: Don't use directly. Use only through vtable. Erased type may have
    // different alignment.
    _object: E,
}

// repr C to ensure that ContextError<D, E> has the same layout as
// ContextError<ManuallyDrop<D>, E> and ContextError<D, ManuallyDrop<E>>.
#[repr(C)]
pub(crate) struct ContextError<D, E> {
    pub(crate) msg: D,
    pub(crate) error: E,
}

impl<E> ErrorImpl<E> {
    fn erase(&self) -> &ErrorImpl<()> {
        // Erase the concrete type of E but preserve the vtable in self.vtable
        // for manipulating the resulting thin pointer. This is analogous to an
        // unsize coercion.
        unsafe { &*(self as *const ErrorImpl<E> as *const ErrorImpl<()>) }
    }
}

impl ErrorImpl<()> {
    pub(crate) fn error(&self) -> &(dyn StdError + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        unsafe { &*(self.vtable.object_ref_stderr)(self) }
    }

    pub(crate) fn diagnostic(&self) -> &(dyn Diagnostic + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        unsafe { &*(self.vtable.object_ref)(self) }
    }

    pub(crate) fn diagnostic_mut(&mut self) -> &mut (dyn Diagnostic + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        unsafe { &mut *(self.vtable.object_mut)(self) }
    }

    pub(crate) fn chain(&self) -> Chain<'_> {
        Chain::new(self.error())
    }
}

impl<E> StdError for ErrorImpl<E>
where
    E: StdError,
{
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        self.erase().diagnostic().source()
    }
}

impl<E> Diagnostic for ErrorImpl<E> where E: Diagnostic {}

impl<E> Debug for ErrorImpl<E>
where
    E: Debug,
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.erase().debug(formatter)
    }
}

impl<E> Display for ErrorImpl<E>
where
    E: Display,
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.erase().diagnostic(), formatter)
    }
}

impl From<Report> for Box<dyn Diagnostic + Send + Sync + 'static> {
    fn from(error: Report) -> Self {
        let outer = ManuallyDrop::new(error);
        unsafe {
            // Read Box<ErrorImpl<()>> from error. Can't move it out because
            // Report has a Drop impl which we want to not run.
            let inner = ptr::read(&outer.inner);
            let erased = ManuallyDrop::into_inner(inner);

            // Use vtable to attach ErrorImpl<E>'s native StdError vtable for
            // the right original type E.
            (erased.vtable.object_boxed)(erased)
        }
    }
}

impl From<Report> for Box<dyn Diagnostic + 'static> {
    fn from(error: Report) -> Self {
        Box::<dyn Diagnostic + Send + Sync>::from(error)
    }
}

impl AsRef<dyn Diagnostic + Send + Sync> for Report {
    fn as_ref(&self) -> &(dyn Diagnostic + Send + Sync + 'static) {
        &**self
    }
}

impl AsRef<dyn Diagnostic> for Report {
    fn as_ref(&self) -> &(dyn Diagnostic + 'static) {
        &**self
    }
}

impl std::borrow::Borrow<dyn Diagnostic> for Report {
    fn borrow(&self) -> &(dyn Diagnostic + 'static) {
        self.as_ref()
    }
}

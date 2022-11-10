use core::any::TypeId;
use core::fmt::{self, Debug, Display};
use core::mem::ManuallyDrop;
use core::ptr::{self, NonNull};
use std::error::Error as StdError;

use super::ptr::{Mut, Own, Ref};
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

    /// Create a new error object from a boxed [`Diagnostic`].
    ///
    /// The boxed type must be thread safe and 'static, so that the `Report`
    /// will be as well.
    ///
    /// Boxed `Diagnostic`s don't implement `Diagnostic` themselves due to trait coherence issues.
    /// This method allows you to create a `Report` from a boxed `Diagnostic`.
    #[cfg_attr(track_caller, track_caller)]
    pub fn new_boxed(error: Box<dyn Diagnostic + Send + Sync + 'static>) -> Self {
        Report::from_boxed(error)
    }

    #[cfg_attr(track_caller, track_caller)]
    pub(crate) fn from_std<E>(error: E) -> Self
    where
        E: Diagnostic + Send + Sync + 'static,
    {
        let vtable = &ErrorVTable {
            object_drop: object_drop::<E>,
            object_ref: object_ref::<E>,
            object_ref_stderr: object_ref_stderr::<E>,
            object_boxed: object_boxed::<E>,
            object_boxed_stderr: object_boxed_stderr::<E>,
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
            object_ref_stderr: object_ref_stderr::<MessageError<M>>,
            object_boxed: object_boxed::<MessageError<M>>,
            object_boxed_stderr: object_boxed_stderr::<MessageError<M>>,
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
            object_ref_stderr: object_ref_stderr::<ContextError<D, E>>,
            object_boxed: object_boxed::<ContextError<D, E>>,
            object_boxed_stderr: object_boxed_stderr::<ContextError<D, E>>,
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
            object_ref_stderr: object_ref_stderr::<BoxedError>,
            object_boxed: object_boxed::<BoxedError>,
            object_boxed_stderr: object_boxed_stderr::<BoxedError>,
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
        let inner = Own::new(inner).cast::<ErasedErrorImpl>();
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
    pub fn wrap_err<D>(self, msg: D) -> Self
    where
        D: Display + Send + Sync + 'static,
    {
        let handler = unsafe { self.inner.by_mut().deref_mut().handler.take() };
        let error: ContextError<D, Report> = ContextError { msg, error: self };

        let vtable = &ErrorVTable {
            object_drop: object_drop::<ContextError<D, Report>>,
            object_ref: object_ref::<ContextError<D, Report>>,
            object_ref_stderr: object_ref_stderr::<ContextError<D, Report>>,
            object_boxed: object_boxed::<ContextError<D, Report>>,
            object_boxed_stderr: object_boxed_stderr::<ContextError<D, Report>>,
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
        unsafe { ErrorImpl::chain(self.inner.by_ref()) }
    }

    /// The lowest level cause of this error &mdash; this error's cause's
    /// cause's cause etc.
    ///
    /// The root cause is the last error in the iterator produced by
    /// [`chain()`](Report::chain).
    pub fn root_cause(&self) -> &(dyn StdError + 'static) {
        self.chain().last().unwrap()
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
        let inner = self.inner.by_mut();
        unsafe {
            // Use vtable to find NonNull<()> which points to a value of type E
            // somewhere inside the data structure.
            let addr = match (vtable(inner.ptr).object_downcast)(inner.by_ref(), target) {
                Some(addr) => addr.by_mut().extend(),
                None => return Err(self),
            };

            // Prepare to read E out of the data structure. We'll drop the rest
            // of the data structure separately so that E is not dropped.
            let outer = ManuallyDrop::new(self);

            // Read E from where the vtable found it.
            let error = addr.cast::<E>().read();

            // Drop rest of the data structure outside of E.
            (vtable(outer.inner.ptr).object_drop_rest)(outer.inner, target);

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
            let addr = (vtable(self.inner.ptr).object_downcast)(self.inner.by_ref(), target)?;
            Some(addr.cast::<E>().deref())
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
            let addr =
                (vtable(self.inner.ptr).object_downcast)(self.inner.by_ref(), target)?.by_mut();
            Some(addr.cast::<E>().deref_mut())
        }
    }

    /// Get a reference to the Handler for this Report.
    pub fn handler(&self) -> &dyn ReportHandler {
        unsafe {
            self.inner
                .by_ref()
                .deref()
                .handler
                .as_ref()
                .unwrap()
                .as_ref()
        }
    }

    /// Get a mutable reference to the Handler for this Report.
    pub fn handler_mut(&mut self) -> &mut dyn ReportHandler {
        unsafe {
            self.inner
                .by_mut()
                .deref_mut()
                .handler
                .as_mut()
                .unwrap()
                .as_mut()
        }
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
        unsafe { ErrorImpl::diagnostic(self.inner.by_ref()) }
    }
}

impl DerefMut for Report {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { ErrorImpl::diagnostic_mut(self.inner.by_mut()) }
    }
}

impl Display for Report {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe { ErrorImpl::display(self.inner.by_ref(), formatter) }
    }
}

impl Debug for Report {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe { ErrorImpl::debug(self.inner.by_ref(), formatter) }
    }
}

impl Drop for Report {
    fn drop(&mut self) {
        unsafe {
            // Invoke the vtable's drop behavior.
            (vtable(self.inner.ptr).object_drop)(self.inner);
        }
    }
}

struct ErrorVTable {
    object_drop: unsafe fn(Own<ErasedErrorImpl>),
    object_ref:
        unsafe fn(Ref<'_, ErasedErrorImpl>) -> Ref<'_, dyn Diagnostic + Send + Sync + 'static>,
    object_ref_stderr:
        unsafe fn(Ref<'_, ErasedErrorImpl>) -> Ref<'_, dyn StdError + Send + Sync + 'static>,
    #[allow(clippy::type_complexity)]
    object_boxed: unsafe fn(Own<ErasedErrorImpl>) -> Box<dyn Diagnostic + Send + Sync + 'static>,
    #[allow(clippy::type_complexity)]
    object_boxed_stderr:
        unsafe fn(Own<ErasedErrorImpl>) -> Box<dyn StdError + Send + Sync + 'static>,
    object_downcast: unsafe fn(Ref<'_, ErasedErrorImpl>, TypeId) -> Option<Ref<'_, ()>>,
    object_drop_rest: unsafe fn(Own<ErasedErrorImpl>, TypeId),
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_drop<E>(e: Own<ErasedErrorImpl>) {
    // Cast back to ErrorImpl<E> so that the allocator receives the correct
    // Layout to deallocate the Box's memory.
    let unerased = e.cast::<ErrorImpl<E>>().boxed();
    drop(unerased);
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_drop_front<E>(e: Own<ErasedErrorImpl>, target: TypeId) {
    // Drop the fields of ErrorImpl other than E as well as the Box allocation,
    // without dropping E itself. This is used by downcast after doing a
    // ptr::read to take ownership of the E.
    let _ = target;
    let unerased = e.cast::<ErrorImpl<ManuallyDrop<E>>>().boxed();
    drop(unerased);
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_ref<E>(
    e: Ref<'_, ErasedErrorImpl>,
) -> Ref<'_, dyn Diagnostic + Send + Sync + 'static>
where
    E: Diagnostic + Send + Sync + 'static,
{
    // Attach E's native StdError vtable onto a pointer to self._object.
    let unerased = e.cast::<ErrorImpl<E>>();

    Ref::from_raw(NonNull::new_unchecked(
        ptr::addr_of!((*unerased.as_ptr())._object) as *mut E,
    ))
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_ref_stderr<E>(
    e: Ref<'_, ErasedErrorImpl>,
) -> Ref<'_, dyn StdError + Send + Sync + 'static>
where
    E: StdError + Send + Sync + 'static,
{
    // Attach E's native StdError vtable onto a pointer to self._object.
    let unerased = e.cast::<ErrorImpl<E>>();

    Ref::from_raw(NonNull::new_unchecked(
        ptr::addr_of!((*unerased.as_ptr())._object) as *mut E,
    ))
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_boxed<E>(e: Own<ErasedErrorImpl>) -> Box<dyn Diagnostic + Send + Sync + 'static>
where
    E: Diagnostic + Send + Sync + 'static,
{
    // Attach ErrorImpl<E>'s native StdError vtable. The StdError impl is below.
    e.cast::<ErrorImpl<E>>().boxed()
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_boxed_stderr<E>(
    e: Own<ErasedErrorImpl>,
) -> Box<dyn StdError + Send + Sync + 'static>
where
    E: StdError + Send + Sync + 'static,
{
    // Attach ErrorImpl<E>'s native StdError vtable. The StdError impl is below.
    e.cast::<ErrorImpl<E>>().boxed()
}

// Safety: requires layout of *e to match ErrorImpl<E>.
unsafe fn object_downcast<E>(e: Ref<'_, ErasedErrorImpl>, target: TypeId) -> Option<Ref<'_, ()>>
where
    E: 'static,
{
    if TypeId::of::<E>() == target {
        // Caller is looking for an E pointer and e is ErrorImpl<E>, take a
        // pointer to its E field.
        let unerased = e.cast::<ErrorImpl<E>>();

        Some(
            Ref::from_raw(NonNull::new_unchecked(
                ptr::addr_of!((*unerased.as_ptr())._object) as *mut E,
            ))
            .cast::<()>(),
        )
    } else {
        None
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, E>>.
unsafe fn context_downcast<D, E>(e: Ref<'_, ErasedErrorImpl>, target: TypeId) -> Option<Ref<'_, ()>>
where
    D: 'static,
    E: 'static,
{
    if TypeId::of::<D>() == target {
        let unerased = e.cast::<ErrorImpl<ContextError<D, E>>>().deref();
        Some(Ref::new(&unerased._object.msg).cast::<()>())
    } else if TypeId::of::<E>() == target {
        let unerased = e.cast::<ErrorImpl<ContextError<D, E>>>().deref();
        Some(Ref::new(&unerased._object.error).cast::<()>())
    } else {
        None
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, E>>.
unsafe fn context_drop_rest<D, E>(e: Own<ErasedErrorImpl>, target: TypeId)
where
    D: 'static,
    E: 'static,
{
    // Called after downcasting by value to either the D or the E and doing a
    // ptr::read to take ownership of that value.
    if TypeId::of::<D>() == target {
        let unerased = e
            .cast::<ErrorImpl<ContextError<ManuallyDrop<D>, E>>>()
            .boxed();
        drop(unerased);
    } else {
        let unerased = e
            .cast::<ErrorImpl<ContextError<D, ManuallyDrop<E>>>>()
            .boxed();
        drop(unerased);
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, Report>>.
unsafe fn context_chain_downcast<D>(
    e: Ref<'_, ErasedErrorImpl>,
    target: TypeId,
) -> Option<Ref<'_, ()>>
where
    D: 'static,
{
    let unerased = e.cast::<ErrorImpl<ContextError<D, Report>>>().deref();
    if TypeId::of::<D>() == target {
        Some(Ref::new(&unerased._object.msg).cast::<()>())
    } else {
        // Recurse down the context chain per the inner error's vtable.
        let source = &unerased._object.error;
        (vtable(source.inner.ptr).object_downcast)(source.inner.by_ref(), target)
    }
}

// Safety: requires layout of *e to match ErrorImpl<ContextError<D, Report>>.
unsafe fn context_chain_drop_rest<D>(e: Own<ErasedErrorImpl>, target: TypeId)
where
    D: 'static,
{
    // Called after downcasting by value to either the D or one of the causes
    // and doing a ptr::read to take ownership of that value.
    if TypeId::of::<D>() == target {
        let unerased = e
            .cast::<ErrorImpl<ContextError<ManuallyDrop<D>, Report>>>()
            .boxed();
        // Drop the entire rest of the data structure rooted in the next Report.
        drop(unerased);
    } else {
        let unerased = e
            .cast::<ErrorImpl<ContextError<D, ManuallyDrop<Report>>>>()
            .boxed();
        // Read out a ManuallyDrop<Box<ErasedErrorImpl>> from the next error.
        let inner = unerased._object.error.inner;
        drop(unerased);
        let vtable = vtable(inner.ptr);
        // Recursively drop the next error using the same target typeid.
        (vtable.object_drop_rest)(inner, target);
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

type ErasedErrorImpl = ErrorImpl<()>;

// Safety: `ErrorVTable` must be the first field of `ErrorImpl`
unsafe fn vtable(p: NonNull<ErasedErrorImpl>) -> &'static ErrorVTable {
    (p.as_ptr() as *const &'static ErrorVTable).read()
}

impl<E> ErrorImpl<E> {
    fn erase(&self) -> Ref<'_, ErasedErrorImpl> {
        // Erase the concrete type of E but preserve the vtable in self.vtable
        // for manipulating the resulting thin pointer. This is analogous to an
        // unsize coercion.
        Ref::new(self).cast::<ErasedErrorImpl>()
    }
}

impl ErasedErrorImpl {
    pub(crate) unsafe fn error<'a>(
        this: Ref<'a, Self>,
    ) -> &'a (dyn StdError + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        (vtable(this.ptr).object_ref_stderr)(this).deref()
    }

    pub(crate) unsafe fn diagnostic<'a>(
        this: Ref<'a, Self>,
    ) -> &'a (dyn Diagnostic + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        (vtable(this.ptr).object_ref)(this).deref()
    }

    pub(crate) unsafe fn diagnostic_mut<'a>(
        this: Mut<'a, Self>,
    ) -> &'a mut (dyn Diagnostic + Send + Sync + 'static) {
        // Use vtable to attach E's native StdError vtable for the right
        // original type E.
        (vtable(this.ptr).object_ref)(this.by_ref())
            .by_mut()
            .deref_mut()
    }

    pub(crate) unsafe fn chain(this: Ref<'_, Self>) -> Chain<'_> {
        Chain::new(Self::error(this))
    }
}

impl<E> StdError for ErrorImpl<E>
where
    E: StdError,
{
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        unsafe { ErrorImpl::diagnostic(self.erase()).source() }
    }
}

impl<E> Diagnostic for ErrorImpl<E> where E: Diagnostic {}

impl<E> Debug for ErrorImpl<E>
where
    E: Debug,
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe { ErrorImpl::debug(self.erase(), formatter) }
    }
}

impl<E> Display for ErrorImpl<E>
where
    E: Display,
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        unsafe { Display::fmt(ErrorImpl::diagnostic(self.erase()), formatter) }
    }
}

impl From<Report> for Box<dyn Diagnostic + Send + Sync + 'static> {
    fn from(error: Report) -> Self {
        let outer = ManuallyDrop::new(error);
        unsafe {
            // Use vtable to attach ErrorImpl<E>'s native StdError vtable for
            // the right original type E.
            (vtable(outer.inner.ptr).object_boxed)(outer.inner)
        }
    }
}

impl From<Report> for Box<dyn StdError + Send + Sync + 'static> {
    fn from(error: Report) -> Self {
        let outer = ManuallyDrop::new(error);
        unsafe {
            // Use vtable to attach ErrorImpl<E>'s native StdError vtable for
            // the right original type E.
            (vtable(outer.inner.ptr).object_boxed_stderr)(outer.inner)
        }
    }
}

impl From<Report> for Box<dyn Diagnostic + 'static> {
    fn from(error: Report) -> Self {
        Box::<dyn Diagnostic + Send + Sync>::from(error)
    }
}

impl From<Report> for Box<dyn StdError + 'static> {
    fn from(error: Report) -> Self {
        Box::<dyn StdError + Send + Sync>::from(error)
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

impl AsRef<dyn StdError + Send + Sync> for Report {
    fn as_ref(&self) -> &(dyn StdError + Send + Sync + 'static) {
        unsafe { ErrorImpl::error(self.inner.by_ref()) }
    }
}

impl AsRef<dyn StdError> for Report {
    fn as_ref(&self) -> &(dyn StdError + 'static) {
        unsafe { ErrorImpl::error(self.inner.by_ref()) }
    }
}

impl std::borrow::Borrow<dyn Diagnostic> for Report {
    fn borrow(&self) -> &(dyn Diagnostic + 'static) {
        self.as_ref()
    }
}

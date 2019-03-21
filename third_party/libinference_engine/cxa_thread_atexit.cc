extern "C" int __cxa_thread_atexit(void (*func)(), void *obj,
                                   void *dso_symbol) {
  int __cxa_thread_atexit_impl(void (*)(), void *, void *);
  return __cxa_thread_atexit_impl(func, obj, dso_symbol);
}

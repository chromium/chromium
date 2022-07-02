from mako import cache
from mako import runtime

UNDEFINED = runtime.UNDEFINED
__M_dict_builtin = dict
__M_locals_builtin = locals
_magic_number = 5
_modified_time = 1267565427.799504
_template_filename = (
    "/Users/classic/dev/mako/test/templates/subdir/modtest.html"
)
_template_uri = "/subdir/modtest.html"
_template_cache = cache.Cache(__name__, _modified_time)
_source_encoding = None
_exports = []


def render_body(context, **pageargs):
    context.caller_stack._push_frame()
    try:
        __M_locals = __M_dict_builtin(pageargs=pageargs)
        __M_writer = context.writer()
        # SOURCE LINE 1
        __M_writer("this is a test")
        return ""
    finally:
        context.caller_stack._pop_frame()

project(
    'libxml2',
    'c',
    version: '2.13.0',
    license: 'MIT',
    default_options: ['buildtype=debug', 'warning_level=3'],
    meson_version: '>= 0.61',
)

v_array = meson.project_version().split('.')
v_maj = v_array[0]
v_min = v_array[1]
v_mic = v_array[2]
v_nbr = v_maj.to_int() * 10000 + v_min.to_int() * 100 + v_mic.to_int()
v_extra = ''
r = run_command('git', 'describe', check: false)
if (r.returncode() == 0)
    v_extra = '-GIT' + r.stdout().strip()
endif

# install paths
dir_prefix = get_option('prefix')
dir_bin = dir_prefix / get_option('bindir')
dir_include = dir_prefix / get_option('includedir')
dir_pkginclude = dir_include / meson.project_name()
dir_lib = dir_prefix / get_option('libdir')
dir_data = dir_prefix / get_option('datadir')
dir_doc = dir_data / 'doc' / 'libxml2'
dir_locale = dir_prefix / get_option('localedir')

# host

host_os = host_machine.system()

cygwin = 'cygwin'
windows = 'windows'
sys_cygwin = cygwin.contains(host_os)
sys_windows = windows.contains(host_os)

libxml2_cflags = []
xml_cflags = ''

if sys_cygwin or sys_windows
    if get_option('default_library') == 'static'
        xml_cflags = '-DLIBXML_STATIC'
        libxml2_cflags += '-DLIBXML_STATIC'
    endif
endif

# binaries
cc = meson.get_compiler('c')

cc.has_type('uint32_t', prefix: '#include <stdint.h>')

# options
want_c14n = get_option('c14n')
want_catalog = get_option('catalog')
want_debug = get_option('debuging')
want_fexceptions = get_option('fexceptions')
want_ftp = get_option('ftp')
want_history = get_option('history')
want_html = get_option('html')
want_http = get_option('http')
want_iconv = get_option('iconv')
want_icu = get_option('icu')
want_ipv6 = get_option('ipv6')
want_iso8859x = get_option('iso8859x')
want_legacy = get_option('legacy')
want_lzma = get_option('lzma')
want_mem_debug = get_option('mem-debug')
want_modules = get_option('modules')
want_output = get_option('output')
want_pattern = get_option('pattern')
want_push = get_option('push')
want_python = get_option('python')
want_reader = get_option('reader')
want_readline = get_option('readline')
want_regexps = get_option('regexps')
want_run_debug = get_option('run-debug')
want_sax1 = get_option('sax1')
want_schemas = get_option('schemas')
want_schematron = get_option('schematron')
want_threads = get_option('threads')
want_thread_alloc = get_option('thread-alloc')
want_tls = get_option('tls')
want_tree = get_option('tree')
want_valid = get_option('valid')
want_writer = get_option('writer')
want_xinclude = get_option('xinclude')
want_xpath = get_option('xpath')
want_xptr = get_option('xptr')
want_xptr_locs = get_option('xptr-locs')
want_zlib = get_option('zlib')

# hard dependencies on options

if want_c14n == true
    if want_output == false
        message('-Dc14n=true overrides -Doutput')
    endif
    want_output = true
    if want_xpath == false
        message('-Dc14n=true overrides -Dxpath')
    endif
    want_xpath = true
endif

if want_schemas == true
    if want_pattern == false
        message('-Dschemas=true overrides -Dpattern')
    endif
    want_pattern = true
    if want_regexps == false
        message('-Dschemas=true overrides -Dregexps')
    endif
    want_regexps = true
endif

if want_schematron == true
    if want_pattern == false
        message('-Dschematron=true overrides -Dpattern')
    endif
    want_pattern = true
    if want_tree == false
        message('-Dschematron=true overrides -Dtree')
    endif
    want_tree = true
    if want_xpath == false
        message('-Dschematron=true overrides -Dxpath')
    endif
    want_xpath = true
endif

if want_reader == true
    if want_push == false
        message('-Dreader=true overrides -Dpush')
    endif
    want_push = true
    if want_tree == false
        message('-Dreader=true overrides -Dtree')
    endif
    want_tree = true
endif

if want_writer == true
    if want_output == false
        message('-Dwriter=true overrides -Doutput')
    endif
    want_output = true
    if want_push == false
        message('-Dwriter=true overrides -Dpush')
    endif
    want_push = true
endif

if want_xinclude == true
    if want_xpath == false
        message('-Dxinclude=true overrides -Dxpath')
    endif
    want_xpath = true
endif

if want_xptr_locs == true
    if want_xptr == false
        message('-Dxptr-locs=true overrides -Dxptr')
    endif
    want_xptr = true
endif

if want_xptr == true
    if want_xpath == false
        message('-Dxptr=true overrides -Dxpath')
    endif
    want_xpath = true
endif

# minimum dependencies

if get_option('minimum')
    want_c14n = false
    want_catalog = false
    want_debug = false
    want_fexceptions = false
    want_history = false
    want_html = false
    want_http = false
    want_iconv = false
    want_ipv6 = false
    want_iso8859x = false
    want_lzma = false
    want_mem_debug = false
    want_modules = false
    want_output = false
    want_pattern = false
    want_push = false
    want_python = false
    want_reader = false
    want_readline = false
    want_regexps = false
    want_run_debug = false
    want_sax1 = false
    want_schemas = false
    want_schematron = false
    want_threads = false
    want_thread_alloc = false
    want_tree = false
    want_valid = false
    want_writer = false
    want_xinclude = false
    want_xpath = false
    want_xptr = false
    want_xptr_locs = false
    want_zlib = false
else
    # Disable dependent modules
    if want_output == false
        want_c14n = false
        want_writer = false
    endif
    if want_pattern == false
        want_schemas = false
        want_schematron = false
    endif
    if want_push == false
        want_reader = false
        want_writer = false
    endif
    if want_regexps == false
        want_schemas = false
    endif
    if want_tree == false
        want_reader = false
        want_schematron = false
    endif
    if want_xpath == false
        want_c14n = false
        want_schematron = false
        want_xinclude = false
        want_xptr = false
    endif
endif

cflags_try = []

### workaround for native compilers, see configure.ac
if cc.get_argument_syntax() == 'gcc'
    cflags_try += [
        '-Wshadow',
        '-Wpointer-arith',
        '-Wcast-align',
        '-Wwrite-strings',
        '-Wstrict-prototypes',
        '-Wmissing-prototypes',
        '-Wno-long-long',
        '-Wno-format-extra-args',
    ]

    if want_fexceptions == true
        cflags_try += '-fexceptions'
    endif

    if host_machine.cpu_family() == 'alpha'
        cflags_try += '-mieee'
    endif
else
    if host_machine.cpu_family() == 'alpha'
        cflags_try += '-ieee'
    elif host_machine.cpu_family() == 'parisc'
        cflags_try += '-Wp,-H30000'
    endif
endif

foreach cf : cflags_try
    if cc.has_argument(cf)
        libxml2_cflags += cf
    endif
endforeach

# configuration
#
# X : done
# N : not done
#
# [X] config.h.in
# [X] include/libxml/xmlversion.h.in
# [N] libxml-2.0-uninstalled.pc.in
# [X] libxml-2.0.pc.in
# [X] libxml2-config.cmake.in
# [X] python/setup.py.in
# [N] xml2-config.in

## config.h
config_h = configuration_data()
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_BIN_DIR', dir_bin)
config_h.set_quoted('PACKAGE_LIB_DIR', dir_lib)
config_h.set_quoted('PACKAGE_DATA_DIR', dir_data)
config_h.set_quoted('LOCALEDIR', dir_locale)

# header files
xml_check_headers = [
    'stdint.h',
    'inttypes.h',
    'fcntl.h',
    'unistd.h',
    'sys/stat.h',
    'sys/mman.h',
    'sys/socket.h',
    'netinet/in.h',
    'arpa/inet.h',
    'netdb.h',
    'sys/select.h',
    'poll.h',
    'sys/time.h',
    'sys/timeb.h',
    'dl.h',
    'dlfcn.h',
    'glob.h',
]

foreach header : xml_check_headers
    if cc.has_header(header)
        config_h.set10('HAVE_' + header.underscorify().to_upper(), true)
    endif
endforeach

# library functions
xml_check_functions = [
    # fct             | header
    ['gettimeofday', 'sys/time.h'],
    ['ftime', 'sys/timeb.h'],
    ['stat', 'sys/stat.h'],
    ['isascii', 'ctype.h'],
    ['mmap', 'sys/mman.h'],
    ['munmap', 'sys/mman.h'],
]

foreach function : xml_check_functions
    if cc.has_header_symbol(function[1], function[0])
        config_h.set10('HAVE_' + function[0].to_upper(), true)
    endif
endforeach

# library

config_dir = [include_directories('.'), include_directories('include')]

## dependencies

xml_deps = []

### math library
if sys_windows == false
    m_dep = cc.find_library('m', required: false)
    if m_dep.found()
        xml_deps += m_dep
    endif
endif

### thread local storage
support_tls = true
if want_tls == true
    tls_src = '''
#include <threads.h>
int main()
{
    _Thread_local int v;
    return 0;
}
    '''
    res = cc.compiles(tls_src, name: '_Thread_local')
    if res == true
        config_h.set('XML_THREAD_LOCAL', '_Thread_local')
    else
        tls_src = '''
int main()
{
    __thread int v;
    return 0;
}
        '''
        res = cc.compiles(tls_src, name: '__thread')
        if res == true
            config_h.set('XML_THREAD_LOCAL', '__thread')
        else
            tls_src = '''
int main()
{
    __declspec(thread) int v;
    return 0;
}
            '''
            res = cc.compiles(tls_src, name: '__declspec(thread)')
            if res == true
                config_h.set('XML_THREAD_LOCAL', '__declspec(thread)')
            else
                want_tls = false
                support_tls = false
            endif
        endif
    endif
endif

### __attribute__((destructor))
if cc.has_function_attribute('destructor')
    config_h.set10('HAVE_ATTRIBUTE_DESTRUCTOR', true)
    config_h.set('ATTRIBUTE_DESTRUCTOR', '__attribute__((destructor))')
endif

### DSO support
with_modules = false
if want_modules == true
    if sys_cygwin == true
        module_extension = '.dll'
    elif sys_windows == true
        module_extension = '.dll'
        with_modules = true
    else
        module_extension = '.so'
    endif

    if with_modules == false
        dl_dep = dependency('dl', required: false)
        if dl_dep.found()
            config_h.set10('HAVE_DLOPEN', true)
            xml_deps += dl_dep
            with_modules = true
        endif
    endif
endif

### threads
if want_threads == true
    if sys_windows == false
        threads_dep = dependency('threads')
        if threads_dep.found()
            config_h.set10('HAVE_PTHREAD_H', true)
            xml_deps += threads_dep
        else
            want_threads = false
        endif
    endif
endif

if want_threads == true
    thread_cflags = '-D_REENTRANT'
    libxml2_cflags += thread_cflags
endif

want_thread_alloc = (
    (want_threads == true and want_threads == true) ? true : false
)

### xmllint shell history
xmllint_deps = []
if want_history == true and want_readline == true
    termlib_lib = ['ncurses', 'curses', 'termcap', 'terminfo', 'termlib']

    foreach tl : termlib_lib
        termlib_dep = cc.find_library(tl)
        if (
            termlib_dep.found()
            and cc.has_function('tputs', dependencies: termlib_dep)
        )
            xmllint_deps += termlib_dep
            config_h.set10('HAVE_LIB' + tl.underscorify().to_upper(), true)
            break
        endif
    endforeach

    history_dep = dependency('history', required: false)
    if history_dep.found()
        xmllint_deps += history_dep
        config_h.set10('HAVE_LIBHISTORY', true)
    endif

    readline_dep = dependency('readline', required: false)
    if readline_dep.found()
        xmllint_deps += readline_dep
        config_h.set10('HAVE_LIBREADLINE', true)
    endif
endif

### crypto
if sys_windows == true
    bcrypt_dep = cc.find_library('bcrypt', required: true)
    xml_deps += bcrypt_dep
endif

### inet
if want_http == true or want_ftp == true
    if sys_windows == true
        ws2_dep = cc.find_library('ws2_32', required: true)
        xml_deps += ws2_dep
    else
        has_in_libc = cc.has_function('gethostbyname')
        if has_in_libc == false
            nsl_dep = cc.find_library('nsl', required: true)
            if nsl_dep.found()
                has_in_nsl = cc.has_function(
                    'gethostbyname',
                    dependencies: nsl_dep,
                    required: false,
                )
                if has_in_nsl == true
                    xml_deps += nsl_dep
                endif
            endif
        endif
    endif

    ### socket length
    socklen_src = '''
#include <stddef.h>
#ifdef _WIN32
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
#endif
int main()
{
    (void)getsockopt (1, 1, 1, NULL, (socklen_t *)NULL);
    return 0;
}
    '''
    res = cc.compiles(socklen_src, name: 'socket length as socklen_t')
    if res == true
        config_h.set('XML_SOCKLEN_T', 'socklen_t')
    else
        socklen_src = '''
#include <stddef.h>
#include <sys/socket.h>
int main()
{
    (void)getsockopt (1, 1, 1, NULL, (size_t *)NULL);
    return 0;
}
        '''
        res = cc.compiles(socklen_src, name: 'socket length as size_t')
        if res == true
            config_h.set('XML_SOCKLEN_T', 'size_t')
        else
            socklen_src = '''
#include <stddef.h>
#include <sys/socket.h>
int main()
{
    (void)getsockopt (1, 1, 1, NULL, (int *)NULL);
    return 0;
}
            '''
            res = cc.compiles(socklen_src, name: 'socket length as int')
            if res == false
                message('could not determine socket length type, use int')
            endif
            config_h.set('XML_SOCKLEN_T', 'int')
        endif
    endif

    if want_ipv6 == true
        ### IPV6 on Windows has been supported since Windows XP SP1 (around 2003)
        ### see:
        ### https://learn.microsoft.com/en-us/windows/win32/winsock/ipv6-support-2
        ### nevertheless, we check it like autotools
        ipv6_src = '''
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#endif
int main()
{
    struct sockaddr_storage ss;
    socket(AF_INET6, SOCK_STREAM, 0);
    getaddrinfo(0, 0, 0, 0);
    return 0;
}
        '''
        res = cc.compiles(ipv6_src, name: 'support for IPV6')
        if res == true
            config_h.set10('SUPPORT_IP6', true)
        endif
    endif
endif

### zlib
if want_zlib == true
    zlib_dep = dependency('zlib', required: false)
    if zlib_dep.found()
        xml_deps += zlib_dep
    else
        want_zlib = false
    endif
endif

### lzma
if want_lzma == true
    lzma_dep = dependency('liblzma', required: false)
    if lzma_dep.found()
        xml_deps += lzma_dep
    else
        want_lzma = false
    endif
endif

### iconv
if want_iconv == true
    iconv_dep = dependency('iconv', required: false)
    if iconv_dep.found()
        xml_deps += iconv_dep
    else
        want_iconv = false
    endif
endif

if want_iconv == false and want_iso8859x == false
    want_iso8859x = false
else
    want_iso8859x = true
endif

# icu
if want_icu == true
    icu_dep = dependency('icu-i18n', method: 'pkg-config', required: false)
    if icu_dep.found()
        def_var = icu_dep.get_variable(pkgconfig: 'DEFS')
        config_dir += include_directories(def_var)
        xml_deps += icu_dep
    else
        want_icu = false
    endif
endif

subdir('include/libxml')

# Set config_h after all subdirs and dependencies have set values

configure_file(output: 'config.h', configuration: config_h)

## libxml2 library

xml_src = [
    'buf.c',
    'chvalid.c',
    'dict.c',
    'entities.c',
    'encoding.c',
    'error.c',
    'globals.c',
    'hash.c',
    'list.c',
    'parser.c',
    'parserInternals.c',
    'SAX2.c',
    'threads.c',
    'tree.c',
    'uri.c',
    'valid.c',
    'xmlIO.c',
    'xmlmemory.c',
    'xmlstring.c',
]

xml_opt_src = [
    [want_c14n, ['c14n.c']],
    [want_catalog, ['catalog.c']],
    [want_debug, ['debugXML.c']],
    [want_ftp, ['nanoftp.c']],
    [want_html, ['HTMLparser.c', 'HTMLtree.c']],
    [want_http, ['nanohttp.c']],
    [want_legacy, ['legacy.c']],
    [want_lzma, ['xzlib.c']],
    [with_modules, ['xmlmodule.c']],
    [want_output, ['xmlsave.c']],
    [want_pattern, ['pattern.c']],
    [want_reader, ['xmlreader.c']],
    [want_regexps, ['xmlregexp.c', 'xmlunicode.c']],
    [want_sax1, ['SAX.c']],
    [want_schemas, ['relaxng.c', 'xmlschemas.c', 'xmlschemastypes.c']],
    [want_schemas and not want_xpath, ['xpath.c']],
    [want_schematron, ['schematron.c']],
    [want_writer, ['xmlwriter.c']],
    [want_xinclude, ['xinclude.c']],
    [want_xpath, ['xpath.c']],
    [want_xptr, ['xlink.c', 'xpointer.c']],
]

foreach file : xml_opt_src
    want = file[0]
    src = file[1]
    if want == true
        if src.length() > 1
            foreach s : src
                xml_src += s
            endforeach
        else
            xml_src += src
        endif
    endif
endforeach

xml_lib = library(
    'xml2',
    files(xml_src),
    c_args: libxml2_cflags,
    dependencies: xml_deps,
    include_directories: config_dir,
    install: true,
    version: meson.project_version(),
)

xml_dep = declare_dependency(link_with: xml_lib)

## xmllint tool

executable(
    'xmllint',
    files('xmllint.c'),
    dependencies: [xml_dep, xmllint_deps],
    include_directories: config_dir,
    install: true,
)

## xmlcatalog tool

executable(
    'xmlcatalog',
    files('xmlcatalog.c'),
    dependencies: [xml_dep, xmllint_deps],
    include_directories: config_dir,
    install: true,
)

## testdso module

testdso_mod = shared_module(
    'testdso',
    files('testdso.c'),
    build_rpath: get_option('libdir'),
    include_directories: config_dir,
    name_prefix: '',
)

## tests

checks = [
    'runsuite',
    'runtest',
    'runxmlconf',
# Disabled for now, see #694
#    'testModule',
    'testThreads',
    'testapi',
    'testchar',
    'testdict',
    'testlimits',
    'testparser',
    'testrecurse',
]

foreach check : checks
    exe = executable(
        check,
        files(check + '.c'),
        dependencies: xml_dep,
        include_directories: config_dir,
    )
    if check != 'testlimits'
        test(check, exe, timeout: 0, workdir: meson.current_source_dir())
    endif
endforeach

subdir('example')
subdir('doc')

if want_python == true
    subdir('python')
endif

## pc files

pkgmod = import('pkgconfig')

pkgmod.generate(
    xml_lib,
    description: 'libXML library version2.',
    filebase: 'libxml-2.0',
    name: 'libXML',
    variables: 'modules=' + with_modules.to_string('1', '0'),
)

## libxml2-config.cmake file

config_cmake = configuration_data()
config_cmake.set('LIBXML_MAJOR_VERSION', v_maj)
config_cmake.set('LIBXML_MINOR_VERSION', v_min)
config_cmake.set('LIBXML_MICRO_VERSION', v_mic)
config_cmake.set('VERSION', meson.project_version())
config_cmake.set('WITH_ICONV', want_iconv.to_int().to_string())
config_cmake.set('WITH_ICU', want_icu.to_int().to_string())
config_cmake.set('WITH_LZMA', want_lzma.to_int().to_string())
config_cmake.set('WITH_MODULES', want_modules.to_int().to_string())
config_cmake.set('WITH_THREADS', want_threads.to_int().to_string())
config_cmake.set('WITH_ZLIB', want_zlib.to_int().to_string())
config_cmake.set('XML_CFLAGS', xml_cflags)
configure_file(
    input: 'libxml2-config.cmake.in',
    output: 'libxml2-config.cmake',
    configuration: config_cmake,
    install_dir: dir_lib / 'cmake' / 'libxml2',
)

install_data(files('libxml.m4'), install_dir: dir_data / 'aclocal')

install_data(files('xmllint.c'), install_dir: dir_doc / 'examples')

if support_tls == false
    message('===============================================================')
    message('WARNING: Your C compiler appears to not support thread-local')
    message('storage. Future versions of libxml2 will require this feature')
    message('for multi-threading.')
    message('===============================================================\n',
    )
endif

# summary

summary(
    {
        'OS': host_os,
        'c14n': want_c14n,
        'catalog': want_catalog,
        'debug': want_debug,
        'fexceptions': want_fexceptions,
        'ftp': want_ftp,
        'history': want_history,
        'html': want_html,
        'http': want_http,
        'iconv': want_iconv,
        'icu': want_icu,
        'ipv6': want_ipv6,
        'iso8859x': want_iso8859x,
        'legacy': want_legacy,
        'lzma': want_lzma,
        'mem-debug': want_mem_debug,
        'modules': want_modules,
        'output': want_output,
        'pattern': want_pattern,
        'push': want_push,
        'python': want_python,
        'reader': want_reader,
        'readline': want_readline,
        'regexps': want_regexps,
        'run-debug': want_run_debug,
        'sax1': want_sax1,
        'schemas': want_schemas,
        'schematron': want_schematron,
        'threads': want_threads,
        'thread-alloc': want_thread_alloc,
        'tls': want_tls,
        'tree': want_tree,
        'valid': want_valid,
        'writer': want_writer,
        'xinclude': want_xinclude,
        'xpath': want_xpath,
        'xptr': want_xptr,
        'xptr-locs': want_xptr_locs,
        'zlib': want_zlib,
    },
    section: 'Configuration Options Summary:',
)

# basic.py - basic benchmarks adapted from Genshi
# Copyright (C) 2006 Edgewall Software
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#  3. The name of the author may not be used to endorse or promote
#     products derived from this software without specific prior
#     written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from cgi import escape
import os
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO
import sys
import timeit

def u(stringlit):
    if sys.version_info >= (3,):
        return stringlit
    else:
        return stringlit.decode('latin1')

__all__ = ['mako', 'mako_inheritance', 'jinja2', 'jinja2_inheritance',
            'cheetah', 'django', 'myghty', 'genshi', 'kid']

# Templates content and constants
TITLE = 'Just a test'
USER = 'joe'
ITEMS = ['Number %d' % num for num in range(1, 15)]
U_ITEMS = [u(item) for item in ITEMS]

def genshi(dirname, verbose=False):
    from genshi.template import TemplateLoader
    loader = TemplateLoader([dirname], auto_reload=False)
    template = loader.load('template.html')
    def render():
        data = dict(title=TITLE, user=USER, items=ITEMS)
        return template.generate(**data).render('xhtml')

    if verbose:
        print(render())
    return render

def myghty(dirname, verbose=False):
    from myghty import interp
    interpreter = interp.Interpreter(component_root=dirname)
    def render():
        data = dict(title=TITLE, user=USER, items=ITEMS)
        buffer = StringIO()
        interpreter.execute("template.myt", request_args=data, out_buffer=buffer)
        return buffer.getvalue()
    if verbose:
        print(render())
    return render

def mako(dirname, verbose=False):
    from mako.template import Template
    from mako.lookup import TemplateLookup
    disable_unicode = (sys.version_info < (3,))
    lookup = TemplateLookup(directories=[dirname], filesystem_checks=False, disable_unicode=disable_unicode)
    template = lookup.get_template('template.html')
    def render():
        return template.render(title=TITLE, user=USER, list_items=U_ITEMS)
    if verbose:
        print(template.code + " " + render())
    return render
mako_inheritance = mako

def jinja2(dirname, verbose=False):
    from jinja2 import Environment, FileSystemLoader
    env = Environment(loader=FileSystemLoader(dirname))
    template = env.get_template('template.html')
    def render():
        return template.render(title=TITLE, user=USER, list_items=U_ITEMS)
    if verbose:
        print(render())
    return render
jinja2_inheritance = jinja2

def cheetah(dirname, verbose=False):
    from Cheetah.Template import Template
    filename = os.path.join(dirname, 'template.tmpl')
    template = Template(file=filename)
    def render():
        template.__dict__.update({'title': TITLE, 'user': USER,
                                  'list_items': U_ITEMS})
        return template.respond()

    if verbose:
        print(dir(template))
        print(template.generatedModuleCode())
        print(render())
    return render

def django(dirname, verbose=False):
    from django.conf import settings
    settings.configure(TEMPLATE_DIRS=[os.path.join(dirname, 'templates')])
    from django import template, templatetags
    from django.template import loader
    templatetags.__path__.append(os.path.join(dirname, 'templatetags'))
    tmpl = loader.get_template('template.html')

    def render():
        data = {'title': TITLE, 'user': USER, 'items': ITEMS}
        return tmpl.render(template.Context(data))

    if verbose:
        print(render())
    return render

def kid(dirname, verbose=False):
    import kid
    kid.path = kid.TemplatePath([dirname])
    template = kid.Template(file='template.kid')
    def render():
        template = kid.Template(file='template.kid',
                                title=TITLE, user=USER, items=ITEMS)
        return template.serialize(output='xhtml')

    if verbose:
        print(render())
    return render


def run(engines, number=2000, verbose=False):
    basepath = os.path.abspath(os.path.dirname(__file__))
    for engine in engines:
        dirname = os.path.join(basepath, engine)
        if verbose:
            print('%s:' % engine.capitalize())
            print('--------------------------------------------------------')
        else:
            sys.stdout.write('%s:' % engine.capitalize())
        t = timeit.Timer(setup='from __main__ import %s; render = %s(r"%s", %s)'
                                       % (engine, engine, dirname, verbose),
                                 stmt='render()')

        time = t.timeit(number=number) / number
        if verbose:
            print('--------------------------------------------------------')
        print('%.2f ms' % (1000 * time))
        if verbose:
            print('--------------------------------------------------------')


if __name__ == '__main__':
    engines = [arg for arg in sys.argv[1:] if arg[0] != '-']
    if not engines:
        engines = __all__

    verbose = '-v' in sys.argv

    if '-p' in sys.argv:
        try:
            import hotshot, hotshot.stats
            prof = hotshot.Profile("template.prof")
            benchtime = prof.runcall(run, engines, number=100, verbose=verbose)
            stats = hotshot.stats.load("template.prof")
        except ImportError:
            import cProfile, pstats
            stmt = "run(%r, number=%r, verbose=%r)" % (engines, 1000, verbose)
            cProfile.runctx(stmt, globals(), {}, "template.prof")
            stats = pstats.Stats("template.prof")
        stats.strip_dirs()
        stats.sort_stats('time', 'calls')
        stats.print_stats()
    else:
        run(engines, verbose=verbose)

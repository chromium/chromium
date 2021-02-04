import os
import re
import sys

from setuptools import find_packages
from setuptools import setup
from setuptools.command.test import test as TestCommand

v = open(os.path.join(os.path.dirname(__file__), "mako", "__init__.py"))
VERSION = (
    re.compile(r".*__version__ = [\"'](.*?)[\"']", re.S)
    .match(v.read())
    .group(1)
)
v.close()

readme = os.path.join(os.path.dirname(__file__), "README.rst")

install_requires = ["MarkupSafe>=0.9.2"]


class UseTox(TestCommand):
    RED = 31
    RESET_SEQ = "\033[0m"
    BOLD_SEQ = "\033[1m"
    COLOR_SEQ = "\033[1;%dm"

    def run_tests(self):
        sys.stderr.write(
            "%s%spython setup.py test is deprecated by PyPA.  Please invoke "
            "'tox' with no arguments for a basic test run.\n%s"
            % (self.COLOR_SEQ % self.RED, self.BOLD_SEQ, self.RESET_SEQ)
        )
        sys.exit(1)


setup(
    name="Mako",
    version=VERSION,
    description="A super-fast templating language that borrows the \
 best ideas from the existing templating languages.",
    long_description=open(readme).read(),
    python_requires=">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: MIT License",
        "Environment :: Web Environment",
        "Intended Audience :: Developers",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
        "Topic :: Internet :: WWW/HTTP :: Dynamic Content",
    ],
    keywords="templates",
    author="Mike Bayer",
    author_email="mike@zzzcomputing.com",
    url="https://www.makotemplates.org/",
    project_urls={
        "Documentation": "https://docs.makotemplates.org",
        "Issue Tracker": "https://github.com/sqlalchemy/mako",
    },
    license="MIT",
    packages=find_packages(".", exclude=["examples*", "test*"]),
    cmdclass={"test": UseTox},
    zip_safe=False,
    install_requires=install_requires,
    entry_points="""
      [python.templating.engines]
      mako = mako.ext.turbogears:TGPlugin

      [pygments.lexers]
      mako = mako.ext.pygmentplugin:MakoLexer
      html+mako = mako.ext.pygmentplugin:MakoHtmlLexer
      xml+mako = mako.ext.pygmentplugin:MakoXmlLexer
      js+mako = mako.ext.pygmentplugin:MakoJavascriptLexer
      css+mako = mako.ext.pygmentplugin:MakoCssLexer

      [babel.extractors]
      mako = mako.ext.babelplugin:extract [babel]

      [lingua.extractors]
      mako = mako.ext.linguaplugin:LinguaMakoExtractor [lingua]

      [console_scripts]
      mako-render = mako.cmd:cmdline
      """,
    extras_require={
        'babel': [
            'Babel',
        ],
        'lingua': [
            'lingua',
        ],
    },
)

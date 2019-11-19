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

readme = open(os.path.join(os.path.dirname(__file__), "README.rst")).read()

if sys.version_info < (2, 6):
    raise Exception("Mako requires Python 2.6 or higher.")

markupsafe_installs = (
    sys.version_info >= (2, 6) and sys.version_info < (3, 0)
) or sys.version_info >= (3, 3)

install_requires = []

if markupsafe_installs:
    install_requires.append("MarkupSafe>=0.9.2")

try:
    import argparse  # noqa
except ImportError:
    install_requires.append("argparse")


class PyTest(TestCommand):
    user_options = [("pytest-args=", "a", "Arguments to pass to py.test")]

    def initialize_options(self):
        TestCommand.initialize_options(self)
        self.pytest_args = []

    def finalize_options(self):
        TestCommand.finalize_options(self)
        self.test_args = []
        self.test_suite = True

    def run_tests(self):
        # import here, cause outside the eggs aren't loaded
        import pytest

        errno = pytest.main(self.pytest_args)
        sys.exit(errno)


setup(
    name="Mako",
    version=VERSION,
    description="A super-fast templating language that borrows the \
 best ideas from the existing templating languages.",
    long_description=readme,
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: MIT License",
        "Environment :: Web Environment",
        "Intended Audience :: Developers",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
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
        "Issue Tracker": "https://github.com/sqlalchemy/mako"
    },
    license="MIT",
    packages=find_packages(".", exclude=["examples*", "test*"]),
    tests_require=["pytest", "mock"],
    cmdclass={"test": PyTest},
    zip_safe=False,
    python_requires=">=2.6",
    install_requires=install_requires,
    extras_require={},
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
      mako = mako.ext.babelplugin:extract

      [lingua.extractors]
      mako = mako.ext.linguaplugin:LinguaMakoExtractor

      [console_scripts]
      mako-render = mako.cmd:cmdline
      """,
)

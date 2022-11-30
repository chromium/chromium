from mako import ast
from mako import exceptions
from mako import pyparser
from mako.testing.assertions import assert_raises
from mako.testing.assertions import eq_

exception_kwargs = {"source": "", "lineno": 0, "pos": 0, "filename": ""}


class AstParseTest:
    def test_locate_identifiers(self):
        """test the location of identifiers in a python code string"""
        code = """
a = 10
b = 5
c = x * 5 + a + b + q
(g,h,i) = (1,2,3)
[u,k,j] = [4,5,6]
foo.hoho.lala.bar = 7 + gah.blah + u + blah
for lar in (1,2,3):
    gh = 5
    x = 12
("hello world, ", a, b)
("Another expr", c)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(
            parsed.declared_identifiers,
            {"a", "b", "c", "g", "h", "i", "u", "k", "j", "gh", "lar", "x"},
        )
        eq_(
            parsed.undeclared_identifiers,
            {"x", "q", "foo", "gah", "blah"},
        )

        parsed = ast.PythonCode("x + 5 * (y-z)", **exception_kwargs)
        assert parsed.undeclared_identifiers == {"x", "y", "z"}
        assert parsed.declared_identifiers == set()

    def test_locate_identifiers_2(self):
        code = """
import foobar
from lala import hoho, yaya
import bleep as foo
result = []
data = get_data()
for x in data:
    result.append(x+7)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"get_data"})
        eq_(
            parsed.declared_identifiers,
            {"result", "data", "x", "hoho", "foobar", "foo", "yaya"},
        )

    def test_locate_identifiers_3(self):
        """test that combination assignment/expressions
        of the same identifier log the ident as 'undeclared'"""
        code = """
x = x + 5
for y in range(1, y):
    ("hi",)
[z for z in range(1, z)]
(q for q in range (1, q))
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"x", "y", "z", "q", "range"})

    def test_locate_identifiers_4(self):
        code = """
x = 5
(y, )
def mydef(mydefarg):
    print("mda is", mydefarg)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"y"})
        eq_(parsed.declared_identifiers, {"mydef", "x"})

    def test_locate_identifiers_5(self):
        code = """
try:
    print(x)
except:
    print(y)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"x", "y"})

    def test_locate_identifiers_6(self):
        code = """
def foo():
    return bar()
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"bar"})

        code = """
def lala(x, y):
    return x, y, z
print(x)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"z", "x"})
        eq_(parsed.declared_identifiers, {"lala"})

        code = """
def lala(x, y):
    def hoho():
        def bar():
            z = 7
print(z)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"z"})
        eq_(parsed.declared_identifiers, {"lala"})

    def test_locate_identifiers_7(self):
        code = """
import foo.bar
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"foo"})
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_8(self):
        code = """
class Hi:
    foo = 7
    def hoho(self):
        x = 5
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"Hi"})
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_9(self):
        code = """
    ",".join([t for t in ("a", "b", "c")])
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"t"})
        eq_(parsed.undeclared_identifiers, {"t"})

        code = """
    [(val, name) for val, name in x]
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"val", "name"})
        eq_(parsed.undeclared_identifiers, {"val", "name", "x"})

    def test_locate_identifiers_10(self):
        code = """
lambda q: q + 5
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, set())
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_11(self):
        code = """
def x(q):
    return q + 5
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"x"})
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_12(self):
        code = """
def foo():
    s = 1
    def bar():
        t = s
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"foo"})
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_13(self):
        code = """
def foo():
    class Bat:
        pass
    Bat
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"foo"})
        eq_(parsed.undeclared_identifiers, set())

    def test_locate_identifiers_14(self):
        code = """
def foo():
    class Bat:
        pass
    Bat

print(Bat)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.declared_identifiers, {"foo"})
        eq_(parsed.undeclared_identifiers, {"Bat"})

    def test_locate_identifiers_16(self):
        code = """
try:
    print(x)
except Exception as e:
    print(y)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"x", "y", "Exception"})

    def test_locate_identifiers_17(self):
        code = """
try:
    print(x)
except (Foo, Bar) as e:
    print(y)
"""
        parsed = ast.PythonCode(code, **exception_kwargs)
        eq_(parsed.undeclared_identifiers, {"x", "y", "Foo", "Bar"})

    def test_no_global_imports(self):
        code = """
from foo import *
import x as bar
"""
        assert_raises(
            exceptions.CompileException,
            ast.PythonCode,
            code,
            **exception_kwargs,
        )

    def test_python_fragment(self):
        parsed = ast.PythonFragment("for x in foo:", **exception_kwargs)
        eq_(parsed.declared_identifiers, {"x"})
        eq_(parsed.undeclared_identifiers, {"foo"})

        parsed = ast.PythonFragment("try:", **exception_kwargs)

        parsed = ast.PythonFragment(
            "except MyException as e:", **exception_kwargs
        )
        eq_(parsed.declared_identifiers, {"e"})
        eq_(parsed.undeclared_identifiers, {"MyException"})

    def test_argument_list(self):
        parsed = ast.ArgumentList(
            "3, 5, 'hi', x+5, " "context.get('lala')", **exception_kwargs
        )
        eq_(parsed.undeclared_identifiers, {"x", "context"})
        eq_(
            [x for x in parsed.args],
            ["3", "5", "'hi'", "(x + 5)", "context.get('lala')"],
        )

        parsed = ast.ArgumentList("h", **exception_kwargs)
        eq_(parsed.args, ["h"])

    def test_function_decl(self):
        """test getting the arguments from a function"""
        code = "def foo(a, b, c=None, d='hi', e=x, f=y+7):pass"
        parsed = ast.FunctionDecl(code, **exception_kwargs)
        eq_(parsed.funcname, "foo")
        eq_(parsed.argnames, ["a", "b", "c", "d", "e", "f"])
        eq_(parsed.kwargnames, [])

    def test_function_decl_2(self):
        """test getting the arguments from a function"""
        code = "def foo(a, b, c=None, *args, **kwargs):pass"
        parsed = ast.FunctionDecl(code, **exception_kwargs)
        eq_(parsed.funcname, "foo")
        eq_(parsed.argnames, ["a", "b", "c", "args"])
        eq_(parsed.kwargnames, ["kwargs"])

    def test_function_decl_3(self):
        """test getting the arguments from a fancy py3k function"""
        code = "def foo(a, b, *c, d, e, **f):pass"
        parsed = ast.FunctionDecl(code, **exception_kwargs)
        eq_(parsed.funcname, "foo")
        eq_(parsed.argnames, ["a", "b", "c"])
        eq_(parsed.kwargnames, ["d", "e", "f"])

    def test_expr_generate(self):
        """test the round trip of expressions to AST back to python source"""
        x = 1
        y = 2

        class F:
            def bar(self, a, b):
                return a + b

        def lala(arg):
            return "blah" + arg

        local_dict = dict(x=x, y=y, foo=F(), lala=lala)

        code = "str((x+7*y) / foo.bar(5,6)) + lala('ho')"
        astnode = pyparser.parse(code)
        newcode = pyparser.ExpressionGenerator(astnode).value()
        eq_(eval(code, local_dict), eval(newcode, local_dict))

        a = ["one", "two", "three"]
        hoho = {"somevalue": "asdf"}
        g = [1, 2, 3, 4, 5]
        local_dict = dict(a=a, hoho=hoho, g=g)
        code = (
            "a[2] + hoho['somevalue'] + "
            "repr(g[3:5]) + repr(g[3:]) + repr(g[:5])"
        )
        astnode = pyparser.parse(code)
        newcode = pyparser.ExpressionGenerator(astnode).value()
        eq_(eval(code, local_dict), eval(newcode, local_dict))

        local_dict = {"f": lambda: 9, "x": 7}
        code = "x+f()"
        astnode = pyparser.parse(code)
        newcode = pyparser.ExpressionGenerator(astnode).value()
        eq_(eval(code, local_dict), eval(newcode, local_dict))

        for code in [
            "repr({'x':7,'y':18})",
            "repr([])",
            "repr({})",
            "repr([{3:[]}])",
            "repr({'x':37*2 + len([6,7,8])})",
            "repr([1, 2, {}, {'x':'7'}])",
            "repr({'x':-1})",
            "repr(((1,2,3), (4,5,6)))",
            "repr(1 and 2 and 3 and 4)",
            "repr(True and False or 55)",
            "repr(lambda x, y: (x + y))",
            "repr(lambda *arg, **kw: arg, kw)",
            "repr(1 & 2 | 3)",
            "repr(3//5)",
            "repr(3^5)",
            "repr([q.endswith('e') for q in " "['one', 'two', 'three']])",
            "repr([x for x in (5,6,7) if x == 6])",
            "repr(not False)",
        ]:
            local_dict = {}
            astnode = pyparser.parse(code)
            newcode = pyparser.ExpressionGenerator(astnode).value()
            if "lambda" in code:
                eq_(code, newcode)
            else:
                eq_(eval(code, local_dict), eval(newcode, local_dict))

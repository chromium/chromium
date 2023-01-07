from io import StringIO

from mako.pygen import adjust_whitespace
from mako.pygen import PythonPrinter
from mako.testing.assertions import eq_


class GeneratePythonTest:
    def test_generate_normal(self):
        stream = StringIO()
        printer = PythonPrinter(stream)
        printer.writeline("import lala")
        printer.writeline("for x in foo:")
        printer.writeline("print x")
        printer.writeline(None)
        printer.writeline("print y")
        assert (
            stream.getvalue()
            == """import lala
for x in foo:
    print x
print y
"""
        )

    def test_generate_adjusted(self):
        block = """
        x = 5 +6
        if x > 7:
            for y in range(1,5):
                print "<td>%s</td>" % y
"""
        stream = StringIO()
        printer = PythonPrinter(stream)
        printer.write_indented_block(block)
        printer.close()
        # print stream.getvalue()
        assert (
            stream.getvalue()
            == """
x = 5 +6
if x > 7:
    for y in range(1,5):
        print "<td>%s</td>" % y

"""
        )

    def test_generate_combo(self):
        block = """
                x = 5 +6
                if x > 7:
                    for y in range(1,5):
                        print "<td>%s</td>" % y
                    print "hi"
                print "there"
                foo(lala)
"""
        stream = StringIO()
        printer = PythonPrinter(stream)
        printer.writeline("import lala")
        printer.writeline("for x in foo:")
        printer.writeline("print x")
        printer.write_indented_block(block)
        printer.writeline(None)
        printer.writeline("print y")
        printer.close()
        # print "->" + stream.getvalue().replace(' ', '#') + "<-"
        eq_(
            stream.getvalue(),
            """import lala
for x in foo:
    print x

    x = 5 +6
    if x > 7:
        for y in range(1,5):
            print "<td>%s</td>" % y
        print "hi"
    print "there"
    foo(lala)

print y
""",
        )

    def test_multi_line(self):
        block = """
    if test:
        print ''' this is a block of stuff.
this is more stuff in the block.
and more block.
'''
        do_more_stuff(g)
"""
        stream = StringIO()
        printer = PythonPrinter(stream)
        printer.write_indented_block(block)
        printer.close()
        # print stream.getvalue()
        assert (
            stream.getvalue()
            == """
if test:
    print ''' this is a block of stuff.
this is more stuff in the block.
and more block.
'''
    do_more_stuff(g)

"""
        )

    def test_false_unindentor(self):
        stream = StringIO()
        printer = PythonPrinter(stream)
        for line in [
            "try:",
            "elsemyvar = 12",
            "if True:",
            "print 'hi'",
            None,
            "finally:",
            "dosomething",
            None,
        ]:
            printer.writeline(line)

        assert (
            stream.getvalue()
            == """try:
    elsemyvar = 12
    if True:
        print 'hi'
finally:
    dosomething
"""
        ), stream.getvalue()

    def test_backslash_line(self):
        block = """
            # comment
    if test:
        if (lala + hoho) + \\
(foobar + blat) == 5:
            print "hi"
    print "more indent"
"""
        stream = StringIO()
        printer = PythonPrinter(stream)
        printer.write_indented_block(block)
        printer.close()
        assert (
            stream.getvalue()
            == """
            # comment
if test:
    if (lala + hoho) + \\
(foobar + blat) == 5:
        print "hi"
print "more indent"

"""
        )


class WhitespaceTest:
    def test_basic(self):
        text = """
        for x in range(0,15):
            print x
        print "hi"
        """
        assert (
            adjust_whitespace(text)
            == """
for x in range(0,15):
    print x
print "hi"
"""
        )

    def test_blank_lines(self):
        text = """
    print "hi"  # a comment

    # more comments

    print g
"""
        assert (
            adjust_whitespace(text)
            == """
print "hi"  # a comment

# more comments

print g
"""
        )

    def test_open_quotes_with_pound(self):
        text = '''
        print """  this is text
          # and this is text
        # and this is too """
'''
        assert (
            adjust_whitespace(text)
            == '''
print """  this is text
          # and this is text
        # and this is too """
'''
        )

    def test_quote_with_comments(self):
        text = """
            print 'hi'
            # this is a comment
            # another comment
            x = 7 # someone's '''comment
            print '''
        there
        '''
            # someone else's comment
"""

        assert (
            adjust_whitespace(text)
            == """
print 'hi'
# this is a comment
# another comment
x = 7 # someone's '''comment
print '''
        there
        '''
# someone else's comment
"""
        )

    def test_quotes_with_pound(self):
        text = '''
        if True:
            """#"""
        elif False:
            "bar"
'''
        assert (
            adjust_whitespace(text)
            == '''
if True:
    """#"""
elif False:
    "bar"
'''
        )

    def test_quotes(self):
        text = """
        print ''' aslkjfnas kjdfn
askdjfnaskfd fkasnf dknf sadkfjn asdkfjna sdakjn
asdkfjnads kfajns '''
        if x:
            print y
"""
        assert (
            adjust_whitespace(text)
            == """
print ''' aslkjfnas kjdfn
askdjfnaskfd fkasnf dknf sadkfjn asdkfjna sdakjn
asdkfjnads kfajns '''
if x:
    print y
"""
        )

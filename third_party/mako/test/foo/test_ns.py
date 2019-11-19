def foo1(context):
    context.write("this is foo1.")
    return ""


def foo2(context, x):
    context.write("this is foo2, x is " + x)
    return ""


foo3 = "I'm not a callable"

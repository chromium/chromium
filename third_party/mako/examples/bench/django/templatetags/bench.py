from django.template import Library, Node, resolve_variable
from django.utils.html import escape

register = Library()

def greeting(name):
    return 'Hello, %s!' % escape(name)
greeting = register.simple_tag(greeting)

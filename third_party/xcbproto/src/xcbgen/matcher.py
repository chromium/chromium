'''
XML parser.  One function for each top-level element in the schema.

Most functions just declare a new object and add it to the module.
For typedefs, eventcopies, xidtypes, and other aliases though,
we do not create a new type object, we just record the existing one under a new name.
'''

from os.path import join
from sys import version_info

if version_info[:2] >= (3, 3):
    from xml.etree.ElementTree import parse
else:
    from xml.etree.cElementTree import parse

from xcbgen.xtypes import *

def import_(node, module, namespace):
    '''
    For imports, we load the file, create a new namespace object,
    execute recursively, then record the import (for header files, etc.)
    '''
    # To avoid circular import error
    from xcbgen import state
    module.import_level = module.import_level + 1
    new_file = join(namespace.dir, '%s.xml' % node.text)
    new_root = parse(new_file).getroot()
    new_namespace = state.Namespace(new_file)
    execute(module, new_namespace)
    module.import_level = module.import_level - 1
    if not module.has_import(node.text):
        module.add_import(node.text, new_namespace)

def typedef(node, module, namespace):
    id = node.get('newname')
    name = namespace.prefix + (id,)
    type = module.get_type(node.get('oldname'))
    module.add_type(id, namespace.ns, name, type)

def xidtype(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = module.get_type('CARD32')
    module.add_type(id, namespace.ns, name, type)

def xidunion(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = module.get_type('CARD32')
    module.add_type(id, namespace.ns, name, type)

def enum(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = Enum(name, node)
    module.add_type(id, namespace.ns, name, type)

def struct(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = Struct(name, node)
    module.add_type(id, namespace.ns, name, type)

def eventstruct(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = EventStruct(name, node)
    module.add_type(id, namespace.ns, name, type)

def union(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = Union(name, node)
    module.add_type(id, namespace.ns, name, type)

def request(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    type = Request(name, node)
    module.add_request(id, name, type)

def event(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    event = Event(name, node)
    event.add_opcode(node.get('number'), name, True)
    module.add_event(id, name, event)

def eventcopy(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    event = module.get_event(node.get('ref'))
    event.add_opcode(node.get('number'), name, False)
    module.add_event(id, name, event)

def error(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    error = Error(name, node)
    error.add_opcode(node.get('number'), name, True)
    module.add_error(id, name, error)

def errorcopy(node, module, namespace):
    id = node.get('name')
    name = namespace.prefix + (id,)
    error = module.get_error(node.get('ref'))
    error.add_opcode(node.get('number'), name, False)
    module.add_error(id, name, error)

funcs = {'import' : import_,
         'typedef' : typedef,
         'xidtype' : xidtype,
         'xidunion' : xidunion,
         'enum' : enum,
         'struct' : struct,
         'eventstruct' : eventstruct,
         'union' : union,
         'request' : request,
         'event' : event,
         'eventcopy' : eventcopy,
         'error' : error,
         'errorcopy' : errorcopy}

def execute(module, namespace):
    for elt in list(namespace.root):
        funcs[elt.tag](elt, module, namespace)

from mako.lookup import TemplateLookup

template_lookup = TemplateLookup()


def run():
    tpl = template_lookup.get_template("not_found.html")

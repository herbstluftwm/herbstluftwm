import herbstluftwm


def main(hlwm):
    hlwm.call('version')

    # if you pass a string to call, it will be tokenized automatically:
    hlwm.call('add new_tag')
    # but you can also pass a list to avoid whitespace issues:
    hlwm.call(['merge_tag', 'new_tag'])

    # hlwm.attr is a convenience wrapper for accessing attributes
    # 1. reading attributes
    old_name = hlwm.attr.tags.focus.name()
    print(f"The focused tag has the name '{old_name}'")
    # The attribute value is queried on (). Alternatively, string conversion
    # queries implicitly:
    print(f"The focused tag has the name '{hlwm.attr.tags.focus.name}'")

    # 2. writing attributes
    hlwm.attr.tags.focus.name = "TagFocus"
    # One can also use [...] notation, if needed:
    hlwm.attr.tags['by-name'].TagFocus.name = old_name

    # 3. creating attributes (attribute name must start with 'my_')
    hlwm.attr.tags.focus.my_altname = "Attribute Value"

    # the AttributeProxy objects can be used multiple times
    curtag = hlwm.attr.tags.focus
    curtag['my_other_attribute'] = 'value'
    assert curtag.my_other_attribute() == 'value'


if __name__ == '__main__':
    main(herbstluftwm.Herbstluftwm())

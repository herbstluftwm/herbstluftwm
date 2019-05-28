#include "theme.h"

using std::vector;
using std::string;

Theme::Theme() {
    // add sub-decorations array as children
    vector<string> type_names = {
        "fullscreen",
        "tiling",
        "floating",
        "minimal",
    };
    for (int i = 0; i < (int)Type::Count; i++) {
        addStaticChild(&dec[i], type_names[i]);
    }

    // forward attribute changes: only to tiling and floating
    auto &t = dec[(int)Type::Tiling], &f = dec[(int)Type::Floating];
    active.makeProxyFor({&t.active, &f.active});
    normal.makeProxyFor({&t.normal, &f.normal});
    urgent.makeProxyFor({&t.urgent, &f.urgent});
}

DecorationScheme::DecorationScheme()
    : reset(this, "reset", &DecorationScheme::resetGetterHelper,
                           &DecorationScheme::resetSetterHelper)
{
    vector<Attribute*> attrs = {
        &border_width,
        &border_color,
        &tight_decoration,
        &inner_color,
        &inner_width,
        &outer_color,
        &outer_width,
        &padding_top,
        &padding_right,
        &padding_bottom,
        &padding_left,
        &background_color,
    };
    wireAttributes(attrs);
    for (auto i : attrs) {
        i->setWriteable();
        // TODO: signal decoration change (leading to relayout)
    }
}

DecTriple::DecTriple()
{
    addStaticChild(&normal, "normal");
    addStaticChild(&active, "active");
    addStaticChild(&urgent, "urgent");
    makeProxyFor({
        &normal,
        &active,
        &urgent,
    });
}

//! reset all attributes to a default value
string DecorationScheme::resetSetterHelper(string)
{
    for (auto it : attributes()) {
        it.second->resetValue();
    }
    return {};
}

string DecorationScheme::resetGetterHelper() {
    return "Writing this resets all attributes to a default value";
}

void DecorationScheme::makeProxyFor(vector<DecorationScheme*> decs) {
    for (auto it : attributes()) {
        string attrib_name = it.first;
        auto source_attribute = it.second;
        if (source_attribute == &reset) {
            continue;
        }
        // if an attribute of this DecorationScheme is changed, then
        auto handler = [decs, attrib_name, source_attribute] () {
            // for each decoration to forward the value to
            for (auto dec_it : decs) {
                auto target_attribute = dec_it->attribute(attrib_name);
                // consider only those having an attribute of the same name
                if (target_attribute) {
                    // note: clumsy, but we have no explicit 'get()'/'set()'
                    target_attribute->change(source_attribute->str());
                }
            }
        };
        source_attribute->changed().connect(handler);
    }
}



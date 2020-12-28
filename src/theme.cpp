#include "theme.h"

using std::vector;
using std::string;

Theme::Theme()
    : fullscreen(*this, "fullscreen")
    , tiling(*this, "tiling")
    , floating(*this, "floating")
    , minimal(*this, "minimal")
    // in the following array, the order must match the order in Theme::Type!
    , decTriples{ &fullscreen, &tiling, &floating, &minimal }
{
    for (auto dec : decTriples) {
        dec->triple_changed_.connect([this](){ this->theme_changed_.emit(); });
    }

    // forward attribute changes: only to tiling and floating
    active.makeProxyFor({&tiling.active, &floating.active});
    normal.makeProxyFor({&tiling.normal, &floating.normal});
    urgent.makeProxyFor({&tiling.urgent, &floating.urgent});
}

DecorationScheme::DecorationScheme()
    : reset(this, "reset", &DecorationScheme::resetGetterHelper,
                           &DecorationScheme::resetSetterHelper)
    , proxyAttributes_ ({
        &border_width,
        &title_height,
        &title_font,
        &title_color,
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
    })
{
    for (auto i : proxyAttributes_) {
        addAttribute(i->toAttribute());
        i->toAttribute()->setWritable();
        i->toAttribute()->changed().connect([this]() { this->scheme_changed_.emit(); });
    }
}

DecTriple::DecTriple()
   : normal(*this, "normal")
   , active(*this, "active")
   , urgent(*this, "urgent")
{
    vector<DecorationScheme*> children = {
        &normal,
        &active,
        &urgent,
    };
    makeProxyFor(children);
    for (auto it : children) {
        it->scheme_changed_.connect([this]() { this->triple_changed_.emit(); });
    }
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
    for (auto it : proxyAttributes_) {
        for (auto target : decs) {
            it->addProxyTarget(target);
        }
    }
}



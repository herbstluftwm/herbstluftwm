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

    setDoc(
          "    inner_color/inner_width\n"
          "          ╻        outer_color/outer_width\n"
          "          │                  ╻\n"
          "          │                  │\n"
          "    ┌────╴│╶─────────────────┷─────┐ ⎫ border_width\n"
          "    │     │      color             │ ⎬ + title_height\n"
          "    │  ┌──┷─────────────────────┐  │ ⎭ + padding_top\n"
          "    │  │====================....│  │\n"
          "    │  │== window content ==....│  │\n"
          "    │  │====================..╾──────── background_color\n"
          "    │  │........................│  │\n"
          "    │  └────────────────────────┘  │ ⎱ border_width +\n"
          "    └──────────────────────────────┘ ⎰ padding_bottom\n"
          "\n"
          "Setting an attribute of the theme object just propagates the "
          "value to the respective attribute of the +tiling+ and the +floating+ "
          "object."
    );
    tiling.setChildDoc(
                "configures the decoration of tiled clients, setting one of "
                "its attributes propagates the respective attribute of the "
                "+active+, +normal+ and +urgent+ child objects.");
    floating.setChildDoc("behaves analogously to +tiling+");
    minimal.setChildDoc("configures clients with minimal decorations "
                        "triggered by +smart_window_surroundings+");
    fullscreen.setChildDoc("configures clients in fullscreen state");
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
    border_width.setDoc("the base width of the border");
    padding_top.setDoc("additional border width on the top");
    padding_right.setDoc("additional border width on the right");
    padding_bottom.setDoc("additional border width on the bottom");
    padding_left.setDoc("additional border width on the left");
    border_color.setDoc("the basic background color of the border");
    inner_width.setDoc("width of the border around the clients content");
    inner_color.setDoc("color of the inner border");
    outer_width.setDoc("width of an border close to the edge");
    outer_color.setDoc("color of the outer border");
    background_color.setDoc("color behind window contents visible on resize");
    tight_decoration.setDoc("specifies whether the size hints also affect "
                            "the window decoration or only the window "
                            "contents of tiled clients (requires enabled "
                            "sizehints_tiling)");
    reset.setDoc("writing this resets all attributes to a default value");
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
    active.setChildDoc("configures the decoration of the focused client");
    normal.setChildDoc("the default decoration scheme for clients");
    urgent.setChildDoc("configures the decoration of urgent clients");
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



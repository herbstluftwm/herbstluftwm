#include "theme.h"

#include "completion.h"
#include "globals.h"

using std::function;
using std::pair;
using std::make_shared;
using std::vector;
using std::shared_ptr;
using std::string;

template<>
Finite<TitleWhen>::ValueList Finite<TitleWhen>::values = ValueListPlain {
    { TitleWhen::always, "always" },
    { TitleWhen::never, "never" },
    { TitleWhen::one_tab, "one_tab" },
    { TitleWhen::multiple_tabs, "multiple_tabs" },
};


Theme::Theme()
    : style_override(this, "style_override", {})
    , fullscreen(*this, "fullscreen")
    , tiling(*this, "tiling")
    , floating(*this, "floating")
    , minimal(*this, "minimal")
    // in the following array, the order must match the order in Theme::Type!
    , decTriples{ &fullscreen, &tiling, &floating, &minimal }
{
    style_override.setWritable();
    style_override.changed().connect([this]() {
        this->theme_changed_.emit();
    });

    for (auto dec : decTriples) {
        dec->triple_changed_.connect(this, &Theme::onDecTripleChange);
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
          "    │     │      color             │ ⎬ + title_height + title_depth\n"
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
          "object.\n"
          "If the title area is divided into tabs, then the not selected tabs "
          "can be styled using the +tab_...+ attributes. If these attributes are "
          "empty, then the colors are taken from the theme of the client to which "
          "the tab refers to."
    );
    tiling.setChildDoc(
                "configures the decoration of tiled clients, setting one of "
                "its attributes propagates the respective attribute of the "
                "+active+, +normal+ and +urgent+ child objects.");
    floating.setChildDoc("behaves analogously to +tiling+");
    minimal.setChildDoc("configures clients with minimal decorations "
                        "triggered by +smart_window_surroundings+");
    fullscreen.setChildDoc("configures clients in fullscreen state");

    generateBuiltinCss();
}

void Theme::onDecTripleChange()
{
    scheme_changed_.emit();
}

shared_ptr<BoxStyle> Theme::computeBoxStyle(DomTree* element)
{
    if (!element) {
        return nullptr;
    }
    shared_ptr<BoxStyle> style = make_shared<BoxStyle>();
    generatedStyle.computeStyle(element, style);
    style_override->computeStyle(element, style);
    return style;
}

void Theme::generateBuiltinCss()
{
    vector<pair<ThemeType, CssName>> triples = {
        { ThemeType::Tiling, CssName::Builtin::tiling },
        { ThemeType::Floating, CssName::Builtin::floating },
        { ThemeType::Minimal, CssName::Builtin::minimal },
        { ThemeType::Fullscreen, CssName::Builtin::fullscreen },
    };
    vector<shared_ptr<CssRuleSet>> blocks;
    for (const auto& triple2cssname : triples) {
        const DecTriple& decTriple = (*this)[triple2cssname.first];
        CssName tripleCssName = triple2cssname.second;
        vector<pair<const DecorationScheme&, CssName>> tripleEntries = {
            { decTriple.normal, CssName::Builtin::normal },
            { decTriple.active, CssName::Builtin::focus },
            { decTriple.urgent, CssName::Builtin::urgent },
        };
        for (const auto& scheme2cssname : tripleEntries) {
            const DecorationScheme& scheme = scheme2cssname.first;
            CssName schemeCssName = scheme2cssname.second;

            // the decoration
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.backgroundColor = scheme.border_color();
                     style.paddingTop =
                         style.paddingRight =
                         style.paddingBottom =
                         style.paddingLeft =
                            scheme.border_width()
                            - scheme.outer_width()
                            - scheme.inner_width();
                     style.borderWidthTop =
                         style.borderWidthRight =
                         style.borderWidthBottom =
                         style.borderWidthLeft =
                         scheme.outer_width();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorBottom =
                         style.borderColorLeft =
                         scheme.outer_color();
                }},
            }}));

            // the client content
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::client_content,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.backgroundColor = scheme.background_color();
                     style.borderWidthTop =
                         style.borderWidthRight =
                         style.borderWidthBottom =
                         style.borderWidthLeft =
                         scheme.inner_width();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorBottom =
                         style.borderColorLeft =
                         scheme.inner_color();
                }},
            }}));

            // tab bar
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tabbar,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     // negative top margin to move tab
                     // close to upper edge of decorations
                     auto border_width = scheme.border_width() - scheme.inner_width();
                     style.marginTop =
                         style.marginLeft =
                         style.marginRight =
                            - border_width;
                     style.marginBottom = border_width;
                }},
            }}));

            // tab default style
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     style.fontColor =
                        scheme.tab_title_color().rightOr(decTriple.normal.title_color());

                     style.font = decTriple.normal.title_font();
                     style.textAlign = scheme.title_align();
                     style.backgroundColor =
                        scheme.tab_color().rightOr(decTriple.normal.border_color());

                     style.borderWidthTop =
                         scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                     style.paddingTop = -1 * style.borderWidthTop;
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             scheme.tab_outer_color().rightOr(decTriple.normal.outer_color());

                     style.borderWidthTop =
                             scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());

                     style.borderColorBottom = scheme.outer_color();
                     style.borderWidthBottom = scheme.outer_width();
                     style.paddingBottom = -scheme.outer_width();

                     style.paddingLeft = scheme.border_width() - scheme.outer_width();
                     style.paddingRight = scheme.border_width() - scheme.outer_width();
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::pseudo_class, CssName::Builtin::first_child,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     style.borderWidthLeft =
                             scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::pseudo_class, CssName::Builtin::last_child,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     style.borderWidthRight =
                             scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                }},
            }}));

            // the selected tab
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::has_class, CssName::Builtin::focus,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.font = scheme.title_font();
                     style.fontColor = scheme.title_color();
                     style.textAlign = scheme.title_align();
                     style.backgroundColor = Unit<BoxStyle::transparent>();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             scheme.outer_color();
                     style.borderWidthLeft =
                         style.borderWidthRight =
                             scheme.outer_width();

                     style.borderWidthBottom = 0;
                     style.paddingBottom = 0;
                }},
            }}));
            // the selected tab
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::window,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::has_class, CssName::Builtin::urgent,
                },
            },
            {
                {[&decTriple](BoxStyle& style) {
                     style.backgroundColor = decTriple.urgent.border_color();
                     style.fontColor = decTriple.urgent.title_color();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             decTriple.urgent.outer_color();
                }},
            }}));
        }
    }
    generatedStyle.content_.resize(blocks.size());
    size_t idx = 0;
    for (const auto& ptr : blocks) {
        generatedStyle.content_[idx++] = *ptr;
    }
    generatedStyle.recomputeSortedSelectors();
}

DecorationScheme::DecorationScheme()
    : reset(this, "reset", &DecorationScheme::resetGetterHelper,
                           &DecorationScheme::resetSetterHelper)
    , proxyAttributes_ ({
        &border_width,
        &title_height,
        &title_depth,
        &title_when,
        &title_font,
        &title_align,
        &title_color,
        &border_color,
        &tight_decoration,
        &inner_color,
        &inner_width,
        &outer_color,
        &outer_width,
        &tab_color,
        &tab_outer_color,
        &tab_outer_width,
        &tab_title_color,
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
    title_depth.setDoc("the space below the baseline of the window title");
    title_when.setDoc("when to show the window title: always, never, "
                      "if the the client is in a tabbed scenario like a max frame (+one_tab+), "
                      "if there are +multiple_tabs+ to be shown.");
    title_align.setDoc("the horizontal alignment of the title within the tab "
                       "or title bar. The value is one of: left, center, right");
    tab_color.setDoc("if non-empty, the color of non-urgent and unfocused tabs");
    tab_outer_color.setDoc(
                "if non-empty, the outer border color of non-urgent and "
                "unfocused tabs; if empty, the colors are taken from the tab's"
                "client decoration settings.");
    tab_outer_width.setDoc("if non-empty, the outer border width of non-urgent and unfocused tabs");
    tab_title_color.setDoc("if non-empty, the title color of non-urgent and unfocused tabs");
    reset.setDoc("writing this resets all attributes to a default value");
}

Rectangle DecorationScheme::outline_to_inner_rect(Rectangle rect, size_t tabCount) const {
    return rect.adjusted(-*border_width, -*border_width)
            .adjusted(-*padding_left,
                      -*padding_top - (showTitle(tabCount) ? (*title_height + *title_depth) : 0),
                      -*padding_right, -*padding_bottom);
}

/**
 * @brief whether to show the window titles
 * @param the number of tabs
 * @return
 */
bool DecorationScheme::showTitle(size_t tabCount) const
{
    if (title_height() == 0) {
        return false;
    }
    switch (title_when()) {
        case TitleWhen::always: return true;
        case TitleWhen::never: return false;
        case TitleWhen::one_tab: return tabCount >= 1;
        case TitleWhen::multiple_tabs: return tabCount >= 2;
    }
    return true; // Dead code. But otherwise, gcc complains
}

Rectangle DecorationScheme::inner_rect_to_outline(Rectangle rect, size_t tabCount) const {
    return rect.adjusted(*border_width, *border_width)
            .adjusted(*padding_left,
                      *padding_top + (showTitle(tabCount) ? (*title_height + *title_depth) : 0),
                      *padding_right, *padding_bottom);
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


template<>
void Converter<Inherit>::complete(Completion& complete, const Inherit* relativeTo)
{
    complete.full("");
}

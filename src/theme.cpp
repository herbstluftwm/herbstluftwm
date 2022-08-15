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
    : name(this, "name", {})
    , style_override(this, "style_override", {})
    , fullscreen(*this, "fullscreen")
    , tiling(*this, "tiling")
    , floating(*this, "floating")
    , minimal(*this, "minimal")
    // in the following array, the order must match the order in Theme::Type!
    , decTriples{ &fullscreen, &tiling, &floating, &minimal }
{
    name.setWritable();
    name.changed().connect([this]() {
        this->theme_changed_.emit();
    });

    style_override.setWritable();
    style_override.changed().connect([this]() {
        this->theme_changed_.emit();
    });

    for (auto dec : decTriples) {
        dec->triple_changed_.connect([this]() {
            this->theme_changed_.emit();
        });
    }

    // forward attribute changes: only to tiling and floating
    active.makeProxyFor({&tiling.active, &floating.active});
    normal.makeProxyFor({&tiling.normal, &floating.normal});
    urgent.makeProxyFor({&tiling.urgent, &floating.urgent});

    // deactivate window titles for minimal and fullscreen decorations per default:
    vector<DecorationScheme*> schemesWithoutTitle = {
        &minimal,
        &minimal.active,
        &minimal.normal,
        &minimal.urgent,
        &fullscreen,
        &fullscreen.active,
        &fullscreen.normal,
        &fullscreen.urgent,
    };
    for (DecorationScheme* s : schemesWithoutTitle) {
        s->title_when.setInitialAndDefaultValue(TitleWhen::never);
    }


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

    name.setDoc("the absolute path to a css theme file. if this is empty, "
                "then the theme is specified by the attributes.");
    style_override.setDoc(
                "additional css source to overwrite parts of the theme. "
                "All rules here have higher precedence than all rules "
                "in the theme");
    generateBuiltinCss();
}

shared_ptr<BoxStyle> Theme::computeBoxStyle(DomTree* element)
{
    if (!element) {
        return nullptr;
    }
    shared_ptr<BoxStyle> style = make_shared<BoxStyle>();
    if (element->parent()) {
        style->inheritFromParent(element->parent()->cachedStyle());
    }
    if (name()) {
        name()->content_.computeStyle(element, style);
    } else {
        generatedStyle.computeStyle(element, style);
    }
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
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.backgroundColor = scheme.border_color();
                     auto bw = scheme.border_width() - scheme.outer_width() - scheme.inner_width();
                     style.paddingTop = bw + scheme.padding_top();
                     style.paddingRight = bw + scheme.padding_right();
                     style.paddingBottom = bw + scheme.padding_bottom();
                     style.paddingLeft = bw + scheme.padding_left();

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
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
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
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
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
                     auto bw = scheme.border_width() - scheme.inner_width();
                     style.marginTop = - bw - scheme.padding_top();
                     style.marginLeft = - bw - scheme.padding_left();
                     style.marginRight = - bw - scheme.padding_right();
                     style.marginBottom = bw;
                     style.borderWidthBottom = scheme.outer_width();
                     style.borderColorBottom = scheme.outer_color();
                }},
            }}));

            // tab default style
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     style.backgroundColor =
                        scheme.tab_color().rightOr(decTriple.normal.border_color());

                     style.borderWidthTop =
                         scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                     style.paddingTop =
                        // allow overlap with border color:
                        -1 * style.borderWidthTop
                        // but apply custom the padding
                        + scheme.padding_top();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             scheme.tab_outer_color().rightOr(decTriple.normal.outer_color());

                     style.borderWidthTop =
                             scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());

                     style.paddingLeft = scheme.padding_left() + scheme.border_width();
                     style.paddingRight = scheme.padding_right() + scheme.border_width();
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::title,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     style.fontColor =
                        scheme.tab_title_color().rightOr(decTriple.normal.title_color());

                     style.font = decTriple.normal.title_font();
                     style.textAlign = scheme.title_align();
                     style.textHeight = scheme.title_height().cases<__typeof__(style.textHeight)>(
                         [](const Inherit&) {
                             return Unit<BoxStyle::auto_>();
                         },
                         [](const int& len) {
                             return CssLen(len);
                         });

                     style.textDepth = scheme.title_depth().cases<__typeof__(style.textDepth)>(
                         [](const Inherit&) {
                             return Unit<BoxStyle::auto_>();
                         },
                         [](const unsigned long& len) {
                             return CssLen(static_cast<int>(len));
                         });
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::pseudo_class, CssName::Builtin::first_child,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     auto bw = scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                     style.borderWidthLeft = bw;
                     style.paddingLeft = scheme.padding_left() + scheme.border_width() - bw;
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::pseudo_class, CssName::Builtin::last_child,
                },
            },
            {
                {[&scheme,&decTriple](BoxStyle& style) {
                     auto bw = scheme.tab_outer_width().rightOr(decTriple.normal.outer_width());
                     style.borderWidthRight = bw;
                     style.paddingRight = scheme.padding_right() + scheme.border_width() - bw;
                }},
            }}));

            // the selected tab
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::has_class, CssName::Builtin::focus,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.backgroundColor = scheme.border_color();
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             scheme.outer_color();
                     style.borderWidthTop =
                         style.borderWidthLeft =
                         style.borderWidthRight =
                         scheme.outer_width();
                     style.paddingTop =
                        // allow overlap with border color:
                        -1 * style.borderWidthTop
                        // but apply custom the padding
                        + scheme.padding_top();

                     style.borderWidthBottom = 0;
                     style.paddingLeft = scheme.padding_left() + scheme.border_width() - scheme.outer_width();
                     style.paddingRight = scheme.padding_right() + scheme.border_width() - scheme.outer_width();
                     style.paddingBottom = scheme.outer_width();
                     style.marginBottom = -scheme.outer_width();
                }},
            }}));
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::has_class, CssName::Builtin::focus,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::title,
                },
            },
            {
                {[&scheme](BoxStyle& style) {
                     style.font = scheme.title_font();
                     style.fontColor = scheme.title_color();
                     style.textAlign = scheme.title_align();
                }},
            }}));
            // the urgent tab
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
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
                     style.borderColorTop =
                         style.borderColorRight =
                         style.borderColorLeft =
                             decTriple.urgent.outer_color();
                }},
            }}));
            // the urgent tab title
            blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
            {
                { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                  CssName::Builtin::has_class, tripleCssName,
                  CssName::Builtin::has_class, schemeCssName,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::tab,
                  CssName::Builtin::has_class, CssName::Builtin::urgent,
                  CssName::Builtin::descendant,
                  CssName::Builtin::has_class, CssName::Builtin::title,
                },
            },
            {
                {[&decTriple](BoxStyle& style) {
                     style.fontColor = decTriple.urgent.title_color();
                }},
            }}));
            // Implement 'title_when' for
            vector<pair<CssName::Builtin, TitleWhen>> css2titlewhen = {
                // for each css class, what is the minimum value of 'title_when'
                // such that the tab bar is shown:
                { CssName::Builtin::no_tabs, TitleWhen::always },
                { CssName::Builtin::one_tab, TitleWhen::one_tab },
                { CssName::Builtin::multiple_tabs, TitleWhen::multiple_tabs },
            };
            for (const auto& it : css2titlewhen) {
                CssName::Builtin cssClass = it.first;
                TitleWhen minimumValue = it.second;
                blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
                {
                    { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
                      CssName::Builtin::has_class, tripleCssName,
                      CssName::Builtin::has_class, schemeCssName,
                      CssName::Builtin::has_class, cssClass,
                      CssName::Builtin::descendant,
                      CssName::Builtin::has_class, CssName::Builtin::bar,
                    },
                },
                {
                    {[&scheme,minimumValue,tripleCssName,schemeCssName](BoxStyle& style) {
                         if (scheme.title_when() >= minimumValue) {
                             style.display = CssDisplay::flex;
                         } else {
                             style.display = CssDisplay::none;
                         }
                    }},
                }}));
            }
        }
    }
    // Hide tab bar for 'fullscreen' and 'minimal' per default
    blocks.push_back(make_shared<CssRuleSet>(CssRuleSet {
    {
        { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
          CssName::Builtin::has_class, CssName::Builtin::minimal,
          CssName::Builtin::descendant,
          CssName::Builtin::has_class, CssName::Builtin::bar,
        },
        { CssName::Builtin::has_class, CssName::Builtin::client_decoration,
          CssName::Builtin::has_class, CssName::Builtin::fullscreen,
          CssName::Builtin::descendant,
          CssName::Builtin::has_class, CssName::Builtin::bar,
        },
    },
    {
        {[](BoxStyle& style) {
             style.display = CssDisplay::none;
        }},
    }}));

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
    title_height.setDoc("the space above the baseline of the window title, "
                       "or \'\' for an automatic calculation based on the font.");
    title_depth.setDoc("the space below the baseline of the window title, "
                       "or \'\' for an automatic calculation based on the font.");
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

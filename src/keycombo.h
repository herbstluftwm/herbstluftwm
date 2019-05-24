#pragma once

#include <X11/X.h>
#include <regex>
#include <string>
#include <vector>

#include "types.h"

/*!
 * Represents the press of a combination of modifiers keys.
 *
 * Handles the parsing and creation of string representations of itself.
 */

class ModifierCombo {
public:
    unsigned int modifiers_;

    static constexpr auto separators = "+-";

    class ModifierNameAndMask {
    public:
        std::string name;
        unsigned int mask;
    };

    /*!
     * List of existing modifiers and their corresponding key masks.
     *
     * This is not an std::map because even though it is a surjective relation,
     * we need well-defined reverse lookups for stringification.
     */
    static const std::vector<ModifierNameAndMask> modifierMasks;
    static unsigned int modifierMaskFromTokens(const std::vector<std::string>& tokens);
    static std::vector<std::string> tokensFromString(std::string keySpec);
    static std::vector<std::string> getNamesForModifierMask(unsigned int mask);
    static unsigned int getMaskForModifierName(std::string name);
};

//! given the modifier-prefix, generate possible completions
typedef std::function<void(Completion&, std::string)> SuffixCompleter;

//! A intermediate format for KeyCombo parsing and printing
class ModifiersWithString : public ModifierCombo {
public:
    ModifiersWithString();
    ModifiersWithString(unsigned int modifiers, std::string suffix);
    std::string suffix_;
    static void complete(Completion& complete, SuffixCompleter suffixCompleter);
};

ConverterInstance(ModifiersWithString)

/*!
 * Represents a keypress combination of a keysym and modifiers (optional).
 *
 * Handles the parsing and creation of string representations of itself.
 */
class KeyCombo : public ModifierCombo {
public:
    KeyCombo() = default;

    std::string str() const;
    bool matches(const std::regex& regex) const;
    bool operator==(const KeyCombo& other) const;
    static KeySym keySymFromString(const std::string& str);
    static KeyCombo fromString(const std::string& str);
    static std::vector<std::string> getPossibleKeySyms();
    static void complete(Completion& complete);

    KeySym keysym;
};

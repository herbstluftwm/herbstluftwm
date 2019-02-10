#include "keycombo.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "globals.h"

using std::string;
using std::vector;

const vector<KeyCombo::ModifierNameAndMask> KeyCombo::modifierMasks = {
    { "Mod1",       Mod1Mask },
    { "Mod2",       Mod2Mask },
    { "Mod3",       Mod3Mask },
    { "Mod4",       Mod4Mask },
    { "Mod5",       Mod5Mask },
    { "Alt",        Mod1Mask },
    { "Super",      Mod4Mask },
    { "Shift",      ShiftMask },
    { "Control",    ControlMask },
    { "Ctrl",       ControlMask },
};

/*!
 * Provides a canonical string representation of the key combo
 */
std::string KeyCombo::str() const {
    std::stringstream str;

    /* add modifiers */
    for (auto& modName : getNamesForModifierMask(modifiers)) {
        str << modName << separators[0];
    }

    /* add keysym */
    const char* name = XKeysymToString(keysym);
    if (!name) {
        HSWarning("XKeysymToString failed! using \'?\' instead\n");
        name = "?";
    }
    str << name;

    return str.str();
}

/*!
 * Returns true if the string representation of this KeyCombo matches the given
 * regex
 */
bool KeyCombo::matches(const std::regex& regex) const {
    return std::regex_match(str(), regex);
}

/*!
 * Determines a modifier mask value from a list of key combo tokens
 *
 * \throws meaningful exceptions on parsing errors
 */
unsigned int KeyCombo::modifierMaskFromTokens(const vector<string>& tokens) {
    unsigned int modifiers = 0;
    for (auto& modName : tokens) {
        modifiers |= getMaskForModifierName(modName);
    }
    return modifiers;
}

/*!
 * Parses a given key sym string into an X11 KeySym.
 *
 * \throws meaningful exceptions on parsing errors
 */
KeySym KeyCombo::keySymFromString(const string& str) {
    auto keysym = XStringToKeysym(str.c_str());
    if (keysym == NoSymbol) {
        throw std::runtime_error("Unknown KeySym \"" + str + "\"");
    }
    return keysym;
}

/*!
 * Creates a KeyCombo from a given string representation
 *
 * Example inputs: "Mod1-space", "Mod4+f", "f"
 *
 * In order to avoid throwing exceptions from constructors (they often get
 * called implicitly), this is implemented as a static method.
 *
 * \throws meaningful exceptions on parsing errors
 */
KeyCombo KeyCombo::fromString(const string& str) {
    KeyCombo combo;
    auto tokens = tokensFromString(str);

    if (tokens.empty()) {
        throw std::runtime_error("Empty keysym");
    }

    auto modifierSlice = vector<string>({tokens.begin(), tokens.end() - 1});
    auto keySymString = tokens.back();

    combo.modifiers = modifierMaskFromTokens(modifierSlice);
    combo.keysym = keySymFromString(keySymString);
    return combo;
}

bool KeyCombo::operator==(const KeyCombo& other) const {
    bool sameMods = modifiers == other.modifiers;
    bool sameKeySym = keysym == other.keysym;
    return sameMods && sameKeySym;
}

//! Splits a given key combo string into a list of tokens
vector<string> KeyCombo::tokensFromString(string keySpec)
{
    // Normalize spec to use the default separator:
    char baseSep = separators[0];
    for (auto &sep : string(separators)) {
        std::replace(keySpec.begin(), keySpec.end(), sep, baseSep);
    }

    // Split spec into tokens:
    vector<string> tokens;
    string token;
    std::istringstream tokenStream(keySpec);
    while (std::getline(tokenStream, token, baseSep)) {
        tokens.push_back(token);
    }
    return tokens;
}

/*!
 * Provides the mask value for a given modifier name (internal helper)
 *
 * \throws std::runtime_error if modifier name is unknown
 */
unsigned int KeyCombo::getMaskForModifierName(string name) {
    // Simple, linear search for matching list entry:
    for (auto& entry : modifierMasks) {
        if (entry.name == name) {
            return entry.mask;
        }
    }

    throw std::runtime_error("Unknown modifier \"" + name + "\"");
}

/*!
 * Provides the names of modifiers that are set in a given mask.
 */
vector<string> KeyCombo::getNamesForModifierMask(unsigned int mask) {
    vector<string> names;
    for (auto& entry : modifierMasks) {
        if (entry.mask & mask) {
            names.push_back(entry.name);

            // remove match from mask
            mask = mask & ~ entry.mask;
        }
    }

    return names;
}

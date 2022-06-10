#include "keycombo.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <sstream> // IWYU pragma: keep
#include <stdexcept>

#include "completion.h"
#include "globals.h"
#include "utils.h"
#include "xkeygrabber.h"

using std::make_pair;
using std::string;
using std::stringstream;
using std::vector;

const vector<KeyCombo::ModifierNameAndMask> ModifierCombo::modifierMasks = {
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

const unsigned int ModifierCombo::allModifierMasks =
        Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask
        | Mod5Mask | ShiftMask | ControlMask;

ModifiersWithString::ModifiersWithString()
    : suffix_("")
{
    this->modifiers_ = 0;
}

ModifiersWithString::ModifiersWithString(unsigned int modifiers, string suffix)
    : suffix_(suffix)
{
    this->modifiers_ = modifiers;
}

void ModifiersWithString::complete(Completion& complete, SuffixCompleter suffixCompleter)
{
    string needle = complete.needle();
    ModifiersWithString mws;
    try {
        if (!needle.empty()) {
            mws = Converter<ModifiersWithString>::parse(needle);
        }
    } catch (...) {
        return;
    }
    string prefix = needle.substr(0, needle.size() - mws.suffix_.size());
    char sep =
        !prefix.empty()
        ? prefix[prefix.size() - 1]
        : ModifierCombo::defaultSeparator();
    for (auto& modifier : KeyCombo::modifierMasks) {
        if (modifier.mask & mws.modifiers_) {
            // the modifier is already present in the combination
            continue;
        }
        complete.partial(prefix + modifier.name + sep);
    }
    suffixCompleter(complete, prefix);
}

template<> ModifiersWithString Converter<ModifiersWithString>::parse(const string& source)
{
    auto tokens = ModifierCombo::tokensFromString(source);

    if (tokens.empty()) {
        throw std::invalid_argument("Must not be empty");
    }

    auto modifierSlice = vector<string>({tokens.begin(), tokens.end() - 1});
    auto modmask = ModifierCombo::modifierMaskFromTokens(modifierSlice);
    return { modmask, tokens.back() };
}

template<> string Converter<ModifiersWithString>::str(ModifiersWithString payload)
{
    stringstream str;
    for (auto& modName : ModifierCombo::getNamesForModifierMask(payload.modifiers_)) {
        str << modName << ModifierCombo::defaultSeparator();
    }
    str << payload.suffix_;
    return str.str();
}

template<> void Converter<ModifiersWithString>::complete(Completion& complete, ModifiersWithString const*)
{
    ModifiersWithString::complete(complete, [] (Completion&, string) {});
}

/*!
 * Provides a canonical string representation of the key combo
 */
string KeyCombo::str() const {
    /* convert keysym */
    const char* name = XKeysymToString(keysym);
    if (!name) {
        HSWarning("XKeysymToString failed! using \'?\' instead\n");
        name = "?";
    }
    string prefix;
    if (onRelease_) {
        prefix = releaseModifier;
        prefix += ModifierCombo::defaultSeparator();
    }
    return prefix + Converter<ModifiersWithString>::str({ modifiers_, name });
}

/*!
 * Determines a modifier mask value from a list of key combo tokens
 *
 * \throws meaningful exceptions on parsing errors
 */
unsigned int ModifierCombo::modifierMaskFromTokens(const vector<string>& tokens) {
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

template<> KeyCombo Converter<KeyCombo>::parse(const string& source) {
    auto str = source; // copy to make it modifiable
    KeyCombo combo = {};
    for (auto &sep : string(KeyCombo::separators)) {
        string prefix = KeyCombo::releaseModifier;
        prefix += sep;
        if (stringStartsWith(str, prefix)) {
            // strip "Release-" from the begin
            str = str.substr(prefix.size());
            combo.onRelease_ = true;
            break;
        }
    }
    auto mws = Converter<ModifiersWithString>::parse(str);
    combo.modifiers_ = mws.modifiers_;
    combo.keysym = KeyCombo::keySymFromString(mws.suffix_);
    return combo;
}

bool KeyCombo::operator==(const KeyCombo& other) const {
    return modifiers_ == other.modifiers_
            && keysym == other.keysym
            && onRelease_ == other.onRelease_;
}

bool KeyCombo::operator<(const KeyCombo& other) const
{
    return std::make_tuple(modifiers_, keysym, onRelease_)
            < std::make_tuple(other.modifiers_, other.keysym, onRelease_);
}

//! Splits a given key combo string into a list of tokens
vector<string> ModifierCombo::tokensFromString(string keySpec)
{
    // Normalize spec to use the default separator:
    char baseSep = defaultSeparator();
    for (auto &sep : string(separators)) {
        std::replace(keySpec.begin(), keySpec.end(), sep, baseSep);
    }
    return ArgList::split(keySpec, baseSep);
}

/*!
 * Provides the mask value for a given modifier name (internal helper)
 *
 * \throws std::runtime_error if modifier name is unknown
 */
unsigned int ModifierCombo::getMaskForModifierName(string name) {
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
vector<string> ModifierCombo::getNamesForModifierMask(unsigned int mask) {
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

template<> void Converter<KeyCombo>::complete(Completion& complete, KeyCombo const*) {
    const string releasePrefix = string(KeyCombo::releaseModifier) + ModifierCombo::defaultSeparator();
    complete.partial(releasePrefix);
    ModifiersWithString::complete(complete, [&releasePrefix] (Completion& compWrapped, string prefix) {
        // Offer full completions for a final keysym:
        auto keySyms = XKeyGrabber::getPossibleKeySyms();
        for (const auto& keySym : keySyms) {
            compWrapped.full(prefix + keySym);
            compWrapped.full(releasePrefix + prefix + keySym);
        }
    });
}

template<> string Converter<KeyCombo>::str(KeyCombo payload) {
    return payload.str();
}

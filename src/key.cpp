#include "key.h"

#include "command.h"
#include "glib-backports.h"
#include "globals.h"
#include "keycombo.h"

void complete_against_keysyms(const char* needle, char* prefix, Output output) {
    auto keySyms = XKeyGrabber::getPossibleKeySyms();
    for (auto& keySym : keySyms) {
        try_complete_prefix(needle, keySym.c_str(), prefix, output);
    }
}

void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output) {
    GString* buf = g_string_sized_new(20);
    for (auto& strToMask : KeyCombo::modifierMasks) {
        g_string_printf(buf, "%s%c", strToMask.name.c_str(), seperator);
        try_complete_prefix_partial(needle, buf->str, prefix, output);
    }
    g_string_free(buf, true);
}

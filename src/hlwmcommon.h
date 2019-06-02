#pragma once

#include <X11/X.h>
#include <unordered_map>

class Root;
class Client;

/** A collection of commonly used functions. The main purpose of
 * this interface is to reduce dependencies between modules.
 */
class HlwmCommon {
public:
    HlwmCommon(Root* root);
    //! The Client object for a window or nullptr if unmanaged.
    Client* client(Window window);
    const std::unordered_map<Window, Client*>& clients();
private:
    Root* root_;
};

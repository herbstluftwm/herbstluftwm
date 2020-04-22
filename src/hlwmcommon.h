#pragma once

#include <X11/X.h>
#include <string>
#include <unordered_map>
#include <vector>

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
    static std::pair<int,std::string> callCommand(const std::vector<std::string>& call);
private:
    Root* root_;
};

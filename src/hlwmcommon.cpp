#include "hlwmcommon.h"

#include "clientmanager.h"
#include "root.h"

using std::make_pair;
using std::string;
using std::vector;
using std::pair;

HlwmCommon::HlwmCommon(Root* root)
    : root_(root)
{
}

Client* HlwmCommon::client(Window window) {
    return root_->clients->client(window);
}

const std::unordered_map<Window, Client*>& HlwmCommon::clients() const {
    return root_->clients->clients();
}


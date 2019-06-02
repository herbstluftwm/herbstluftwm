#include "hlwmcommon.h"

#include "clientmanager.h"
#include "root.h"

HlwmCommon::HlwmCommon(Root* root)
    : root_(root)
{
}

Client* HlwmCommon::client(Window window) {
    return root_->clients->client(window);
}

const std::unordered_map<Window, Client*>& HlwmCommon::clients() {
    return root_->clients->clients();
}

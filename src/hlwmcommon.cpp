#include "hlwmcommon.h"

#include <sstream>

#include "clientmanager.h"
#include "command.h"
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

const std::unordered_map<Window, Client*>& HlwmCommon::clients() {
    return root_->clients->clients();
}

//! wrapper around Commands::call()
pair<int,string> HlwmCommon::callCommand(const vector<string>& call) {
    // the call consists of the command and its arguments
    std::ostringstream output;
    auto input =
        (call.empty())
        ? Input("", call)
        : Input(call[0], vector<string>(call.begin() + 1, call.end()));
    int status = Commands::call(input, output);
    return make_pair(status, output.str());
}


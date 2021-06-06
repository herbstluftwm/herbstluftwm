#pragma once

#include "attribute_.h"
#include "object.h"

class Completion;

class Autostart : public Object
{
public:
    Autostart(const std::string& autostartFromCmdLine, const std::string& globalAutostart);
    void reloadCmd();
    int reloadCmdDummyOutput(Output) { reloadCmd(); return 0; };
    Attribute_<std::string> autostart_path_;
    Attribute_<std::string> global_autostart_path_;
    Attribute_<unsigned long> pid_;
    Attribute_<bool> running_;
    Attribute_<int> last_status_;

    //! the main should call this if a child exits:
    void childExited(std::pair<pid_t, int> childInfo);
};

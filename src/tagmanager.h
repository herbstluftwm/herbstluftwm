#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "tag.h"
#include "childbyindex.h"
#include "byname.h"

class Settings;
class MonitorManager;

class TagManager : public ChildByIndex<HSTag> {
public:
    TagManager(Settings* settings);
    void setMonitorManager(MonitorManager* monitors);
    int tag_add_command(Input input, Output output);
    int tag_rename_command(Input input, Output output);
    int tag_move_window_command(Input input, Output output);
    int tag_move_window_by_index_command(Input input, Output output);
    HSTag* add_tag(const std::string& name);
    HSTag* find(const std::string& name);
    HSTag* ensure_tags_are_available();
    HSTag* byIndexStr(const std::string& index_str, bool skip_visible_tags);
    void moveClient(HSClient* client, HSTag* target);
    void moveFocusedClient(HSTag* target);
private:
    ByName by_name;
    MonitorManager* monitors;
    Settings* settings;
};

extern TagManager* tags; // temporarily

#endif

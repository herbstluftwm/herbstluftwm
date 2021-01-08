#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "byname.h"
#include "commandio.h"
#include "indexingobject.h"
#include "link.h"
#include "signal.h"
#include "tag.h"

class Client;
class CommandBinding;
class FrameTree;
class Monitor;
class MonitorManager;
class Settings;

typedef std::function<int(FrameTree&,Input,Output)> FrameCommand;
typedef void (FrameTree::*FrameCompleter)(Completion&);
class TagManager : public IndexingObject<HSTag> {
public:
    TagManager();
    void injectDependencies(MonitorManager* m, Settings *s);

    int removeTag(Input input, Output output);
    int tag_add_command(Input input, Output output);
    int tag_rename_command(Input input, Output output);
    int tag_move_window_command(Input argv, Output output);
    int tag_move_window_by_index_command(Input argv, Output output);
    int floatingCmd(Input input, Output output);
    void floatingComplete(Completion& complete);
    HSTag* add_tag(const std::string& name);
    HSTag* find(const std::string& name);
    void completeTag(Completion& complete);
    HSTag* ensure_tags_are_available();
    HSTag* byIndexStr(const std::string& index_str, bool skip_visible_tags);
    HSTag* unusedTag();
    void moveClient(Client* client, HSTag* target, std::string frameIndex = {}, bool focus = true);
    void moveFocusedClient(HSTag* target);
    std::function<int(Input, Output)> frameCommand(FrameCommand cmd);
    CommandBinding frameCommand(FrameCommand cmd, FrameCompleter completer);
    std::function<int()> frameCommand(std::function<int(FrameTree&)> cmd);
    void updateFocusObject(Monitor* focusedMonitor);
    std::string isValidTagName(std::string name);
    Signal_<HSTag*> needsRelayout_;
private:
    std::function<void(Completion&)> frameCompletion(FrameCompleter completer);
    void onTagRename(HSTag* tag);
    ByName by_name_;
    MonitorManager* monitors_ = {}; // circular dependency
    Settings* settings_;
    Link_<HSTag> focus_;
};

extern TagManager* global_tags; // temporary, set in Root constr.

#endif

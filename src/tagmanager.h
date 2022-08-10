#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "byname.h"
#include "commandio.h"
#include "indexingobject.h"
#include "link.h"
#include "runtimeconverter.h"
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
typedef void (FrameTree::*FrameCallOrComplete)(CallOrComplete);

template<>
RunTimeConverter<HSTag*>* Converter<HSTag*>::converter;

class TagManager : public IndexingObject<HSTag>, public Manager<HSTag> {
public:
    TagManager();
    void injectDependencies(MonitorManager* m, Settings *s);

    // RunTimeConverter<HSTag*>:
    virtual HSTag* parse(const std::string& str) override;
    virtual std::string str(HSTag* tag) override;
    virtual void completeEntries(Completion& completion) override;

    void mergeTagCommand(CallOrComplete invoc);
    bool mergeTag(HSTag* tagToRemove, HSTag* targetTag);
    void addCommand(CallOrComplete invoc);
    void tag_rename_command(CallOrComplete invoc);
    void tag_move_window_command(CallOrComplete invoc);
    void tag_move_window_by_index_command(CallOrComplete invoc);
    int floatingCmd(Input input, Output output);
    void floatingComplete(Completion& complete);
    HSTag* add_tag(const std::string& name);
    HSTag* find(const std::string& name);
    HSTag* ensure_tags_are_available();
    HSTag* byIndexStr(const std::string& index_str, bool skip_visible_tags);
    HSTag* unusedTag();
    void moveClient(Client* client, HSTag* target, std::string frameIndex = {}, bool focus = true);
    void moveFocusedClient(HSTag* target);
    std::function<int(Input, Output)> frameCommand(FrameCommand cmd);
    CommandBinding frameCommand(FrameCommand cmd, FrameCompleter completer);
    CommandBinding frameCommand(FrameCallOrComplete cmd);
    std::function<int()> frameCommand(std::function<int(FrameTree&)> cmd);
    void updateFocusObject(Monitor* focusedMonitor);
    std::string isValidTagName(std::string name);
    Signal_<HSTag*> needsRelayout_;
    Link_<HSTag> focus_;
    size_t indexOfAtEndSection();
private:
    std::function<void(Completion&)> frameCompletion(FrameCompleter completer);
    void onTagRename(HSTag* tag);
    void onTagAtEndChange(HSTag* tag);
    ByName by_name_;
    MonitorManager* monitors_ = {}; // circular dependency
    Settings* settings_;
};

extern TagManager* global_tags; // temporary, set in Root constr.

#endif

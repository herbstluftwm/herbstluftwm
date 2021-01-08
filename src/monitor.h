#ifndef __HERBSTLUFT_MONITOR_H_
#define __HERBSTLUFT_MONITOR_H_

#include <X11/X.h>

#include "attribute_.h"
#include "object.h"
#include "rules.h"
#include "rectangle.h"

class HSTag;
class MonitorManager;
class Settings;

class Monitor : public Object {
public:
    Monitor(Settings* settings, MonitorManager* monman, Rectangle Rect, HSTag* tag);
    ~Monitor() override;
    Rectangle getFloatingArea() const;
    int relativeX(int x_root);
    int relativeY(int y_root);

    HSTag*      tag;    // currently viewed tag
    HSTag*      tag_previous;    // previously viewed tag
    Attribute_<std::string>   name;
    Attribute_<unsigned long> index;
    DynAttribute_<std::string>   tag_string;
    Attribute_<int>         pad_up;
    Attribute_<int>         pad_right;
    Attribute_<int>         pad_down;
    Attribute_<int>         pad_left;
    Attribute_<bool>        lock_tag;
    // whether the above pads were determined automatically
    // from autodetected panels
    std::vector<bool>       pad_automatically_set;
    bool        dirty;
    bool        lock_frames;
    struct {
        // last saved mouse position
        int x;
        int y;
    } mouse;
    Rectangle   rect;   // area for this monitor
    Window      stacking_window;   // window used for making stacking easy
    Signal monitorMoved;
    void setIndexAttribute(unsigned long index) override;
    int lock_tag_cmd(Input argv, Output output);
    int unlock_tag_cmd(Input argv, Output output);
    void noComplete(Completion& complete);
    int list_padding(Input input, Output output);
    int move_cmd(Input input, Output output);
    void move_complete(Completion& complete);
    int renameCommand(Input input, Output output);
    void renameComplete(Completion& complete);
    bool setTag(HSTag* new_tag);
    void applyLayout();
    void restack();
    std::string getDescription();
    void evaluateClientPlacement(Client* client, ClientPlacement placement) const;
private:
    std::string getTagString();
    std::string setTagString(std::string new_tag);
    Settings* settings;
    MonitorManager* monman;
};

// adds a new monitor to the monitors list and returns a pointer to it
Monitor* find_monitor_with_tag(HSTag* tag);
void monitor_focus_by_index(unsigned new_selection);
int monitor_cycle_command(int argc, char** argv);
int monitor_focus_command(int argc, char** argv, Output output);
Monitor* find_monitor_by_name(const char* name);
Monitor* string_to_monitor(const char* string);
int remove_monitor_command(int argc, char** argv, Output output);
int remove_monitor(int index);
int monitor_rect_command(int argc, char** argv, Output output);
Monitor* get_current_monitor();
int monitor_set_tag(Monitor* monitor, HSTag* tag);
int monitor_set_pad_command(int argc, char** argv, Output output);
int monitor_set_tag_command(int argc, char** argv, Output output);
int monitor_set_tag_by_index_command(int argc, char** argv, Output output);
int monitor_set_previous_tag_command(Output output);
void all_monitors_apply_layout();
void ensure_monitors_are_available();
void all_monitors_replace_previous_tag(HSTag* old, HSTag* newmon);

void monitor_update_focus_objects();

int shift_to_monitor(int argc, char** argv, Output output);

#endif


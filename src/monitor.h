#ifndef __HERBSTLUFT_MONITOR_H_
#define __HERBSTLUFT_MONITOR_H_

#include <X11/X.h>

#include "attribute_.h"
#include "object.h"
#include "rectangle.h"
#include "rules.h"

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

    HSTag*      tag;    // currently viewed tag, this is always non-null
    HSTag*      tag_previous;    // previously viewed tag, this is always non-null
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
    Attribute_<Rectangle>   rect;   // area for this monitor
    DynAttribute_<Rectangle> contentGeometry;
    Window      stacking_window;   // window used for making stacking easy
    Signal monitorMoved;
    Rectangle clampRelativeGeometry(Rectangle relativeGeo) const;
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
    static std::string atLeastMinWindowSize(Rectangle geom);
private:
    std::string getTagString();
    std::string setTagString(std::string new_tag);
    Settings* settings;
    MonitorManager* monman;
};

// adds a new monitor to the monitors list and returns a pointer to it
Monitor* find_monitor_with_tag(HSTag* tag);
void monitor_focus_by_index(unsigned new_selection);
Monitor* find_monitor_by_name(const char* name);
Monitor* string_to_monitor(const char* string);
Monitor* get_current_monitor();
int monitor_set_tag(Monitor* monitor, HSTag* tag);
void all_monitors_apply_layout();
void ensure_monitors_are_available();

void monitor_update_focus_objects();


#endif


#ifndef __HERBSTLUFT_TAG_H_
#define __HERBSTLUFT_TAG_H_

#include "object.h"
#include "attribute_.h"

#include "glib-backports.h"
#include <memory>

class HSStack;
class HSFrame;
class Settings;

class HSTag : public Object {
public:
    HSTag(std::string name, Settings* settings);
    ~HSTag() override;
    std::shared_ptr<HSFrame>        frame;  // the master frame
    Attribute_<unsigned long> index = {"index", 0};
    Attribute_<bool>         floating = {"floating", false};
    Attribute_<std::string>  name = {"name", {}};   // name of this tag
    DynAttribute_<int> frame_count;
    DynAttribute_<int> client_count;
    DynAttribute_<int> curframe_windex;
    DynAttribute_<int> curframe_wcount;
    int             flags;
    std::shared_ptr<HSStack> stack;
    void setIndexAttribute(unsigned long new_index) override;
private:
    //! get the number of clients on this tag
    int computeClientCount();
    //! get the number of clients on this tag
    int computeFrameCount();
};

void tag_init();
void tag_destroy();

// for tags
HSTag* find_tag(const char* name);
HSTag* find_unused_tag();
HSTag* find_tag_with_toplevel_frame(class HSFrame* frame);
HSTag* get_tag_by_index(int index);
int    tag_get_count();
int tag_set_floating_command(int argc, char** argv, Output output);
void tag_update_focus_layer(HSTag* tag);
void tag_foreach(void (*action)(HSTag*,void*), void* data);
void tag_update_each_focus_layer();
void tag_update_focus_objects();
void tag_force_update_flags();
void tag_update_flags();
void tag_set_flags_dirty();

#endif


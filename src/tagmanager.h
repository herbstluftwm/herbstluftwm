#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "tag.h"
#include "childbyindex.h"
#include "byname.h"


class TagManager : public ChildByIndex<HSTag> {
public:
    TagManager();
    int tag_add_command(Input input, Output output);
    int tag_rename_command(Input input, Output output);
    std::shared_ptr<HSTag> add_tag(const std::string& name);
    std::shared_ptr<HSTag> find(const std::string& name);
    std::shared_ptr<HSTag> ensure_tags_are_available();
    std::shared_ptr<HSTag> byIndexStr(const std::string& index_str, bool skip_visible_tags);
private:
    ByName by_name;
};

extern Ptr(TagManager) tags; // temporarily

#endif

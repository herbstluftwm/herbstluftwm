
#ifndef __HLWM_NAMED_HOOK_H_
#define __HLWM_NAMED_HOOK_H_

#ifdef ENABLE_NAMED_HOOK

#include "object.h"
#include "hook.h"
#include "attribute_.h"

class NamedHook : public Object, public Hook {
public:

    NamedHook(const std::string &path);
    void hook_into(Object* root);

    Type type() { return Type::HOOK; }

    // emit hook, used by path elements
    void operator()(Object* sender, HookEvent event,
                    const std::string &name);

    void trigger(const std::string &action, ArgList args);

    std::string name() { return name_; }

private:
    /* are we listening to an object rather an attribute? If so,
     * our complete chain is 1 element longer than the path (includes root) */
    bool targetIsObject() { return path_.size() < chain_.size(); }

    // for external trigger and called by others
    void emit(const ArgList args);
    // for Event::CHILD_* cases
    void emit(HookEvent event, const std::string &name);
    // for Event::ATTRIBUTE_CHANGED case
    void emit(const std::string &old, const std::string &current);

    // check if chain needs to be altered
    void check_chain(std::shared_ptr<Object> sender, HookEvent event,
                     const std::string &name);

    // remove tail from chain
    void cutoff_chain(size_t length);
    // rebuild chain after existing elements
    void complete_chain();

    void debug_hook(std::shared_ptr<Object> sender = {},
                    HookEvent event = HookEvent::ATTRIBUTE_CHANGED,
                    const std::string &name = {});

    std::string name_;
    // counter attribute
    Attribute_<int> counter_;
    // test if hook is currently working
    Attribute_<bool> active_;
    // external trigger for hook
    Action emit_;

    // chain of directories that report to us
    std::vector<std::weak_ptr<Object>> chain_;
    // tokenized path (derived from our name)
    std::vector<std::string> path_;

    // last known value, used to print new value vs. old value
    std::string value_;
};

#endif // ENABLE_NAMED_HOOK

#endif


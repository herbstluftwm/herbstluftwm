#include "namedhook.h"
#include "object.h"

#include <algorithm>
#include <iostream>

#ifdef ENABLE_NAMED_HOOK

NamedHook::NamedHook(const std::string &path) :
    counter_("counter", 0),
    active_("active", false),
    emit_("emit"),
    path_(Path::split(path)) {
    wireAttributes({ &counter_, &active_ });
    wireActions({ &emit_ });
}

void NamedHook::hook_into(Object* root) {
    cutoff_chain(0);
    chain_ = { root };
    /* we don't register with root; root Object shall never change */
    complete_chain();
}

void NamedHook::operator()(Object* sender, HookEvent event,
                      const std::string &name) {
    //debug_hook(sender, event, name);

    if (event == HookEvent::ATTRIBUTE_CHANGED) {
        if (targetIsObject())
            return;
        if (name != path_.back())
            return;
        auto last = chain_.back().lock();
        if (!last || sender != last)
            return;
        auto o = std::dynamic_pointer_cast<Object>(sender);
        if (!o)
            return; // TODO: throw
        auto newvalue = o->read(name);
        if (newvalue == value_)
            return;

        emit(value_, newvalue);
        value_ = newvalue;
    }
    if (event == HookEvent::CHILD_ADDED || event == HookEvent::CHILD_REMOVED) {
        auto last = chain_.back().lock();
        if (targetIsObject() && last && sender == last) {
            emit(event, name);
        } else { // originating from somewhere in the chain
            check_chain(sender, event, name);
        }
    }

    //debug_hook();
}

void NamedHook::trigger(const std::string &action, ArgList args)
{
    if (action == emit_.name()) {
        emit(args);
        return;
    }
    Object::trigger(action, args);
}

void NamedHook::emit(const ArgList args)
{
    counter_ = counter_ + 1;
    // TODO: properly emit
    std::cout << "Hook " << name_ << " emitting:\t";
    for (auto a : args)
        std::cout << a << " ";
    std::cout << std::endl;
}

void NamedHook::emit(HookEvent event, const std::string &name)
{
    emit({
        (event == HookEvent::CHILD_ADDED ? "added" : "removed"),
        "child", name
    });
}

void NamedHook::emit(const std::string &old, const std::string &current)
{
    if (!old.empty()) {
        if (current.empty()) {
            emit({"cleared from", old});
        } else {
            emit({"changed from", old, "to", current});
        }
    } else {
        emit({"initialized to", current});
    }
}

void NamedHook::check_chain(Object* sender, HookEvent event,
                       const std::string &name) {
    if (event == HookEvent::CHILD_REMOVED) {
        // find sender in chain
        size_t i = 0;
        for (; i < chain_.size(); ++i) {
            auto elem = chain_[i].lock();
            if (!elem)
                return; // TODO: throw, element missing before sender
            if (elem == sender)
                break; // found sender in chain
        }
        if (i == chain_.size())
            return; // TODO: throw, sender not in chain

        if (name != path_[i]) // index i, not i+1, as path does not contain root
            return; // we are not affected

        // next element in our chain was removed. destroy it from there
        cutoff_chain(i+1);
    }
    if (event == HookEvent::CHILD_ADDED) {
        auto last = chain_.back().lock();
        if (!last || sender != last)
            return;

        // something was added. it might already contain children.
        complete_chain();
    }
}

void NamedHook::cutoff_chain(size_t length) {
    auto self = std::dynamic_pointer_cast<NamedHook>(shared_from_this());
    for (auto i = length; i < chain_.size(); ++i) {
        auto o = chain_[i].lock();
        if (o)
            o->removeHook(this);
    }
    chain_.resize(length);
    active_ = false;
}

void NamedHook::complete_chain() {
    auto self = std::dynamic_pointer_cast<NamedHook>(shared_from_this());
    auto current = chain_.back().lock();
    // current should always be o.k., in the worst case it is the root

    // process everything until leaf (note: path_ does not include root)
    for (auto i = chain_.size() - 1; i < path_.size(); ++i) {
        auto next = current->children().find(path_[i]);
        if (next != current->children().end()) {
            next->second->addHook(this);
            chain_.emplace_back(next->second);
            current = next->second;
        }
    }

    if (targetIsObject()) {
        active_ = true;
        return;
    }

    // now process leaf in case it is an attribute
    if (chain_.size() < path_.size())
        return; // no chance, we are still incomplete

    auto o = std::dynamic_pointer_cast<Object>(chain_.back().lock());
    if (!o || !o->attribute(path_.back()))
        return; // TODO: throw
    active_ = true;
    auto newvalue = o->read(path_.back());
    if (newvalue != value_) {
        emit(value_, newvalue);
        value_ = newvalue;
    }
}

void NamedHook::debug_hook(std::shared_ptr<Object> sender, HookEvent event,
                      const std::string &name)
{
    if (sender) {
        std::cerr << "\t" << name_ << " triggered";
        std::string eventstr = (event == HookEvent::CHILD_ADDED ? "added"
                             : (event == HookEvent::CHILD_REMOVED ? "removed"
                                                                  : "changed"));
        std::cerr << " with " << name << " being " << eventstr;
        std::cerr << std::endl;
    }
    // std::cerr << "\tChain is: ";
    // for (auto c : chain_) {
    //     auto d = c.lock();
    //     if (d)
    //         std::cerr << d->name() << ".";
    //     else
    //         std::cerr << "!";
    // }
    // std::cerr << std::endl;
}
#endif // ENABLE_NAMED_HOOK


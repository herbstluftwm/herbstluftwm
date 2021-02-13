#include "watchers.h"

#include "argparse.h"
#include "completion.h"
#include "hook.h"
#include "metacommands.h"
#include "object.h"

using std::string;

Watchers::Watchers()
    : count_(this, "count", &Watchers::count)
{
    count_.setDoc("the number of attributes that are watched");
}

void Watchers::injectDependencies(Object* root)
{
    root_ = root;
}

void Watchers::scanForChanges()
{
    for (auto& it : lastValue_) {
        Attribute* attr = root_->deepAttribute(it.first);
        string newValue = "";
        if (attr) {
            newValue = attr->str();
        }
        if (newValue != it.second) {
            hook_emit({"attribute_changed", it.first, it.second, newValue});
            it.second = newValue;
        }
    }
}

int Watchers::watchCommand(Input input, Output output)
{
    string path;
    ArgParse args = ArgParse().mandatory(path);
    if (args.parsingAllFails(input, output)) {
        return args.exitCode();
    }
    string value = "";
    Attribute* attr = root_->deepAttribute(path);
    if (attr) {
        value = attr->str();
    }
    lastValue_[path] = value;
    return 0;
}

void Watchers::watchCompletion(Completion& complete)
{
    if (complete == 0) {
        MetaCommands::completeObjectPath(complete, root_, true);
    } else {
        complete.none();
    }
}

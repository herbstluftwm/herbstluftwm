#ifndef GLOBALCOMMANDS_H
#define GLOBALCOMMANDS_H

#include "commandio.h"

class Root;

/**
 * @brief The GlobalCommands class collects commands
 * that affect the global state of hlwm and that inspect
 * its structure. Hence, this requires a Root reference
 * (in contrast to MetaCommands which only require a
 * reference of type Object)
 */
class GlobalCommands
{
public:
    GlobalCommands(Root& root);
    int tagStatusCommand(Input input, Output output);
    void tagStatusCompletion(Completion& complete);

    int jumptoCommand(Input input, Output output);
    void jumptoCompletion(Completion& complete);

    int bringCommand(Input input, Output output);
    void bringCompletion(Completion& complete);
private:
    Root& root_;
};

#endif // GLOBALCOMMANDS_H

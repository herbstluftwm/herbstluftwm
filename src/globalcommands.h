#ifndef GLOBALCOMMANDS_H
#define GLOBALCOMMANDS_H

#include "commandio.h"

class Client;
class Monitor;
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
    void tagStatusCommand(CallOrComplete invoc);
    void tagStatus(Monitor* monitor, Output output);

    int focusEdgeCommand(Input input, Output output);
    void focusEdgeCompletion(Completion& complete);
    int shiftEdgeCommand(Input input, Output output);
    void shiftEdgeCompletion(Completion& complete);

    void useTagCommand(CallOrComplete invoc);
    void useTagByIndexCommand(CallOrComplete invoc);

    int cycleValueCommand(Input input, Output output);
    void cycleValueCompletion(Completion& complete);

    void usePreviousCommand(CallOrComplete invoc);

    void jumptoCommand(CallOrComplete invoc);

    void bringCommand(CallOrComplete invoc);
    void bring(Client* client);

    void closeCommand(CallOrComplete invoc);

    void raiseCommand(CallOrComplete invoc);
    void lowerCommand(CallOrComplete invoc);

    void focusNthCommand(CallOrComplete invoc);
private:
    Root& root_;
};

#endif // GLOBALCOMMANDS_H

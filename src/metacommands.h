#ifndef __HERBSTLUFT_METACOMMANDS_H_
#define __HERBSTLUFT_METACOMMANDS_H_

#include <functional>
#include <memory>
#include <vector>

#include "attribute.h"
#include "commandio.h"
#include "converter.h"

class Object;
class Completion;

/** this class collects high-level commands that don't need any internal
 * structures but instead just uses:
 *
 *   - the object tree as the user sees it
 *   - the command system (invokation of commands)
 *   - generic c functions (e.g. accessing the environment)
 *
 * Hence, this does not inherit from Object and is not exposed to the user as an object.
*/
class MetaCommands {
public:
    /** This class shall have minimal dependencies to other hlwm modules, therefore the
     * 'root' reference held by this class has the Object type instead of Root.
     */
    MetaCommands(Object& root);

    Attribute* getAttribute(std::string path, Output output);

    /* external interface */
    // find an attribute deep in the object tree.
    // on failure, the error message is printed to output and NULL
    // is returned
    int get_attr_cmd(Input in, Output output);
    void get_attr_complete(Completion& complete);
    int set_attr_cmd(Input in, Output output);
    void set_attr_complete(Completion& complete);
    int attr_cmd(Input in, Output output);
    void attr_complete(Completion& complete);
    int print_object_tree_command(Input in, Output output);
    void print_object_tree_complete(Completion& complete);
    int attrTypeCommand(Input input, Output output);
    void attrTypeCompletion(Completion& complete);

    int substitute_cmd(Input input, Output output);
    void substitute_complete(Completion& complete);
    int foreachCmd(Input input, Output output);
    void foreachComplete(Completion& complete);
    int sprintf_cmd(Input input, Output output);
    void sprintf_complete(Completion& complete);
    int new_attr_cmd(Input input, Output output);
    void new_attr_complete(Completion& complete);
    int remove_attr_cmd(Input input, Output output);
    void remove_attr_complete(Completion& complete);
    int compare_cmd(Input input, Output output);
    void compare_complete(Completion& complete);
    static Attribute* newAttributeWithType(std::string typestr, std::string attr_name, Output output);
    static void completeAttributeType(Completion& complete);
    static void completeObjectPath(Completion& complete, Object* rootObject,
                                   bool attributes = false,
                                   std::function<bool(Attribute*)> attributeFilter = {});
    void completeObjectPath(Completion& complete, bool attributes = false,
                            std::function<bool(Attribute*)> attributeFilter = {});
    void completeAttributePath(Completion& complete);

    int helpCommand(Input input, Output output);
    void helpCompletion(Completion& complete);

    int tryCommand(Input input, Output output);
    int silentCommand(Input input, Output output);
    int negateCommand(Input input, Output output);
    void completeCommandShifted1(Completion& complete);
    int echoCommand(Input input, Output output);
    void echoCompletion(Completion& ) {}; // no completion

    int setenvCommand(Input input, Output output);
    void setenvCompletion(Completion& complete);
    int exportEnvCommand(Input input, Output output);
    void exportEnvCompletion(Completion& complete);
    int getenvCommand(Input input, Output output);
    void getenvUnsetenvCompletion(Completion& complete); //! completion for unsetenv and getenv
    int unsetenvCommand(Input input, Output output);
    void completeEnvName(Completion& complete);

    int chainCommand(Input input, Output output);
    void chainCompletion(Completion& complete);

    std::vector<std::vector<std::string>> splitCommandList(ArgList::Container input);
private:
    Object& root;
    std::vector<std::unique_ptr<Attribute>> userAttributes_;

    class FormatStringBlob {
    public:
        bool literal_; //! whether the data_ field is understood literall
        std::string data_; //! text blob or placeholder
    };
    typedef std::vector<FormatStringBlob> FormatString;
    FormatString parseFormatString(const std::string& format);
};


#endif

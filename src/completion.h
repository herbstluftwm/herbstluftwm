#ifndef HLWM_COMPLETION
#define HLWM_COMPLETION

#include <functional>

#include "arglist.h"
#include "commandio.h"

namespace Commands {
void complete(Completion& completion);
}

/** The completion object holds the state of a running
 * command completion, that is the list of args and the index
 * of the element in the arglist that needs to be completed. A completion
 * object shall be passed around and not be copied.
 *
 * == Additional Information for the creator of the Completion-object: ==
 *
 * All given completions are printed to the output given in the constructor. If
 * shellOutput is set, then the output is treated literally by the shell. This
 * means that full("status") will append a space on the shell whereas something
 * like partial("clients.focus.") will not append a space in the shell input line.
 *
 * However, if the shellOutput mode is not active, all completions are treated
 * as full arguments.
 */
class Completion {
public:
    Completion(ArgList args, size_t index, const std::string& prepend, bool shellOutput, Output output);

    //! the given word is a possible argument, e.g. full("status")
    void full(const std::string& word);
    void full(const std::initializer_list<std::string>& wordList);

    //! the given word is a partial argument, e.g. partial("fullscreen=")
    void partial(const std::string& word);

    //! there is no more parameter expected
    void none();
    //! reset a previous call to none()
    void parametersStillExpected();

    //! some of the previous arguments are invalid
    void invalidArguments();


    /** State queries:
     */
    //! if none(); was called
    bool noParameterExpected() const;
    //! if invalidArguments() was called;
    bool ifInvalidArguments() const;

    /** compare the position of the argument that is completed
     * The first parameter has index 0
     */
    bool operator==(size_t index) const { return index_ == index; }
    bool operator<=(size_t index) const { return index_ <= index; }
    bool operator<(size_t index) const { return index_ < index; }
    bool operator>=(size_t index) const { return index_ >= index; }
    bool operator>(size_t index) const { return index_ > index; }
    size_t index() const { return index_; }

    std::string operator[](size_t index) const;

    static bool prefixOf(const std::string& shorter, const std::string& longer);
    const std::string& needle() const;
    size_t needleIndex() const { return index_; };

    /** Grants access to private members as long as Commands::complete is still
     * wrapper around complete_against_commands.
     */
    friend void Commands::complete(Completion& completion);

    void completeCommands(size_t offset);
    void withPrefix(const std::string& prependPrefix, std::function<void(Completion&)> callback);
private:
    /** The intended use is to pass the completion state as the reference and
     * to return possible completions via this Completion object. This is why
     * the operator= and the copy constructor are private. It ensures that a
     * completion object is not accidentally duplicated.
     */
    void operator=(const Completion& other);
    Completion(const Completion& other);
    void mergeResultsFrom(Completion& source);

    Completion shifted(size_t offset) const;
    std::string escape(const std::string& str);

    ArgList args_;
    size_t index_;
    std::string needle_;
    std::string prepend_; //! a string that is prepended to all completion results
    Output output_;
    bool   shellOutput_;
    bool   noParameterExpected_ = false;
    bool   invalidArgument_ = false;
};

#endif


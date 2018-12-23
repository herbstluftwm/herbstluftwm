#ifndef HLWM_COMPLETION
#define HLWM_COMPLETION

#include "arglist.h"
#include "types.h"

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
    Completion(ArgList args, size_t index, bool shellOutput, Output output);

    //! the given word is a possible argument, e.g. full("status")
    void full(const std::string& word);
    void full(const std::initializer_list<std::string>& wordList);

    //! the given word is a partial argument, e.g. partial("fullscreen=")
    void partial(const std::string& word);

    //! there is no more parameter expected
    void none();

    bool noParameterExpected() const;

    /** compare the position of the argument that is completed
     * The first parameter has index 0
     */
    bool operator==(size_t index) const { return index_ == index; }
    bool operator<=(size_t index) const { return index_ <= index; }
    bool operator<(size_t index) const { return index_ < index; }

    std::string operator[](size_t index) const;

    static bool prefixOf(const std::string& shorter, const std::string& longer);
    const std::string& needle() const;
private:
    /** The intended use is to pass the completion state as the reference and
     * to return possible completions via this Completion object. This is why
     * the operator= and the copy constructor are private. It ensures that a
     * completion object is not accidentally duplicated.
     */
    Completion(const Completion& other);
    void operator=(const Completion& other);

    std::string escape(const std::string& str);

    ArgList args_;
    size_t index_;
    std::string needle_;
    Output output_;
    bool   shellOutput_;
    bool   noParameterExpected_ = false;
};

#endif


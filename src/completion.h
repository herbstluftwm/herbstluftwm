#ifndef HLWM_COMPLETION
#define HLWM_COMPLETION

#include "arglist.h"

/** The completion object holds the state of a running
 * command completion, that is the list of args and the index
 * of the element in the arglist that needs to be completed.
 *
 * The intended use is to pass the completion state as the reference and to
 * return possible completions via this Completion object. This is why the =
 * and the copy constructor are private. It ensures that a completion object
 * is not accidentally duplicated.
 *
 */
class Completion {
public:
    Completion(ArgList args, size_t index);

private:
    Completion(const Completion& other);
    void operator=(const Completion& other);
    ArgList args_;
    size_t index_;
};

#endif


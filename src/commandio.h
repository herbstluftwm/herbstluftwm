#pragma once

/** types and utilities for input and output
 * of commands
 */

#include "arglist.h"

class Completion;

/* A path in the object tree */
using Path = ArgList;

/* Types for I/O with the user */
/**
 * @brief A bundle of the standard output and the
 * error output channel. Writing to it (via the << operator) just passes
 * the data to the output channel.
 *
 * It is recommended to pass references to OutputChannels (see the Output type)
 * around to avoid that the command name is copied over and over again.
 */
class OutputChannels {
public:
    OutputChannels(const std::string& commandName, std::ostream& output, std::ostream& error)
        : commandName_(commandName)
        , output_(output)
        , error_(error)
    {}
    static OutputChannels stdio();

    const std::string& command() {
        return commandName_;
    }

    std::ostream& error() {
        return error_;
    }

    std::ostream& output() {
        return output_;
    }

    //! same as error() but prefix with the command name.
    std::ostream& perror();

    // writing data to output
    template<typename T>
    OutputChannels& operator<<(const T& arg) {
        output_ << arg;
        return *this;
    }

    // passing flags to output, e.g. std::endl
    OutputChannels& operator<<(std::ostream& (*f)(std::ostream&)) {
        f(output_);
        return *this;
    }

private:
    std::string commandName_;
    std::ostream& output_;
    std::ostream& error_;
};

using Output = OutputChannels&;

/** The Input for a command consists of a command name
 * (known as argv[0] in a C main()) and of arguments (argv[1] up to
 * argv[argc] in a C main()).
 *
 * This class is in transition and not yet in its final state!
 * Currently, this class behaves as the ArgList. But later, ArgList will become
 * protected and so one can only use >> to access the arguments
 *
 * This means that currently, the base class holds the entire argv. Later, the
 * base class will only hold the arguments and not the command name and
 * furthermore the ArgList parent class will become private.
 */
class Input : public ArgList {
public:
    Input(const std::string command, const Container &c = {})
        : ArgList(c), command_(std::make_shared<std::string>(command)) {}

    Input(const std::string command, Container::const_iterator from, Container::const_iterator to)
        : ArgList(from, to), command_(std::make_shared<std::string>(command)) {}

    //! create a new Input but drop already parsed arguments
    Input(const Input& other)
        : ArgList(other.toVector()), command_(other.command_) {}

    const std::string& command() const { return *command_; }

    Input &operator>>(std::string &val) override;

    //! construct a new Input where the first (current) arg is the command
    Input fromHere();

    //! Replace every occurence of 'from' by 'to'
    //! @note this includes the command itself
    void replace(const std::string &from, const std::string &to);

protected:
    //! Command name
    //! A shared pointer to avoid copies when passing Input around
    //! @note The C-style compatibility layer DEPENDS on the shared_ptr!
    std::shared_ptr<std::string> command_;
};

/**
 * @brief The CallOrComplete class combines the functionality of
 * 1. calling a command and 2. completing the arguments of a command.
 * If a Completion object is given, the command should perform argument
 * completion, and if Input/Output is given, the command should actually be
 * called.
 *
 * This class should not be used directly, but only implicitly
 * on the calling side (class Command) and on the receiving side (ArgParse)
 */
class CallOrComplete {
public:
    static CallOrComplete complete(Completion& complete) {
        CallOrComplete invoc;
        invoc.complete_ = &complete;
        return invoc;
    }
private:
    friend class CommandBinding;
    friend class ArgParse;
    Completion* complete_ = nullptr;
    std::pair<Input, Output>* inputOutput_ = nullptr;
    int* exitCode_ = nullptr;
};

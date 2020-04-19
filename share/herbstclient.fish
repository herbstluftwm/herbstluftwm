# fish completion for herbstclient

function _first_hlwm_command
    # Find first hlwm command in given tokens (e.g. commandline -o)
    # Print token position if found; exit status 0 if found, else 1

    for i in (seq 2 (count $argv)) # start after 'herbstclient'
        if not string match -q -- "-*" $argv[$i]
            echo $i
            return 0
        end
    end
    return 1
end

function _get_herbstluftwm_completion
    set tokens (commandline -o)
    set first (_first_hlwm_command $tokens)
    if not test -n $first; return; end # no command found, we are done
    set tokens $tokens[$first..-1]

    set -l position (count $tokens)

    # If there are tokens and cursor sits on a (possibly unfinished) word
    # decrease position due to 0-based in hlwm and 1-based in fish
    if test $position -gt 0 -a -n (commandline -ct)
        set position (math "$position - 1")
    end
    herbstclient -q complete $position $tokens
end

function _complete_herbstclient
    # do not complete herbstclient options after commands
    # TODO: this could check if cursor sits in front of said command
    complete -fc herbstclient -n 'not _first_hlwm_command (commandline -o)' $argv
end

# add completions for herbstclient options
_complete_herbstclient -s n -l no-newline -d 'Do not print a newline if output does not end with a newline.'
_complete_herbstclient -s 0 -l print0 -d 'Use the null character as delimiter between the output of hooks.'
_complete_herbstclient -s l -l last-arg -d 'When using -i or -w, only print the last argument of the hook.'
_complete_herbstclient -s i -l idle -d 'Wait for hooks instead of executing commands.'
_complete_herbstclient -s w -l wait -d 'Same as --idle but exit after first --count hooks.'
_complete_herbstclient -s c -l count -r -d 'Let --wait exit after COUNT hooks were received and printed.'
_complete_herbstclient -s q -l quiet -d 'Do not print error messages if herbstclient cannot connect to the running herbstluftwm instance.'
_complete_herbstclient -s v -l version -d 'Print the herbstclient version.'
_complete_herbstclient -s h -l help -d 'Print the herbstclient usage with its command line options.'

# defer all other completions to herbstluftwm
complete -xc herbstclient -d "" -a '(_get_herbstluftwm_completion)'


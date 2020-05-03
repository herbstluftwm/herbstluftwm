# fish completion for herbstclient

function _first_hlwm_command
    # Find first hlwm command in given tokens (e.g. commandline -o)
    # Print token position if found, else print no. tokens + 1
    # Exit status 0 if found, 1 if not found, 2 if not allowed due to options

    for i in (seq 2 (count $argv)) # start after 'herbstclient'
        if string match -qr -- '^(-v|-h|--help|-w|--wait|-i|--idle)$' $argv[$i]
            return 2 # these options do not take commands
        end
        if not string match -q -- "-*" $argv[$i]
            echo $i
            return 0
        end
    end
    math (count $argv) + 1
    return 1
end

function _get_herbstluftwm_completion
    set tokens (commandline -op)
    set first (_first_hlwm_command $tokens)
    if test $status -eq 2; return; end # no hlwm completion desired
    set tokens $tokens[$first..-1]

    # TODO: we should derive the real position but it is tricky
    set position (count $tokens)
    # cursor sits on a (possibly unfinished) word
    if test -n (commandline -ct) -a $position -gt 0
        set posadjusted (math "$position - 1") # 0-based position in hlwm
        set completions (herbstclient -q complete_shell $posadjusted $tokens)
        # check for partial completion, we assume it only occurs exclusively
        if test (count $completions) -eq 1; and string match -qr -- '[^\s]$' $completions[1]
            # eagerly retrieve next available full completions
            set tokens[$position] $completions[1]
            set -a completions (herbstclient -q complete_shell $posadjusted $tokens)
        end
    else
        # use unaltered position which is the last token + 1 (for hlwm)
        set completions (herbstclient -q complete_shell $position $tokens)
    end

    for result in $completions # return as a newline-delimited list
        # output w/o trailing spaces fish doesn't understand
        string trim -r -- $result
    end
end

function _complete_herbstclient
    # do not complete herbstclient options after commands
    complete -fc herbstclient -n 'not _first_hlwm_command (commandline -op --cut-at-cursor)' $argv
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


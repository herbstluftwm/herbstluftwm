# fish completion for herbstclient

function _get_herbstluftwm_completion
    set -l tokens (commandline -op)

    # Filter herbstclient related tokens
    set -l index 1
    for token in $tokens
        if string match -- "$token" "herbstclient"
            or string match -- "-" (string sub -l 1 -- $token)
            set -e tokens[$index]
        end
        set index (math "$index + 1")
    end

    # If no tokens remain return everything
    if test (count $tokens) -eq 0
        herbstclient -q complete 0
        return
    end

    # Position is zero-based in hlwm
    set -l position (math (count $tokens) - 1)
    set -l completions (herbstclient -q complete $position $tokens)

    # If the last parameter equals the only result increase the position to get the next token
    if test (count $completions) -eq 1
        and string match -- "$completions" "$tokens[-1]"
        set position (math $position + 1)
        herbstclient -q complete $position $tokens
    else
        string replace --all -- ' ' \n "$completions"
    end
end

complete -xc herbstclient -d "" -a '(_get_herbstluftwm_completion)'

complete -c herbstclient -f -s n -l no-newline -d 'Do not print a newline if output does not end with a newline.'
complete -c herbstclient -f -s 0 -l print0 -d 'Use the null character as delimiter between the output of hooks.'
complete -c herbstclient -f -s l -l last-arg -d 'When using -i or -w, only print the last argument of the hook.'
complete -c herbstclient -f -s i -l idle -d 'Wait for hooks instead of executing commands.'
complete -c herbstclient -f -s w -l wait -d 'Same as --idle but exit after first --count hooks.'
complete -c herbstclient -f -s c -l count -r -d 'Let --wait exit after COUNT hooks were received and printed.'
complete -c herbstclient -f -s q -l quiet -d 'Do not print error messages if herbstclient cannot connect to the running herbstluftwm instance.'
complete -c herbstclient -f -s v -l version -d 'Print the herbstclient version.'
complete -c herbstclient -f -s h -l help -d 'Print the herbstclient usage with its command line options.'

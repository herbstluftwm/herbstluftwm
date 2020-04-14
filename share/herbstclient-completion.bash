# bash completion for herbstclient

_herbstclient_complete() {
    local IFS=$'\n'
    # do not split at =, because BASH would not split at a '='.
    COMP_WORDBREAKS=${COMP_WORDBREAKS//=}
    COMPREPLY=(
        # just call the herbstclient complete .. but without herbstclient as argument
        $(herbstclient -q complete_shell "$((COMP_CWORD-1))" "${COMP_WORDS[@]:1}")
    )
}

complete -F _herbstclient_complete -o nospace herbstclient

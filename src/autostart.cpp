#include "autostart.h"

#include <unistd.h>

#include "globals.h"

using std::pair;
using std::string;

Autostart::Autostart(const string& autostartFromCmdLine, const string& globalAutostart)
    : autostart_path_(this, "path", autostartFromCmdLine)
    , global_autostart_path_(this, "global_path", globalAutostart)
    , pid_(this, "pid", 0)
    , running_(this, "running", false)
    , last_status_(this, "last_status", 0)
{
    autostart_path_.setWritable();
    autostart_path_.setDoc(
                "Custom path to the user\'s autostart path. "
                "If it is empty, then the autostart in $XDG_CONFIG_HOME "
                "or $HOME is used.");
    global_autostart_path_.setDoc("Path of the system-wide autostart, used as a fallback.");
    pid_.setDoc(
                "the process id of the last autostart invokation. "
                "Even if the autostart is not running anymore, its pid "
                "is still present here.");
    running_.setDoc("whether the autostart process (with \'pid\') is still running.");
    last_status_.setDoc(
                "the exit status of the last autostart run. "
                "if the autostart is still \'running\', then this "
                "status corresponds to the exit status of the previous "
                "autostart invocation."
    );
}

void Autostart::reloadCmd()
{
    string path;
    if (!autostart_path_().empty()) {
        path = autostart_path_();
    } else {
        // find right directory
        char* xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            path = xdg_config_home;
        } else {
            char* home = getenv("HOME");
            if (!home) {
                HSWarning("Will not run autostart file. "
                          "Neither $HOME or $XDG_CONFIG_HOME is set.\n");
                return;
            }
            path = string(home) + "/.config";
        }
        path += "/" HERBSTLUFT_AUTOSTART;
    }
    pid_t pid = fork();
    if (0 == pid) {
        // in the child
        if (g_display) {
            close(ConnectionNumber(g_display));
        }
        setsid();
        execl(path.c_str(), path.c_str(), nullptr);

        const char* global_autostart = global_autostart_path_->c_str();
        HSDebug("Cannot execute %s, falling back to %s\n", path.c_str(), global_autostart);
        execl(global_autostart, global_autostart, nullptr);

        fprintf(stderr, "herbstluftwm: execvp \"%s\"", global_autostart);
        perror(" failed");
        exit(EXIT_FAILURE);
    } else {
        // in the parent process
        pid_ = static_cast<unsigned long>(pid);
        running_ = true;
    }
}

void Autostart::childExited(pair<pid_t, int> childInfo)
{
    if (childInfo.first <= 0) {
        return;
    }
    unsigned long pidUnsigned = static_cast<unsigned long>(childInfo.first);
    if (pidUnsigned == pid_()) {
        running_ = false;
        last_status_ = childInfo.second;
    }
}

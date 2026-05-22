#include "zero_shell/process_runner.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

namespace zero_shell {
namespace {

const char *first_nonempty_env(const char *first, const char *second, const char *fallback)
{
    const char *value = std::getenv(first);
    if (value && *value) {
        return value;
    }

    value = std::getenv(second);
    if (value && *value) {
        return value;
    }

    return fallback;
}

void apply_child_display_environment()
{
    const char *fbdev = first_nonempty_env("CARDPUTER_ZERO_FB", "ZEROSHELL_FBDEV", "/dev/fb1");
    setenv("CARDPUTER_ZERO_FB", fbdev, 0);
    setenv("ZEROSHELL_FBDEV", fbdev, 0);
    setenv("LV_LINUX_FBDEV_DEVICE", fbdev, 0);
    setenv("APPLAUNCH_LINUX_FBDEV_DEVICE", fbdev, 0);
}

int run_shell_command(const std::string &command)
{
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);
        apply_child_display_environment();
        execlp("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    setpgid(pid, pid);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
}

} // namespace

int run_blocking_command(const std::string &command)
{
    return run_shell_command(command);
}

int run_terminal_command(const std::string &command, bool sysplause)
{
    std::string wrapped = command;
    if (sysplause) {
        wrapped += "; printf '\\n[ZeroShell] command exited. Press Enter...'; read _";
    }
    return run_shell_command(wrapped);
}

int run_zero_helper(const std::string &arguments)
{
    return run_shell_command("/usr/local/sbin/zero-helper " + arguments);
}

} // namespace zero_shell

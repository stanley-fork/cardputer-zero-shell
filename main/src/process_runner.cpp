#include "zero_shell/process_runner.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

namespace zero_shell {
namespace {

int run_shell_command(const std::string &command)
{
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);
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
    return run_shell_command("sudo /usr/local/sbin/zero-helper " + arguments);
}

} // namespace zero_shell

#include "command.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include <algorithm>
#include <chrono>

namespace command {

#if defined(_WIN32)

// Not yet implemented for Windows - fork/exec/pipe/waitpid below are POSIX-only.
// Tracked alongside the existing "Cross-platform verification pass (Windows)"
// issue rather than guessed at here without a way to test it.
CommandResult run(const CommandOptions& options) {
  CommandResult result;
  result.setup_error = "run_command is not yet supported on Windows.";
  return result;
}

#else

namespace {

// Reaps every zombie currently parented to us, without blocking. A killed
// shell's own children (e.g. a backgrounded "sleep 100 &" inside the
// command) aren't OUR direct child - killpg() still signals them since they
// share the process group, but nothing calls wait() for them once their
// immediate parent (the shell) is gone, so they'd otherwise sit as zombies
// until reaped by *something*. On Linux, marking ourselves a child subreaper
// (below) means the kernel reparents them to us instead of PID 1, so this
// loop actually catches them instead of relying on the OS's init to get to
// it eventually - which some minimal/containerized environments never do.
void reapAllPendingChildren() {
  // Keep reaping while we're finding something (there could be more than
  // one), and if a child exists but hasn't finished dying yet (SIGKILL is
  // asynchronous - the kernel still needs a scheduling opportunity to
  // actually terminate it), give it a brief moment and retry. Bounded so a
  // truly stuck child can't loop this forever.
  for (int attempt = 0; attempt < 10; ++attempt) {
    pid_t reaped = waitpid(-1, nullptr, WNOHANG);
    if (reaped > 0) {
      continue;
    }
    if (reaped == 0) {
      struct timespec ts = {0, 20000000L};  // 20ms
      nanosleep(&ts, nullptr);
      continue;
    }
    break;  // reaped == -1 (ECHILD): no children left waiting - done
  }
}

}  // namespace

CommandResult run(const CommandOptions& options) {
  CommandResult result;

  if (options.command.empty()) {
    result.setup_error = "No command provided.";
    return result;
  }

#if defined(__linux__)
  // Makes orphaned descendants of the child we're about to fork (e.g. a
  // backgrounded subprocess whose immediate parent gets killed) reparent to
  // *us* rather than PID 1, so reapAllPendingChildren() below can actually
  // reap them. Idempotent and cheap enough to just call every time rather
  // than tracking whether it's already set.
  prctl(PR_SET_CHILD_SUBREAPER, 1);
#endif

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    result.setup_error = "Failed to create pipe.";
    return result;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    result.setup_error = "Failed to fork.";
    return result;
  }

  if (pid == 0) {
    // Child: become its own process group leader so a timeout can kill any
    // subprocesses it spawns too (e.g. "sleep 100 &" inside the command),
    // not just the immediate shell.
    setpgid(0, 0);
    close(pipe_fds[0]);
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);
    execl("/bin/sh", "sh", "-c", options.command.c_str(), (char*)nullptr);
    _exit(127);  // execl only returns on failure
  }

  // Parent: also set the child's process group from this side, to close the
  // race where we might try to killpg() before the child's own setpgid()
  // call has run. Calling it from both sides is the standard idiom - it's
  // idempotent, whichever side runs first "wins".
  setpgid(pid, pid);
  close(pipe_fds[1]);

  int read_fd = pipe_fds[0];
  fcntl(read_fd, F_SETFL, O_NONBLOCK);

  std::string output;
  bool timed_out = false;
  bool truncated = false;
  bool child_reaped = false;
  int status = 0;

  auto start = std::chrono::steady_clock::now();
  auto appendChunk = [&](const char* buf, ssize_t n) {
    output.append(buf, static_cast<size_t>(n));
    if (output.size() > options.max_output_bytes) {
      output.resize(options.max_output_bytes);
      truncated = true;
    }
  };

  while (true) {
    long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    long remaining_ms = static_cast<long>(options.timeout_seconds) * 1000 - elapsed_ms;
    if (remaining_ms <= 0) {
      timed_out = true;
      killpg(pid, SIGKILL);
      waitpid(pid, &status, 0);
      reapAllPendingChildren();
      child_reaped = true;
      break;
    }

    struct pollfd pfd;
    pfd.fd = read_fd;
    pfd.events = POLLIN;
    // Poll in short slices (capped at 200ms) so a long timeout doesn't mean
    // a long delay before we notice the child has already exited.
    int poll_timeout = static_cast<int>(std::min<long>(remaining_ms, 200));
    int poll_ret = poll(&pfd, 1, poll_timeout);

    if (poll_ret > 0 && (pfd.revents & POLLIN)) {
      char buf[4096];
      ssize_t n = read(read_fd, buf, sizeof(buf));
      if (n > 0) {
        appendChunk(buf, n);
        if (truncated) {
          killpg(pid, SIGKILL);
          waitpid(pid, &status, 0);
          reapAllPendingChildren();
          child_reaped = true;
          break;
        }
      }
    }

    pid_t wait_ret = waitpid(pid, &status, WNOHANG);
    if (wait_ret == pid) {
      // Drain whatever's left in the pipe before declaring done - the child
      // may have written its last chunk right before exiting.
      char buf[4096];
      ssize_t n;
      while ((n = read(read_fd, buf, sizeof(buf))) > 0) {
        appendChunk(buf, n);
        if (truncated) break;
      }
      child_reaped = true;
      break;
    }
  }

  if (!child_reaped) {
    waitpid(pid, &status, 0);
  }
  close(read_fd);

  result.ok = true;
  result.output = output;
  result.timed_out = timed_out;
  result.truncated = truncated;

  if (timed_out) {
    result.exit_code = -1;
  } else if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = -WTERMSIG(status);
  }

  return result;
}

#endif

}  // namespace command
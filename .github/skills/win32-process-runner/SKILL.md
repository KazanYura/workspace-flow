---
name: win32-process-runner
description: 'Implement process launching and management on Windows for DevFlow Orchestrator. Use when writing the Subprocess/command/poll execution code — CreateProcessW, capturing stdout/stderr via anonymous pipes, waiting on exit codes, timeouts, and terminating or suspending processes with RAII handle wrappers.'
---

# Win32 Process Runner

Guidance for spawning and controlling child processes for `command` and `poll` tasks. Every OS handle must be owned by an RAII wrapper — no manual `CloseHandle` on happy paths only.

## When to Use
- Building the `Subprocess` wrapper or `PipelineRunner` execution branches.
- Capturing a child's stdout/stderr into the log queue.
- Implementing timeouts, exit-code checks, kill, or suspend/resume.

## RAII Handle Wrapper
Wrap `HANDLE` so it can never leak:
```cpp
struct HandleDeleter {
  void operator()(HANDLE h) const noexcept {
    if (h && h != INVALID_HANDLE_VALUE) ::CloseHandle(h);
  }
};
using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;
```

## Launch + Capture Pattern
1. Create an inheritable anonymous pipe for the child's stdout/stderr:
   ```cpp
   SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
   HANDLE rd = nullptr, wr = nullptr;
   ::CreatePipe(&rd, &wr, &sa, 0);
   UniqueHandle read_end(rd), write_end(wr);
   ::SetHandleInformation(read_end.get(), HANDLE_FLAG_INHERIT, 0); // parent keeps read private
   ```
2. Fill `STARTUPINFOW` with `STARTF_USESTDHANDLES`, pointing `hStdOutput`/`hStdError` at the pipe's write end.
3. Convert the UTF-8 command to a mutable UTF-16 buffer (`CreateProcessW` may write to the command line arg). Call with `bInheritHandles = TRUE`.
4. **Close the parent's copy of the write end immediately** after `CreateProcessW`, or the read will never see EOF.
5. Read the pipe in a loop on a worker thread; push lines into the thread-safe log queue.
6. `WaitForSingleObject(pi.hProcess, timeout_ms)` — on `WAIT_TIMEOUT`, `TerminateProcess` and report failure. On success, `GetExitCodeProcess`.

## Poll Semantics
For a `poll` task, run the probe (either `CreateProcessW` + exit code, or a direct check such as `CreateFileW` on `\\.\pipe\docker_engine`) every `retry_interval_sec` up to `max_retries`. Use a `std::stop_token`-aware sleep so cancellation is responsive.

## Suspend / Resume (for thermal mitigation)
There is no single Win32 "suspend process" call. Enumerate the target's threads via a toolhelp snapshot and `SuspendThread`/`ResumeThread` each, or call the undocumented `NtSuspendProcess`/`NtResumeProcess` from `ntdll.dll`. Prefer `TerminateProcess` when the target need not resume.

## Pitfalls
- Forgetting to close the parent's pipe write handle → the reader hangs forever waiting for EOF.
- Passing a string literal to `CreateProcessW`'s command-line arg → undefined behavior; use a writable buffer.
- Not checking `GetLastError()` after a `FALSE` return.
- Blocking the pipe read on the main/GUI thread — always read on a worker.

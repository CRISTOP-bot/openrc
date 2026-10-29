# OpenRC on NEWOS — port status and engineering plan

This fork is being adapted for **NEWOS**. It remains OpenRC: the goal is to
preserve OpenRC's dependency-based runlevels, service state, service scripts,
and user-facing commands, while replacing or extending the operating-system
integration where NEWOS requires it. A renamed shell or a command-shaped demo
is not a port.

## Current status

**Porting investigation only. No OpenRC component in this branch is yet
validated to build or run on NEWOS. Do not install this fork as NEWOS PID 1.**

This document is based on upstream OpenRC commit
`14e11b7f5b09072aa1433e790bbf832800858fec` and NEWOS's current kernel,
userland, and ABI sources. Recheck those sources before implementing each
stage; the NEWOS superproject pins component repositories independently.

## Verified NEWOS baseline

NEWOS has a freestanding x86-64 kernel and native `/init` shell. The current
userland exposes `SYS_SPAWN2`/`SYS_SPAWN3`, `SYS_WAITPID`, `SYS_KILL`, pipes,
file descriptors, `sleep`, and a small native shell capable of reading simple
startup scripts. Its init path is selected by the kernel and is not currently
an OpenRC init process.

The public ABI documentation explicitly says `fork`, `execve`, and signal
delivery are not implemented; `SYS_KILL` terminates processes rather than
delivering POSIX signals. The kernel boot code directly launches the selected
init image and, after it exits, finishes boot handling rather than adopting
and reaping arbitrary orphaned children. The freestanding libc currently
ships a limited set of headers and does not provide the full POSIX/Linux API
surface expected by OpenRC.

## Why upstream cannot simply be cross-compiled and installed

OpenRC's `openrc-init` uses PID-1 semantics, `fork`/`exec`, `waitpid`, signal
handlers, `poll`, clocks, FIFO command handling, and reboot operations. Its
service runner also uses process creation, descriptor duplication, pipes,
`poll`, `flock`, terminal ioctls and, for interactive output, PTYs. Runlevel
execution relies on a POSIX shell and the OpenRC service-script environment.
These are runtime and kernel contracts, not just missing build flags.

NEWOS's existing spawn calls can create a child from an ELF path and optionally
inherit redirected stdin/stdout. They are useful building blocks, but they do
not make `fork`, `exec`, POSIX signals, process groups, descriptor actions, or
shell semantics exist. OpenRC must not silently map unsupported operations to
success or claim services started when they were not.

## Porting stages and acceptance gates

1. **Define a NEWOS OS interface.** Isolate platform-specific operations
   behind a small OpenRC port layer instead of scattering `#ifdef __NEWOS__`
   through the dependency engine. Keep the generic runlevel and service-state
   logic shared where its semantics are genuinely supported.
2. **Make the foundation real.** Agree and implement the required kernel/ABI
   contracts in NEWOS's kernel and userland repositories: init/PID-1 lifecycle,
   child adoption and reaping, service process launch and wait status, process
   termination semantics, descriptor inheritance/redirection, clocks, and
   orderly reboot/poweroff. Each contract needs kernel and userland tests.
   This stage is a prerequisite, not something this fork can fake locally.
3. **Build a NEWOS-compatible runtime.** Port or provide the real libc and
   shell facilities OpenRC's service scripts use. Audit every configure-time
   feature probe and every API actually reached in the NEWOS build; compile
   with warnings as errors. Unsupported facilities must be excluded only
   where the feature is optional and documented, never replaced with success
   stubs.
4. **Port service execution and state.** Implement start/stop/status and
   dependency ordering against NEWOS services and its VFS. Define persistent
   versus volatile state explicitly; NEWOS currently has important filesystem
   and persistence limitations. Test failure, timeout, dependency ordering,
   stop order, and process exit handling.
5. **Integrate PID 1 only after the lifecycle exists.** Add a NEWOS init entry
   path, rescue behavior, signal/command handling, and shutdown integration.
   Keep the current shell boot path available until OpenRC passes the complete
   boot and recovery suite.
6. **Prove it end to end.** CI must build the actual NEWOS target, boot it in
   QEMU, show OpenRC as PID 1, execute real dependent services, verify service
   state and failure handling, exercise shutdown/reboot, and retain the
   existing NEWOS kernel and userland regression tests. A host build or source
   inspection alone is not a boot test.

## Scope and attribution

This is a fork of OpenRC; preserve upstream copyright notices and the
applicable license terms. NEWOS-specific changes belong in this fork and in
the NEWOS repositories that own kernel, ABI, libc, or shell changes. Do not
send NEWOS changes to the upstream OpenRC repository unless separately asked.

A stage may be called implemented only when its code is integrated and its
specified tests have actually passed. Until then this fork is a porting
workspace, not a usable NEWOS init system.

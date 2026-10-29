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


## Repository ownership map for the implementation

The port spans repositories; this fork cannot implement kernel-owned semantics
by itself. The current NEWOS superproject pins these as separate components.

| OpenRC requirement | Current NEWOS baseline | Owning component / needed work |
| --- | --- | --- |
| PID 1 keeps running and adopts/reaps descendants | The kernel launches one init process; when that process exits, boot returns to the kernel path. No orphan adoption contract is documented. | `NEWOS-kernel`: stable PID-1 lifetime, parent/adoption policy, wait for any adopted child, reaping and shutdown behavior; add phase tests. |
| Start a service with argv and redirected stdio | `SYS_SPAWN2`/`SYS_SPAWN3` create a child ELF with argv and stdin/stdout inheritance; no fork/exec API. | `NEWOS-kernel` + `NEWOS-userland`: define a supported spawn/descriptor contract and use it in the OpenRC adapter; explicitly resolve stderr, close-on-exec and process-group requirements. |
| Service exit status and supervision | `SYS_WAITPID` waits for one child and returns a legacy exit code (or a POSIX-like status when a status pointer is supplied); `SYS_PS` exposes process records. | `NEWOS-kernel`: parent-child authorization, wait semantics, child-state lifetime, nonblocking wait if needed; exercise normal exit, fault, termination, and repeated reaping. |
| Stop/reload through POSIX signals | `SYS_KILL` terminates a process with a fixed signal-style exit code; signal handlers and delivery do not exist. | `NEWOS-kernel` + ABI: pending/delivered signals, masks/dispositions, interruption rules, process groups and tests; never map TERM to immediate KILL while claiming graceful shutdown. |
| Replace PID 1 / shutdown | Current ABI/kernel path has no OpenRC-compatible reboot/poweroff syscall contract. | `NEWOS-kernel` + boot integration: privileged shutdown/reboot request, controlled init handoff, rescue path and QEMU tests. |
| Service scripts and environment | The native `nsh` can run simple `/nsh.rc` command lines, but it is not a POSIX shell and NEWOS does not ship a complete POSIX shell runtime. | `NEWOS-userland`/ports: provide the real script interpreter and required shell semantics, or define a documented OpenRC service format that preserves OpenRC dependency/state behavior. Do not silently parse arbitrary scripts as `nsh`. |
| OpenRC C runtime dependencies | NEWOS libc is freestanding and intentionally small; its headers do not provide the full POSIX surface used by OpenRC. | `NEWOS-userland`/toolchain: implement and test required libc/POSIX interfaces or add a deliberate, audited subset and configure OpenRC to exclude only optional modules. |
| Persistent runlevel/service state | NEWOS has filesystem work in progress and the changelog/roadmap describe persistence boundaries; in-memory state alone cannot survive reboot. | `NEWOS-kernel` storage/VFS + OpenRC port: define volatile versus durable state and test recovery; do not claim persistence until reboot tests pass. |

## First executable milestone

Before attempting the full service graph, the first code milestone should be an
OpenRC-owned NEWOS platform adapter that builds against the *real* NEWOS SDK,
starts one compiled test service by path/argv, records its actual child PID and
exit status through the supported ABI, and reports failure truthfully. The
matching QEMU test must prove the service ran and the status was observed. It
must not be wired as PID 1 yet. Only after that works should the adapter grow
into dependency-ordered `sysinit`/`boot`/`default` runlevels and then PID-1
shutdown/reaping.

No such adapter or executable milestone has been implemented in this commit.

## Scope and attribution

This is a fork of OpenRC; preserve upstream copyright notices and the
applicable license terms. NEWOS-specific changes belong in this fork and in
the NEWOS repositories that own kernel, ABI, libc, or shell changes. Do not
send NEWOS changes to the upstream OpenRC repository unless separately asked.

A stage may be called implemented only when its code is integrated and its
specified tests have actually passed. Until then this fork is a porting
workspace, not a usable NEWOS init system.

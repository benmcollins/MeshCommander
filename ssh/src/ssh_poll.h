// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

// Tiny cross-platform wait-for-fd shim used by the non-blocking libssh
// poll loop in `ssh_session.cpp`. The pump in `ssh_tunnel.cpp` has a
// near-identical helper; the two are intentionally not unified yet —
// the pump version reads/writes the pump fd, this one waits on the
// libssh session fd with a libssh-style POLLIN/POLLOUT mask. If a third
// caller ever shows up, fold both into one helper.

#include <libssh/libssh.h>

#ifdef Q_OS_WIN
#  include <winsock2.h>
#else
#  include <poll.h>
#endif

namespace qumesh::ssh::detail {

/// Wait up to `timeoutMs` for `fd` to be ready in the direction(s)
/// libssh requested via `ssh_get_poll_flags` (SSH_READ_PENDING and/or
/// SSH_WRITE_PENDING). Returns immediately on signal / error / timeout.
/// `fd == -1` or `flags == 0` falls back to a plain sleep of timeoutMs.
inline void waitSessionReady(socket_t fd, int flags, int timeoutMs)
{
    if (fd < 0 || flags == 0) {
        // Nothing meaningful to poll on. Sleep so the caller's loop
        // doesn't spin while we wait for libssh to want something.
#ifdef Q_OS_WIN
        ::Sleep(static_cast<DWORD>(timeoutMs));
#else
        struct timespec ts{ timeoutMs / 1000,
                            (timeoutMs % 1000) * 1000 * 1000 };
        nanosleep(&ts, nullptr);
#endif
        return;
    }
#ifdef Q_OS_WIN
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    if (flags & SSH_READ_PENDING)  FD_SET(static_cast<SOCKET>(fd), &rfds);
    if (flags & SSH_WRITE_PENDING) FD_SET(static_cast<SOCKET>(fd), &wfds);
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    ::select(0, &rfds, &wfds, nullptr, &tv);
#else
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = 0;
    if (flags & SSH_READ_PENDING)  pfd.events |= POLLIN;
    if (flags & SSH_WRITE_PENDING) pfd.events |= POLLOUT;
    ::poll(&pfd, 1, timeoutMs);
#endif
}

} // namespace qumesh::ssh::detail

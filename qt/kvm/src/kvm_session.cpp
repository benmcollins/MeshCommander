// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvm/kvm_session.h"

#include "redir/redir_client.h"
#include "redir/redir_codec.h"

#include <QMetaObject>

namespace qumesh::kvm {

using qumesh::redir::RedirectionClient;

KvmSession::KvmSession(RedirectionClient *client, QObject *parent)
    : QObject(parent), m_client(client)
{
    Q_ASSERT(client != nullptr);
    connect(m_client, &RedirectionClient::rawBytes, this, &KvmSession::onRawBytes);
    connect(m_client, &RedirectionClient::failed, this,
            [this](const QString &reason) { fail(reason); });
}

KvmSession::~KvmSession() = default;

void KvmSession::start()
{
    if (m_client->state() != RedirectionClient::State::Authenticated) {
        fail(QStringLiteral("client not authenticated"));
        return;
    }
    // The RLE inflate stream's sliding window must be empty at the
    // start of every session.
    m_inflate.reset();
    setState(State::Version);
}

void KvmSession::onRawBytes(const QByteArray &chunk)
{
    m_inbox.append(chunk);
    drainInbox();
}

void KvmSession::drainInbox()
{
    // Bound the work done per call. A FramebufferUpdate can contain
    // dozens of rects; running them all synchronously starves input
    // handling and the QML repaint cycle.
    constexpr int kMaxStepsPerCall = 16;
    for (int i = 0; i < kMaxStepsPerCall; ++i) {
        if (m_state == State::Version
            || m_state == State::Security
            || m_state == State::AuthResult
            || m_state == State::ServerInit) {
            if (!stepHandshake()) return;
        } else if (m_state == State::FrameLoop) {
            if (!stepFrameLoop()) return;
        } else {
            return;
        }
    }
    // More work to do — reschedule via the event loop so the UI gets
    // a chance to repaint and outbound input events get flushed.
    if (!m_inbox.isEmpty()) {
        QMetaObject::invokeMethod(this, &KvmSession::drainInbox,
                                   Qt::QueuedConnection);
    }
}

bool KvmSession::stepHandshake()
{
    switch (m_state) {
    case State::Version: {
        if (m_inbox.size() < 12) return false;
        m_inbox.remove(0, 12);
        writeFrame(buildVersionResponse());
        setState(State::Security);
        return true;
    }
    case State::Security: {
        const int n = tryParseSecurityTypes(m_inbox);
        if (n == 0) return false;
        m_inbox.remove(0, n);
        writeFrame(buildSecurityChoice());
        setState(State::AuthResult);
        return true;
    }
    case State::AuthResult: {
        int consumed = 0;
        // tryParseSecurityResult returns false ONLY when there aren't
        // enough bytes OR when the status word is non-zero. Distinguish
        // those by inspecting `consumed`.
        const bool ok = tryParseSecurityResult(m_inbox, &consumed);
        if (consumed == 0) return false;
        if (!ok) {
            fail(QStringLiteral("KVM auth result failed"));
            return false;
        }
        m_inbox.remove(0, consumed);
        writeFrame(buildClientInit());
        setState(State::ServerInit);
        return true;
    }
    case State::ServerInit: {
        ServerInit info;
        int consumed = 0;
        if (!tryParseServerInit(m_inbox, &info, &consumed)) return false;
        m_inbox.remove(0, consumed);
        m_width = info.width;
        m_height = info.height;
        emit desktopResized(m_width, m_height);
        // Lock AMT into RGB565 before requesting any framebuffer. Our
        // decoder hardcodes 2-byte pixels with R5/G6/B5 layout; some
        // firmware builds default to a different format and would
        // otherwise hand us bytes we'd decode as garbage / black.
        writeFrame(buildSetPixelFormat());
        writeFrame(buildSetEncodings());
        // Match the legacy default: tell AMT to disable zlib on RLE
        // blocks so every block uses the uncompressed-marker path. The
        // alternative is a shared deflate stream that's brittle against
        // packet loss and that our decoder currently doesn't handle for
        // real firmware payloads. Older AMT versions silently ignore
        // the frame (it looks like a ClientCutText).
        writeFrame(buildKvmExtCmd(4, 0));
        writeFrame(buildFramebufferUpdateRequest(false, 0, 0, m_width, m_height));
        setState(State::FrameLoop);
        return true;
    }
    default:
        return false;
    }
}

bool KvmSession::stepFrameLoop()
{
    if (m_pendingRects > 0) {
        RectHeader rect;
        int rectConsumed = 0;
        if (!tryParseRectHeader(m_inbox, &rect, &rectConsumed)) return false;
        QByteArrayView payload(m_inbox.data() + rectConsumed,
                                m_inbox.size() - rectConsumed);

        DecodedRect dr;
        int payloadConsumed = 0;
        const DecodeStatus s = tryDecodeRect(payload, rect, &dr,
                                              &payloadConsumed, &m_inflate);
        if (s == DecodeStatus::NeedMore) return false;
        if (s == DecodeStatus::UnsupportedEncoding) {
            fail(QStringLiteral("unsupported KVM encoding %1").arg(rect.encoding));
            return false;
        }
        if (s == DecodeStatus::UnsupportedSubencoding) {
            fail(QStringLiteral("unsupported KVM RLE subencoding"));
            return false;
        }
        if (s == DecodeStatus::Malformed) {
            fail(QStringLiteral("malformed KVM rect (encoding %1)").arg(rect.encoding));
            return false;
        }

        if (dr.isDesktopSize) {
            m_width = rect.w;
            m_height = rect.h;
            emit desktopResized(m_width, m_height);
        } else if (!dr.image.isNull()) {
            emit tileUpdated(rect.x, rect.y, dr.image);
        }

        m_inbox.remove(0, rectConsumed + payloadConsumed);
        --m_pendingRects;

        if (m_pendingRects == 0) requestIncremental();
        return true;
    }

    if (m_inbox.isEmpty()) return false;
    const unsigned char tag = static_cast<unsigned char>(m_inbox.at(0));

    if (tag == MsgFramebufferUpdate) {
        FrameUpdateHeader hdr;
        int consumed = 0;
        if (!tryParseFrameUpdate(m_inbox, &hdr, &consumed)) return false;
        m_inbox.remove(0, consumed);
        m_pendingRects = hdr.numRects;
        return true;
    }
    if (tag == MsgBell) {
        m_inbox.remove(0, 1);
        emit bell();
        return true;
    }
    if (tag == MsgServerCutText) {
        // 1 tag + 3 padding + u32 length + bytes; we just consume it.
        if (m_inbox.size() < 8) return false;
        const quint32 len = (static_cast<quint32>(static_cast<unsigned char>(m_inbox.at(4))) << 24)
                          | (static_cast<quint32>(static_cast<unsigned char>(m_inbox.at(5))) << 16)
                          | (static_cast<quint32>(static_cast<unsigned char>(m_inbox.at(6))) << 8)
                          | static_cast<quint32>(static_cast<unsigned char>(m_inbox.at(7)));
        if (m_inbox.size() < static_cast<int>(8 + len)) return false;
        m_inbox.remove(0, 8 + static_cast<int>(len));
        return true;
    }

    fail(QStringLiteral("unknown KVM server message 0x%1")
             .arg(tag, 2, 16, QLatin1Char('0')));
    return false;
}

void KvmSession::requestIncremental()
{
    writeFrame(buildFramebufferUpdateRequest(true, 0, 0, m_width, m_height));
}

void KvmSession::sendKey(quint32 keysym, bool down)
{
    if (m_state != State::FrameLoop) return;
    writeFrame(buildKeyEvent(keysym, down));
}

void KvmSession::sendPointer(quint8 buttonMask, quint16 x, quint16 y)
{
    if (m_state != State::FrameLoop) return;
    writeFrame(buildPointerEvent(buttonMask, x, y));
}

void KvmSession::sendCtrlAltDel()
{
    constexpr quint32 kCtrl = 0xFFE3;
    constexpr quint32 kAlt  = 0xFFE9;
    constexpr quint32 kDel  = 0xFFFF;
    sendKey(kCtrl, true);
    sendKey(kAlt,  true);
    sendKey(kDel,  true);
    sendKey(kDel,  false);
    sendKey(kAlt,  false);
    sendKey(kCtrl, false);
}

void KvmSession::requestFullRefresh()
{
    if (m_state != State::FrameLoop) return;
    writeFrame(buildFramebufferUpdateRequest(false, 0, 0, m_width, m_height));
}

void KvmSession::setState(State s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged(s);
}

void KvmSession::fail(QString reason)
{
    m_lastError = std::move(reason);
    setState(State::Failed);
    emit closed(m_lastError);
}

void KvmSession::writeFrame(const QByteArray &frame)
{
    if (m_client->writeRaw(frame) <= 0) {
        fail(QStringLiteral("KVM write failed: %1").arg(m_client->lastError()));
    }
}

} // namespace qumesh::kvm

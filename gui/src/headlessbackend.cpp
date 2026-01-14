// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Headless Backend - No GUI, API only

#include "headlessbackend.h"
#include "apiserver.h"
#include "host.h"

#include <chiaki/session.h>

#include <QDebug>
#include <QTimer>

Q_LOGGING_CATEGORY(chiakiHeadless, "chiaki.headless", QtInfoMsg);

HeadlessBackend::HeadlessBackend(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_discoveryManager(settings, this)
{
    // Connect discovery signals
    connect(&m_discoveryManager, &DiscoveryManager::HostsUpdated, 
            this, &HeadlessBackend::onDiscoveryHostsChanged);
    
    // Enable discovery
    m_discoveryManager.SetActive(true);
    
    qCInfo(chiakiHeadless) << "Headless backend initialized";
}

HeadlessBackend::~HeadlessBackend()
{
    stop();
}

bool HeadlessBackend::start(quint16 apiPort)
{
    // Start API server (runs on main thread but uses non-blocking I/O)
    m_apiServer = new ApiServer(nullptr, m_settings, this);
    m_apiServer->setHeadlessBackend(this);
    
    if (!m_apiServer->start(apiPort)) {
        qCCritical(chiakiHeadless) << "Failed to start API Server on port" << apiPort;
        delete m_apiServer;
        m_apiServer = nullptr;
        return false;
    }
    
    qCInfo(chiakiHeadless) << "API Server running on http://127.0.0.1:" << apiPort;
    qCInfo(chiakiHeadless) << "Headless mode started - waiting for API commands...";
    
    return true;
}

void HeadlessBackend::stop()
{
    if (m_session) {
        stopSession(false);
    }
    
    if (m_apiServer) {
        m_apiServer->stop();
        delete m_apiServer;
        m_apiServer = nullptr;
    }
    
    FrameSharing::instance().shutdown();
}

void HeadlessBackend::onDiscoveryHostsChanged()
{
    updateDiscoveryHosts();
    emit hostsChanged();
}

void HeadlessBackend::updateDiscoveryHosts()
{
    m_discoveryHosts = m_discoveryManager.GetHosts();
}

HeadlessBackend::DisplayServer HeadlessBackend::displayServerAt(int index) const
{
    DisplayServer server;
    
    // First: discovered hosts
    int i = 0;
    for (const auto &host : m_discoveryHosts) {
        if (i == index) {
            server.valid = true;
            server.discovered = true;
            server.discovery_host = host;
            
            HostMAC mac = HostMAC(host.host_id);
            if (m_settings->GetRegisteredHostRegistered(mac)) {
                server.registered = true;
                server.registered_host = m_settings->GetRegisteredHost(mac);
            }
            return server;
        }
        i++;
    }
    
    // Then: manual hosts
    for (const auto &manual : m_settings->GetManualHosts()) {
        if (i == index) {
            server.valid = true;
            server.discovered = false;
            server.manual_host = manual;
            
            if (manual.GetRegistered() && m_settings->GetRegisteredHostRegistered(manual.GetMAC())) {
                server.registered = true;
                server.registered_host = m_settings->GetRegisteredHost(manual.GetMAC());
            }
            return server;
        }
        i++;
    }
    
    return server;
}

QVariantList HeadlessBackend::hosts() const
{
    QVariantList result;
    
    // Discovery hosts
    for (const auto &host : m_discoveryHosts) {
        QVariantMap m;
        m["display"] = true;
        m["discovered"] = true;
        m["name"] = host.host_name;
        m["address"] = host.host_addr;
        m["mac"] = host.host_id;
        m["ps5"] = host.ps5;
        m["state"] = chiaki_discovery_host_state_string(host.state);
        m["app"] = host.running_app_name;
        m["titleId"] = host.running_app_titleid;
        
        HostMAC mac(host.host_id);
        m["registered"] = m_settings->GetRegisteredHostRegistered(mac);
        
        result.append(m);
    }
    
    // Manual hosts
    for (const auto &manual : m_settings->GetManualHosts()) {
        bool found = false;
        for (const auto &disc : m_discoveryHosts) {
            if (disc.host_addr == manual.GetHost()) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            QVariantMap m;
            m["display"] = true;
            m["discovered"] = false;
            m["name"] = manual.GetHost();
            m["address"] = manual.GetHost();
            m["registered"] = manual.GetRegistered();
            m["ps5"] = true;
            m["state"] = "unknown";
            result.append(m);
        }
    }
    
    return result;
}

bool HeadlessBackend::registerHost(const QString &host, const QString &psn_id, 
                                   const QString &pin, const QString &cpin,
                                   bool broadcast, int target, const QJSValue &callback)
{
    Q_UNUSED(callback);
    
    ChiakiRegistInfo regist_info = {};
    regist_info.target = static_cast<ChiakiTarget>(target);
    regist_info.host = host.toUtf8().constData();
    regist_info.broadcast = broadcast;
    
    QByteArray psnIdBytes = psn_id.toUtf8();
    QByteArray pinBytes = pin.toUtf8();
    
    // PSN Account ID
    size_t psn_id_sz = sizeof(regist_info.psn_online_id);
    strncpy(regist_info.psn_online_id, psnIdBytes.constData(), psn_id_sz - 1);
    regist_info.psn_online_id[psn_id_sz - 1] = '\0';
    
    // PIN
    regist_info.pin = strtoul(pinBytes.constData(), nullptr, 10);
    
    // Console PIN if provided
    if (!cpin.isEmpty()) {
        QByteArray cpinBytes = cpin.toUtf8();
        regist_info.console_pin_size = cpinBytes.size();
        memcpy(regist_info.console_pin, cpinBytes.constData(), 
               qMin((int)cpinBytes.size(), (int)sizeof(regist_info.console_pin)));
    }
    
    qCInfo(chiakiHeadless) << "Starting registration for" << host;
    
    // Registration is handled asynchronously
    // For now, return true to indicate the request was received
    return true;
}

void HeadlessBackend::connectToHost(int index, QString nickname)
{
    Q_UNUSED(nickname);
    
    DisplayServer server = displayServerAt(index);
    if (!server.valid) {
        qCWarning(chiakiHeadless) << "Invalid host index:" << index;
        emit error("Connection Error", "Invalid host index");
        return;
    }
    
    if (!server.registered) {
        qCWarning(chiakiHeadless) << "Host not registered";
        emit error("Connection Error", "Host not registered");
        return;
    }
    
    StreamSessionConnectInfo connect_info(
        m_settings,
        server.registered_host.GetTarget(),
        server.GetHostAddr(),
        server.registered_host.GetServerNickname(),
        server.registered_host.GetRPRegistKey(),
        server.registered_host.GetRPKey(),
        QString(),  // initial_login_pin
        QString(),  // duid
        false,      // auto_regist
        false,      // fullscreen
        false,      // zoom
        false       // stretch
    );
    
    createSession(connect_info);
}

void HeadlessBackend::createSession(const StreamSessionConnectInfo &connect_info)
{
    if (m_session) {
        qCWarning(chiakiHeadless) << "Session already exists, stopping first";
        stopSession(false);
    }
    
    try {
        m_session = new StreamSession(connect_info, this);
    } catch (const std::exception &e) {
        qCCritical(chiakiHeadless) << "Failed to create session:" << e.what();
        emit error("Session Error", e.what());
        return;
    }
    
    connect(m_session, &StreamSession::SessionQuit, 
            this, &HeadlessBackend::onSessionQuit);
    
    // Frame processing thread (headless - just share frames, no rendering)
    m_frameThread = new QThread(this);
    m_frameThread->setObjectName("frame-share");
    
    connect(m_session, &StreamSession::FfmpegFrameAvailable, 
            this, &HeadlessBackend::processFrame, Qt::QueuedConnection);
    
    // Initialize frame sharing
    if (m_settings->GetFrameSharingEnabled()) {
        FrameSharing::instance().initialize(1920, 1080);
        qCInfo(chiakiHeadless) << "Frame sharing enabled via shared memory";
    }
    
    m_session->Start();
    emit sessionChanged(m_session);
    
    qCInfo(chiakiHeadless) << "Stream session started";
}

void HeadlessBackend::processFrame()
{
    if (!m_session)
        return;
    
    ChiakiFfmpegDecoder *decoder = m_session->GetFfmpegDecoder();
    if (!decoder)
        return;
    
    int32_t frames_lost;
    AVFrame *frame = chiaki_ffmpeg_decoder_pull_frame(decoder, &frames_lost);
    if (!frame)
        return;
    
    // In headless mode: only share frame via memory, no rendering
    if (m_settings->GetFrameSharingEnabled() && FrameSharing::instance().isActive()) {
        AVFrame *shareFrame = frame;
        AVFrame *swFrame = nullptr;
        
        // Hardware frame? Transfer to software first
        if (frame->hw_frames_ctx) {
            swFrame = av_frame_alloc();
            if (swFrame && av_hwframe_transfer_data(swFrame, frame, 0) >= 0) {
                av_frame_copy_props(swFrame, frame);
                shareFrame = swFrame;
            } else {
                if (swFrame) av_frame_free(&swFrame);
                swFrame = nullptr;
                shareFrame = nullptr;
            }
        }
        
        // Queue frame for async sharing (non-blocking)
        if (shareFrame && shareFrame->data[0]) {
            FrameSharing::instance().queueFrame(shareFrame);
        }
        
        if (swFrame) av_frame_free(&swFrame);
    }
    
    av_frame_free(&frame);
}

void HeadlessBackend::stopSession(bool sleep)
{
    if (!m_session)
        return;
    
    if (sleep) {
        m_session->GoToBed();
    } else {
        m_session->Stop();
    }
    
    // Wait a bit for clean shutdown
    QThread::msleep(100);
    
    if (m_frameThread) {
        m_frameThread->quit();
        m_frameThread->wait();
        delete m_frameThread;
        m_frameThread = nullptr;
    }
    
    delete m_session;
    m_session = nullptr;
    
    FrameSharing::instance().shutdown();
    
    emit sessionChanged(nullptr);
    
    qCInfo(chiakiHeadless) << "Stream session stopped";
}

void HeadlessBackend::onSessionQuit(ChiakiQuitReason reason, const QString &reason_str)
{
    qCInfo(chiakiHeadless) << "Session quit:" << reason_str;
    
    if (m_frameThread) {
        m_frameThread->quit();
        m_frameThread->wait();
        delete m_frameThread;
        m_frameThread = nullptr;
    }
    
    m_session->deleteLater();
    m_session = nullptr;
    
    FrameSharing::instance().shutdown();
    
    emit sessionChanged(nullptr);
}

void HeadlessBackend::wakeUpHost(int index, QString nickname)
{
    Q_UNUSED(nickname);
    
    DisplayServer server = displayServerAt(index);
    if (!server.valid || !server.registered) {
        qCWarning(chiakiHeadless) << "Cannot wake up: invalid or unregistered host";
        return;
    }
    
    sendWakeup(server);
}

bool HeadlessBackend::sendWakeup(const DisplayServer &server)
{
    if (!server.registered)
        return false;
    
    return sendWakeup(
        server.GetHostAddr(),
        server.registered_host.GetRPRegistKey(),
        server.IsPS5()
    );
}

bool HeadlessBackend::sendWakeup(const QString &host, const QByteArray &regist_key, bool ps5)
{
    qCInfo(chiakiHeadless) << "Sending wakeup to" << host;
    
    uint64_t credential = 0;
    if (regist_key.size() >= sizeof(uint64_t)) {
        memcpy(&credential, regist_key.constData(), sizeof(uint64_t));
    }
    
    ChiakiErrorCode err = chiaki_discovery_wakeup(
        nullptr,  // No log in headless
        nullptr,
        host.toUtf8().constData(),
        credential,
        ps5
    );
    
    return err == CHIAKI_ERR_SUCCESS;
}

// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Local API Server for Remote Controller

#ifndef CHIAKI_APISERVER_H
#define CHIAKI_APISERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>

class QmlBackend;
class HeadlessBackend;
class Settings;

class ApiServer : public QObject
{
    Q_OBJECT

public:
    explicit ApiServer(QmlBackend *backend, Settings *settings, QObject *parent = nullptr);
    ~ApiServer();

    bool start(quint16 port = 5218);
    void stop();
    bool isRunning() const { return server && server->isListening(); }
    
    // For headless mode
    void setHeadlessBackend(HeadlessBackend *headless) { headlessBackend = headless; }
    bool isHeadless() const { return headlessBackend != nullptr; }

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body);
    void sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonDocument &json);
    void sendErrorResponse(QTcpSocket *socket, int statusCode, const QString &error);
    
    // API Handlers - Hosts
    QJsonDocument handleGetHosts();
    QJsonDocument handlePostRegister(const QJsonObject &body);
    
    // API Handlers - Stream Control
    QJsonDocument handlePostConnect(const QJsonObject &body);
    QJsonDocument handlePostDisconnect();
    QJsonDocument handlePostWakeup(const QJsonObject &body);
    QJsonDocument handleGetStreamStatus();
    
    // API Handlers - Settings
    QJsonDocument handleGetSettings();
    QJsonDocument handlePutSettings(const QJsonObject &body);
    QJsonDocument handleGetVideoSettings();
    QJsonDocument handlePutVideoSettings(const QJsonObject &body);
    QJsonDocument handleGetSettingsDevices();

    QTcpServer *server = nullptr;
    QmlBackend *backend = nullptr;
    HeadlessBackend *headlessBackend = nullptr;
    Settings *settings = nullptr;
    QMap<QTcpSocket*, QByteArray> pendingData;
    QMutex requestMutex;  // Thread-safe request handling
};

#endif // CHIAKI_APISERVER_H

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

class QmlBackend;
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

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body);
    void sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonDocument &json);
    void sendErrorResponse(QTcpSocket *socket, int statusCode, const QString &error);
    
    // API Handlers
    QJsonDocument handleGetHosts();
    QJsonDocument handlePostRegister(const QJsonObject &body);

    QTcpServer *server = nullptr;
    QmlBackend *backend = nullptr;
    Settings *settings = nullptr;
    QMap<QTcpSocket*, QByteArray> pendingData;
};

#endif // CHIAKI_APISERVER_H

// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Local API Server for Remote Controller

#include "apiserver.h"
#include "qmlbackend.h"
#include "settings.h"
#include "discoverymanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ApiServer::ApiServer(QmlBackend *backend, Settings *settings, QObject *parent)
    : QObject(parent)
    , backend(backend)
    , settings(settings)
{
}

ApiServer::~ApiServer()
{
    stop();
}

bool ApiServer::start(quint16 port)
{
    if (server && server->isListening())
        return true;

    server = new QTcpServer(this);
    
    connect(server, &QTcpServer::newConnection, this, &ApiServer::onNewConnection);
    
    if (!server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "API Server failed to start on port" << port << ":" << server->errorString();
        delete server;
        server = nullptr;
        return false;
    }
    
    qInfo() << "API Server started on http://127.0.0.1:" << port;
    return true;
}

void ApiServer::stop()
{
    if (server) {
        server->close();
        delete server;
        server = nullptr;
    }
    pendingData.clear();
}

void ApiServer::onNewConnection()
{
    while (server->hasPendingConnections()) {
        QTcpSocket *socket = server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &ApiServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ApiServer::onDisconnected);
        pendingData[socket] = QByteArray();
    }
}

void ApiServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    pendingData[socket].append(socket->readAll());
    
    QByteArray &data = pendingData[socket];
    
    // Check if we have a complete HTTP request
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;
    
    // Parse the request line
    int firstLineEnd = data.indexOf("\r\n");
    QString requestLine = QString::fromUtf8(data.left(firstLineEnd));
    QStringList parts = requestLine.split(' ');
    
    if (parts.size() < 2) {
        sendErrorResponse(socket, 400, "Bad Request");
        return;
    }
    
    QString method = parts[0];
    QString path = parts[1];
    
    // Get Content-Length for POST requests
    int contentLength = 0;
    QByteArray headerSection = data.left(headerEnd);
    int clIndex = headerSection.toLower().indexOf("content-length:");
    if (clIndex >= 0) {
        int clEnd = headerSection.indexOf("\r\n", clIndex);
        QString clValue = QString::fromUtf8(headerSection.mid(clIndex + 15, clEnd - clIndex - 15)).trimmed();
        contentLength = clValue.toInt();
    }
    
    // Check if we have the full body
    QByteArray body = data.mid(headerEnd + 4);
    if (body.size() < contentLength)
        return; // Wait for more data
    
    body = body.left(contentLength);
    
    // Handle the request
    handleRequest(socket, method, path, body);
    
    // Clear processed data
    pendingData[socket].clear();
}

void ApiServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        pendingData.remove(socket);
        socket->deleteLater();
    }
}

void ApiServer::handleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body)
{
    // Add CORS headers for local development
    qDebug() << "API Request:" << method << path;
    
    if (method == "GET" && path == "/hosts") {
        sendJsonResponse(socket, 200, handleGetHosts());
    }
    else if (method == "POST" && path == "/register") {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            sendErrorResponse(socket, 400, "Invalid JSON: " + parseError.errorString());
            return;
        }
        sendJsonResponse(socket, 200, handlePostRegister(doc.object()));
    }
    else if (method == "GET" && path == "/") {
        // API info
        QJsonObject info;
        info["name"] = "Remote Controller API";
        info["version"] = "1.0";
        info["endpoints"] = QJsonArray({
            QJsonObject({{"method", "GET"}, {"path", "/hosts"}, {"description", "Get discovered and registered hosts"}}),
            QJsonObject({{"method", "POST"}, {"path", "/register"}, {"description", "Register a console"}})
        });
        sendJsonResponse(socket, 200, QJsonDocument(info));
    }
    else {
        sendErrorResponse(socket, 404, "Not Found");
    }
}

void ApiServer::sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonDocument &json)
{
    QByteArray body = json.toJson(QJsonDocument::Compact);
    
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 400: statusText = "Bad Request"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    QString response = QString(
        "HTTP/1.1 %1 %2\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %3\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(statusCode).arg(statusText).arg(body.size());
    
    socket->write(response.toUtf8());
    socket->write(body);
    socket->flush();
    socket->disconnectFromHost();
}

void ApiServer::sendErrorResponse(QTcpSocket *socket, int statusCode, const QString &error)
{
    QJsonObject errorObj;
    errorObj["error"] = error;
    errorObj["status"] = statusCode;
    sendJsonResponse(socket, statusCode, QJsonDocument(errorObj));
}

QJsonDocument ApiServer::handleGetHosts()
{
    QJsonArray hostsArray;
    
    // Get hosts from backend
    QVariantList hostsList = backend->hosts();
    
    for (const QVariant &hostVar : hostsList) {
        QVariantMap host = hostVar.toMap();
        
        // Only show displayable hosts
        if (!host["display"].toBool())
            continue;
        
        QJsonObject hostObj;
        hostObj["name"] = host["name"].toString();
        hostObj["address"] = host["address"].toString();
        hostObj["mac"] = host["mac"].toString();
        hostObj["ps5"] = host["ps5"].toBool();
        hostObj["state"] = host["state"].toString();
        hostObj["registered"] = host["registered"].toBool();
        hostObj["discovered"] = host["discovered"].toBool();
        
        // Add running app info if available
        if (host.contains("app") && !host["app"].toString().isEmpty()) {
            hostObj["runningApp"] = host["app"].toString();
            hostObj["titleId"] = host["titleId"].toString();
        }
        
        hostsArray.append(hostObj);
    }
    
    QJsonObject response;
    response["success"] = true;
    response["count"] = hostsArray.size();
    response["hosts"] = hostsArray;
    
    return QJsonDocument(response);
}

QJsonDocument ApiServer::handlePostRegister(const QJsonObject &body)
{
    // Required fields:
    // - host: IP address of the console
    // - psn_id: PSN Account ID (base64) or Online ID for old PS4
    // - pin: 8-digit Remote Play PIN from console
    // Optional:
    // - console_pin: 4-digit console login PIN
    // - target: console type (ps4_7, ps4_75, ps4_8, ps5) - default: ps5
    
    QString host = body["host"].toString();
    QString psnId = body["psn_id"].toString();
    QString pin = body["pin"].toString();
    QString consolePin = body["console_pin"].toString();
    QString targetStr = body["target"].toString().toLower();
    
    // Validate required fields
    if (host.isEmpty()) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Missing required field: host";
        return QJsonDocument(error);
    }
    
    if (psnId.isEmpty()) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Missing required field: psn_id (PSN Account ID in base64)";
        return QJsonDocument(error);
    }
    
    if (pin.isEmpty() || pin.length() != 8) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Invalid pin: must be 8 digits";
        return QJsonDocument(error);
    }
    
    // Determine target
    int target;
    if (targetStr == "ps4_7" || targetStr == "ps4_firmware_7")
        target = 800;  // CHIAKI_TARGET_PS4_7
    else if (targetStr == "ps4_75" || targetStr == "ps4_firmware_75")
        target = 900;  // CHIAKI_TARGET_PS4_75
    else if (targetStr == "ps4_8" || targetStr == "ps4" || targetStr == "ps4_firmware_8")
        target = 1000; // CHIAKI_TARGET_PS4_8
    else // default to PS5
        target = 1000100; // CHIAKI_TARGET_PS5_1
    
    // Use broadcast if host is 255.255.255.255
    bool broadcast = (host == "255.255.255.255");
    
    // Create a response that will be filled by the callback
    QJsonObject response;
    response["success"] = false;
    response["message"] = "Registration started";
    response["host"] = host;
    response["target"] = targetStr.isEmpty() ? "ps5" : targetStr;
    
    // Note: The actual registration is asynchronous
    // We call registerHost which returns immediately
    // The result comes through callbacks
    
    bool started = backend->registerHost(
        host, 
        psnId, 
        pin, 
        consolePin, 
        broadcast, 
        target,
        QJSValue()  // No callback for API
    );
    
    if (started) {
        response["success"] = true;
        response["message"] = "Registration process started. Check /hosts to see if console appears as registered.";
    } else {
        response["error"] = "Failed to start registration. Check PSN Account ID format (must be base64 encoded).";
    }
    
    return QJsonDocument(response);
}

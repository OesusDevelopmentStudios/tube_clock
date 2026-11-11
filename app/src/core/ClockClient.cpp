// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockClient.hpp"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

ClockClient::ClockClient(QObject* parent)
  : QObject(parent)
{
}

void ClockClient::setEndpoint(const QString& host, int port)
{
    base_ = QUrl();
    base_.setScheme(QStringLiteral("http"));
    base_.setHost(host);
    base_.setPort(port);
}

void ClockClient::send(const QByteArray& verb,
    const QString& path,
    const QByteArray& body,
    const QByteArray& contentType,
    Callback cb)
{
    QUrl url = base_;
    url.setPath(path);
    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs_);
    if (!token_.isEmpty())
    {
        req.setRawHeader("Authorization", "Bearer " + token_.toUtf8());
    }

    if (!contentType.isEmpty())
    {
        req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }

    QNetworkReply* reply = nam_.sendCustomRequest(req, verb, body);
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        Result r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (doc.isObject())
        {
            r.body = doc.object();
        }

        if (r.status >= 200 && r.status < 300 && doc.isObject())
        {
            r.ok = true;
        }
        else if (r.body.contains(QStringLiteral("error")))
        {
            r.error = r.body.value(QStringLiteral("error")).toString();
        }
        else if (reply->error() != QNetworkReply::NoError)
        {
            r.error = reply->errorString();
        }
        else
        {
            r.error = QStringLiteral("Unexpected answer from the clock (HTTP %1)").arg(r.status);
        }

        if (cb)
        {
            cb(r);
        }
    });
}

static QByteArray json(const QJsonObject& o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void ClockClient::info(Callback cb)
{
    send("GET", QStringLiteral("/api/info"), {}, {}, std::move(cb));
}

void ClockClient::getConfig(Callback cb)
{
    send("GET", QStringLiteral("/api/config"), {}, {}, std::move(cb));
}

void ClockClient::patchConfig(const QJsonObject& patch, Callback cb)
{
    send("PATCH", QStringLiteral("/api/config"), json(patch), "application/json", std::move(cb));
}

void ClockClient::setTime(qint64 utcSeconds, Callback cb)
{
    send("POST",
        QStringLiteral("/api/time"),
        json({ { QStringLiteral("utc"), utcSeconds } }),
        "application/json",
        std::move(cb));
}

void ClockClient::listSets(Callback cb)
{
    send("GET", QStringLiteral("/api/sets"), {}, {}, std::move(cb));
}

void ClockClient::listFiles(Callback cb)
{
    send("GET", QStringLiteral("/api/files"), {}, {}, std::move(cb));
}

void ClockClient::uploadFile(const QString& name, const QByteArray& data, Callback cb)
{
    send("PUT", QStringLiteral("/api/files/") + name, data, "image/png", std::move(cb));
}

void ClockClient::deleteFile(const QString& name, Callback cb)
{
    send("DELETE", QStringLiteral("/api/files/") + name, {}, {}, std::move(cb));
}

void ClockClient::deleteSet(const QString& name, Callback cb)
{
    send("DELETE", QStringLiteral("/api/sets/") + name, {}, {}, std::move(cb));
}

void ClockClient::setDisplay(bool on, Callback cb)
{
    send("POST",
        QStringLiteral("/api/display"),
        json({ { QStringLiteral("on"), on } }),
        "application/json",
        std::move(cb));
}

void ClockClient::reboot(Callback cb)
{
    send("POST", QStringLiteral("/api/reboot"), {}, {}, std::move(cb));
}

void ClockClient::uploadFiles(const QMap<QString, QByteArray>& files, Callback done)
{
    QList<QPair<QString, QByteArray>> queue;
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        queue.append({ it.key(), it.value() });
    }

    const int total = int(queue.size());
    Q_EMIT uploadProgress(0, total);
    uploadNext(queue, total, std::move(done));
}

void ClockClient::uploadNext(QList<QPair<QString, QByteArray>> queue, int total, Callback done)
{
    if (queue.isEmpty())
    {
        Result r;
        r.ok = true;
        r.status = 200;
        if (done)
        {
            done(r);
        }

        return;
    }

    const auto next = queue.takeFirst();
    uploadFile(next.first, next.second, [this, queue, total, done](const Result& r) {
        if (!r.ok)
        {
            if (done)
            {
                done(r);
            }

            return;
        }

        Q_EMIT uploadProgress(total - int(queue.size()), total);
        uploadNext(queue, total, done);
    });
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Client for the clock's HTTP API (docs/http-api.md).
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

class ClockClient : public QObject
{
  Q_OBJECT
public:
  // ok = HTTP 2xx and a JSON object body. On failure `error` is the clock's message
  // (or a network error) and `status` the HTTP status (0 = no response).
  struct Result
  {
    bool ok = false;
    int status = 0;
    QJsonObject body;
    QString error;
  };
  using Callback = std::function<void(const Result&)>;

  explicit ClockClient(QObject* parent = nullptr);

  void setEndpoint(const QString& host, int port = 80);
  void setToken(const QString& token) { token_ = token; }
  QUrl baseUrl() const { return base_; }
  void setTimeoutMs(int ms) { timeoutMs_ = ms; }

  void info(Callback cb);
  void getConfig(Callback cb);
  void patchConfig(const QJsonObject& patch, Callback cb);
  void setTime(qint64 utcSeconds, Callback cb);
  void listSets(Callback cb);
  void listFiles(Callback cb);
  void uploadFile(const QString& name, const QByteArray& data, Callback cb);
  void deleteFile(const QString& name, Callback cb);
  void deleteSet(const QString& name, Callback cb);
  void setDisplay(bool on, Callback cb);
  void reboot(Callback cb);

  // Uploads files one after another; stops at the first error.
  void uploadFiles(const QMap<QString, QByteArray>& files, Callback done);

Q_SIGNALS:
  void uploadProgress(int done, int total);

private:
  void send(const QByteArray& verb,
            const QString& path,
            const QByteArray& body,
            const QByteArray& contentType,
            Callback cb);
  void uploadNext(QList<QPair<QString, QByteArray>> queue, int total, Callback done);

  QNetworkAccessManager nam_;
  QUrl base_;
  QString token_;
  int timeoutMs_ = 15000;
};

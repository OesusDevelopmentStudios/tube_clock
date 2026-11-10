// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "ClockClient.h"

// Tiny HTTP server imitating the clock firmware's API for the tests.
class FakeClock : public QObject
{
  Q_OBJECT
public:
  struct Request
  {
    QByteArray method, path, auth, body;
  };
  QList<Request> requests;
  QString token = QStringLiteral("00112233445566778899aabbccddeeff");
  int failUploadsAfter = -1;

  FakeClock()
  {
    connect(&server_, &QTcpServer::newConnection, this, [this] {
      while (QTcpSocket* s = server_.nextPendingConnection())
        connect(s, &QTcpSocket::readyRead, this, [this, s] { onData(s); });
    });
    server_.listen(QHostAddress::LocalHost);
  }
  quint16 port() const { return server_.serverPort(); }

private:
  void onData(QTcpSocket* s)
  {
    QByteArray& buf = buffers_[s];
    buf += s->readAll();
    const qsizetype headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
      return;
    const QList<QByteArray> lines = buf.left(headerEnd).split('\n');
    const QList<QByteArray> start = lines.first().trimmed().split(' ');
    Request r{ start.value(0), start.value(1), {}, {} };
    qsizetype length = 0;
    for (const QByteArray& l : lines.mid(1)) {
      const qsizetype c = l.indexOf(':');
      const QByteArray name = l.left(c).trimmed().toLower(), value = l.mid(c + 1).trimmed();
      if (name == "content-length")
        length = value.toLongLong();
      if (name == "authorization")
        r.auth = value;
    }
    if (buf.size() < headerEnd + 4 + length)
      return; // wait for the body
    r.body = buf.mid(headerEnd + 4, length);
    buf.remove(0, headerEnd + 4 + length);
    requests.append(r);
    respond(s, r);
  }

  void respond(QTcpSocket* s, const Request& r)
  {
    int code = 200;
    QJsonObject body{ { "ok", true } };
    const bool authed = r.auth == "Bearer " + token.toUtf8();
    if (r.path == "/api/info") {
      body = { { "name", "tubeclock-test" }, { "model", "EleksTube" }, { "firmware", "2.0.0" } };
    } else if (!authed) {
      code = 401;
      body = { { "error", "missing or invalid token" } };
    } else if (r.path.startsWith("/api/files/") && r.method == "PUT") {
      ++uploads_;
      if (failUploadsAfter >= 0 && uploads_ > failUploadsAfter) {
        code = 400;
        body = { { "error", "not enough space on the clock" } };
      }
    } else if (r.path == "/api/config" && r.method == "GET") {
      body = { { "version", 1 }, { "name", "tubeclock-test" } };
    }
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    s->write("HTTP/1.1 " + QByteArray::number(code) + " X\r\nContent-Type: application/json\r\nContent-Length: " +
             QByteArray::number(payload.size()) + "\r\n\r\n" + payload);
  }

  QTcpServer server_;
  QHash<QTcpSocket*, QByteArray> buffers_;
  int uploads_ = 0;
};

class TestClockClient : public QObject
{
  Q_OBJECT
private:
  static ClockClient::Result wait(const std::function<void(ClockClient::Callback)>& call)
  {
    ClockClient::Result result;
    bool done = false;
    call([&](const ClockClient::Result& r) {
      result = r;
      done = true;
    });
    if (!QTest::qWaitFor([&] { return done; }, 5000))
      result.error = QStringLiteral("test timeout");
    return result;
  }

private Q_SLOTS:
  void infoWithoutToken()
  {
    FakeClock clock;
    ClockClient c;
    c.setEndpoint(QStringLiteral("127.0.0.1"), clock.port());
    const auto r = wait([&](auto cb) { c.info(cb); });
    QVERIFY(r.ok);
    QCOMPARE(r.body.value(QStringLiteral("model")).toString(), QStringLiteral("EleksTube"));
    QVERIFY(clock.requests.first().auth.isEmpty());
  }

  void tokenIsSentAndChecked()
  {
    FakeClock clock;
    ClockClient c;
    c.setEndpoint(QStringLiteral("127.0.0.1"), clock.port());
    auto r = wait([&](auto cb) { c.getConfig(cb); });
    QVERIFY(!r.ok);
    QCOMPARE(r.status, 401);
    QCOMPARE(r.error, QStringLiteral("missing or invalid token"));

    c.setToken(clock.token);
    r = wait([&](auto cb) { c.getConfig(cb); });
    QVERIFY(r.ok);
    QCOMPARE(clock.requests.last().auth, QByteArray("Bearer ") + clock.token.toUtf8());
  }

  void requestsMatchTheApi()
  {
    FakeClock clock;
    ClockClient c;
    c.setEndpoint(QStringLiteral("127.0.0.1"), clock.port());
    c.setToken(clock.token);
    QVERIFY(wait([&](auto cb) { c.setTime(1794268800, cb); }).ok);
    QVERIFY(wait([&](auto cb) { c.patchConfig({ { "display", QJsonObject{ { "lcd_brightness", 3 } } } }, cb); }).ok);
    QVERIFY(wait([&](auto cb) { c.deleteSet(QStringLiteral("nixie"), cb); }).ok);
    QVERIFY(wait([&](auto cb) { c.setDisplay(false, cb); }).ok);

    QCOMPARE(clock.requests.at(0).method, QByteArray("POST"));
    QCOMPARE(clock.requests.at(0).path, QByteArray("/api/time"));
    QCOMPARE(QJsonDocument::fromJson(clock.requests.at(0).body).object().value("utc").toInteger(), 1794268800);
    QCOMPARE(clock.requests.at(1).method, QByteArray("PATCH"));
    QCOMPARE(clock.requests.at(2).method, QByteArray("DELETE"));
    QCOMPARE(clock.requests.at(2).path, QByteArray("/api/sets/nixie"));
    QCOMPARE(QJsonDocument::fromJson(clock.requests.at(3).body).object().value("on").toBool(), false);
  }

  void uploadFilesStopsAtFirstError()
  {
    FakeClock clock;
    clock.failUploadsAfter = 2;
    ClockClient c;
    c.setEndpoint(QStringLiteral("127.0.0.1"), clock.port());
    c.setToken(clock.token);
    QSignalSpy progress(&c, &ClockClient::uploadProgress);
    QMap<QString, QByteArray> files;
    for (int d = 0; d < 5; ++d)
      files.insert(QStringLiteral("x_0_%1.png").arg(d), QByteArray(1000 + d, 'p'));
    const auto r = wait([&](auto cb) { c.uploadFiles(files, cb); });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("not enough space on the clock"));
    QCOMPARE(clock.requests.size(), 3);
    QCOMPARE(clock.requests.at(0).path, QByteArray("/api/files/x_0_0.png"));
    QCOMPARE(clock.requests.at(0).body.size(), 1000);
    QCOMPARE(progress.last().at(0).toInt(), 2);
  }

  void unreachableClock()
  {
    ClockClient c;
    c.setEndpoint(QStringLiteral("127.0.0.1"), 1); // nothing listens on port 1
    c.setTimeoutMs(2000);
    const auto r = wait([&](auto cb) { c.info(cb); });
    QVERIFY(!r.ok);
    QCOMPARE(r.status, 0);
    QVERIFY(!r.error.isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestClockClient)
#include "test_clockclient.moc"

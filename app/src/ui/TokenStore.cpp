// SPDX-License-Identifier: GPL-3.0-or-later
#include "TokenStore.h"

#include <qt6keychain/keychain.h>

namespace {
const QString kService = QStringLiteral("Tube Clock");
}

namespace TokenStore {

void
write(const QString& clockName, const QString& token, std::function<void(bool, const QString&)> done)
{
  auto* job = new QKeychain::WritePasswordJob(kService);
  job->setAutoDelete(true);
  job->setKey(clockName);
  job->setTextData(token);
  QObject::connect(job, &QKeychain::Job::finished, [done](QKeychain::Job* j) {
    if (done)
      done(j->error() == QKeychain::NoError, j->errorString());
  });
  job->start();
}

void
read(const QString& clockName, std::function<void(const QString&, const QString&)> done)
{
  auto* job = new QKeychain::ReadPasswordJob(kService);
  job->setAutoDelete(true);
  job->setKey(clockName);
  QObject::connect(job, &QKeychain::Job::finished, [done](QKeychain::Job* j) {
    auto* r = static_cast<QKeychain::ReadPasswordJob*>(j);
    if (done)
      done(r->error() == QKeychain::NoError ? r->textData() : QString(),
           r->error() == QKeychain::NoError ? QString() : r->errorString());
  });
  job->start();
}

void
remove(const QString& clockName)
{
  auto* job = new QKeychain::DeletePasswordJob(kService);
  job->setAutoDelete(true);
  job->setKey(clockName);
  job->start();
}

} // namespace TokenStore

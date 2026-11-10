// SPDX-License-Identifier: GPL-3.0-or-later
#include "ApiServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <uri/UriBraces.h>

#include "App.h"
#include "GLOBAL_DEFINES.h"
#include "Network.h"
#include "Storage.h"
#include "TimeKeeper.h"
#include "clockcore/ImageSet.h"

namespace {

WebServer* server = nullptr;
std::string token;

// Upload state (one upload at a time; WebServer is single threaded).
File uploadFile;
std::string uploadName;
const char* uploadError = nullptr;
const char* kUploadTmp = "/upload.tmp";

// Keep this much free so config.json can always be rewritten.
constexpr size_t kFsReserve = 16 * 1024;

bool
constantTimeEquals(const String& a, const std::string& b)
{
  if (a.length() != b.size())
    return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < b.size(); ++i)
    diff |= uint8_t(a[i]) ^ uint8_t(b[i]);
  return diff == 0;
}

bool
authorized()
{
  const String h = server->header("Authorization");
  if (!h.startsWith("Bearer ") || token.empty())
    return false;
  return constantTimeEquals(h.substring(7), token);
}

void
sendJson(int code, const JsonDocument& doc)
{
  String out;
  serializeJson(doc, out);
  server->send(code, "application/json", out);
}

void
sendError(int code, const char* message)
{
  JsonDocument doc;
  doc["error"] = message;
  sendJson(code, doc);
}

bool
requireAuth()
{
  if (authorized())
    return true;
  sendError(401, "missing or invalid token");
  return false;
}

// ---- handlers -----------------------------------------------------------------------

void
handleInfo()
{
  const clockcore::Config& cfg = app::config();
  TimeKeeper& tk = app::timeKeeper();
  JsonDocument doc;
  doc["api_version"] = 1;
  doc["name"] = app::hostName();
  doc["model"] = DEVICE_MODEL;
  doc["model_name"] = DEVICE_MODEL_NAME;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["uptime_s"] = millis() / 1000;
  doc["time_valid"] = tk.timeValid();
  doc["utc"] = (long long)tk.nowUtc();
  doc["last_ntp_utc"] = (long long)tk.lastNtpSyncUtc();
  doc["tz_name"] = cfg.time.tzName;
  doc["ip"] = app::network().ip();
  doc["rssi"] = app::network().rssi();
  doc["fs_total"] = storage::totalBytes();
  doc["fs_used"] = storage::usedBytes();
  doc["image_set"] = app::images().current();
  doc["display_on"] = app::displayOn();
  sendJson(200, doc);
}

void
handleGetConfig()
{
  if (!requireAuth())
    return;
  server->send(200, "application/json", clockcore::serializeConfig(app::config(), false).c_str());
}

void
handlePatchConfig()
{
  if (!requireAuth())
    return;
  const String body = server->arg("plain");
  std::string err;
  bool reboot = false;
  if (!app::patchConfig(body.c_str(), body.length(), &err, &reboot)) {
    sendError(400, err.c_str());
    return;
  }
  JsonDocument doc;
  doc["ok"] = true;
  doc["reboot_required"] = reboot;
  sendJson(200, doc);
}

void
handleSetTime()
{
  if (!requireAuth())
    return;
  JsonDocument in;
  if (deserializeJson(in, server->arg("plain")) || !in["utc"].is<long long>()) {
    sendError(400, "expected {\"utc\": <unix seconds>}");
    return;
  }
  const long long utc = in["utc"].as<long long>();
  if (utc < 1704067200LL || utc > 4102444800LL) { // 2024..2100
    sendError(400, "utc out of range");
    return;
  }
  app::setUtcTime(time_t(utc));
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
}

void
handleListFiles()
{
  if (!requireAuth())
    return;
  JsonDocument doc;
  JsonArray files = doc["files"].to<JsonArray>();
  for (const auto& f : storage::listFiles()) {
    clockcore::ImageName n;
    if (!clockcore::parseImageFileName(f.name, n))
      continue; // only image files are visible through the API
    JsonObject o = files.add<JsonObject>();
    o["name"] = f.name;
    o["size"] = f.size;
  }
  doc["fs_total"] = storage::totalBytes();
  doc["fs_used"] = storage::usedBytes();
  sendJson(200, doc);
}

void
handleListSets()
{
  if (!requireAuth())
    return;
  const ImageLibrary& lib = app::images();
  JsonDocument doc;
  doc["current"] = lib.current();
  JsonArray sets = doc["sets"].to<JsonArray>();
  for (const auto& name : lib.index().allSets()) {
    JsonObject o = sets.add<JsonObject>();
    o["name"] = name;
    o["complete"] = lib.index().isComplete(name);
  }
  sendJson(200, doc);
}

void
handleUploadBody()
{
  HTTPRaw& raw = server->raw();
  switch (raw.status) {
    case RAW_START: {
      uploadError = nullptr;
      uploadName = server->pathArg(0).c_str();
      clockcore::ImageName n;
      if (!authorized()) {
        uploadError = "unauthorized";
      } else if (!clockcore::parseImageFileName(uploadName, n)) {
        uploadError = "invalid file name, expected <set>_<screen>_<digit>.png";
      } else if (server->clientContentLength() <= 0 ||
                 storage::usedBytes() + size_t(server->clientContentLength()) + kFsReserve > storage::totalBytes()) {
        uploadError = "not enough space on the clock";
      } else {
        uploadFile = LittleFS.open(kUploadTmp, "w");
        if (!uploadFile)
          uploadError = "cannot create file";
      }
      break;
    }
    case RAW_WRITE:
      if (uploadError == nullptr && uploadFile.write(raw.buf, raw.currentSize) != raw.currentSize)
        uploadError = "write failed (filesystem full?)";
      break;
    case RAW_END:
      if (uploadFile)
        uploadFile.close();
      if (uploadError == nullptr) {
        const std::string target = "/" + uploadName;
        LittleFS.remove(target.c_str());
        if (!LittleFS.rename(kUploadTmp, target.c_str()))
          uploadError = "rename failed";
      }
      if (uploadError != nullptr)
        LittleFS.remove(kUploadTmp);
      break;
    case RAW_ABORTED:
      if (uploadFile)
        uploadFile.close();
      LittleFS.remove(kUploadTmp);
      uploadError = "upload aborted";
      break;
  }
}

void
handleUploadDone()
{
  if (uploadError != nullptr) {
    sendError(strcmp(uploadError, "unauthorized") == 0 ? 401 : 400, uploadError);
    return;
  }
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
  // Rescanning after every file of a 60 file upload is cheap (directory listing only).
  app::filesChanged();
}

void
handleDeleteFile()
{
  if (!requireAuth())
    return;
  const std::string name = server->pathArg(0).c_str();
  clockcore::ImageName n;
  if (!clockcore::parseImageFileName(name, n)) {
    sendError(400, "invalid file name");
    return;
  }
  if (!storage::removeFile(name)) {
    sendError(404, "no such file");
    return;
  }
  app::filesChanged();
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
}

void
handleDeleteSet()
{
  if (!requireAuth())
    return;
  const std::string set = server->pathArg(0).c_str();
  if (!clockcore::isValidSetName(set)) {
    sendError(400, "invalid set name");
    return;
  }
  int removed = 0;
  for (const auto& f : storage::listFiles()) {
    clockcore::ImageName n;
    if (clockcore::parseImageFileName(f.name, n) && n.set == set && storage::removeFile(f.name))
      ++removed;
  }
  if (removed == 0) {
    sendError(404, "no such set");
    return;
  }
  app::filesChanged();
  JsonDocument doc;
  doc["ok"] = true;
  doc["removed"] = removed;
  sendJson(200, doc);
}

void
handleDisplay()
{
  if (!requireAuth())
    return;
  JsonDocument in;
  if (deserializeJson(in, server->arg("plain")) || !in["on"].is<bool>()) {
    sendError(400, "expected {\"on\": true|false}");
    return;
  }
  app::setDisplayPower(in["on"].as<bool>());
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
}

void
handleReboot()
{
  if (!requireAuth())
    return;
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(200, doc);
  app::scheduleReboot(500);
}

} // namespace

void
ApiServer::start(const std::string& t)
{
  if (running_ || t.empty())
    return;
  token = t;
  server = new WebServer(80);
  const char* headers[] = { "Authorization" };
  server->collectHeaders(headers, 1);

  server->on("/api/info", HTTP_GET, handleInfo);
  server->on("/api/config", HTTP_GET, handleGetConfig);
  server->on("/api/config", HTTP_PATCH, handlePatchConfig);
  server->on("/api/config", HTTP_POST, handlePatchConfig); // for clients without PATCH
  server->on("/api/time", HTTP_POST, handleSetTime);
  server->on("/api/files", HTTP_GET, handleListFiles);
  server->on(UriBraces("/api/files/{}"), HTTP_PUT, handleUploadDone, handleUploadBody);
  server->on(UriBraces("/api/files/{}"), HTTP_DELETE, handleDeleteFile);
  server->on("/api/sets", HTTP_GET, handleListSets);
  server->on(UriBraces("/api/sets/{}"), HTTP_DELETE, handleDeleteSet);
  server->on("/api/display", HTTP_POST, handleDisplay);
  server->on("/api/reboot", HTTP_POST, handleReboot);
  server->onNotFound([] { sendError(404, "not found"); });
  server->begin();
  running_ = true;
  Serial.println("API: listening on port 80");
}

void
ApiServer::stop()
{
  if (!running_)
    return;
  server->stop();
  delete server;
  server = nullptr;
  token.clear();
  running_ = false;
  Serial.println("API: stopped");
}

void
ApiServer::loop()
{
  if (running_)
    server->handleClient();
}

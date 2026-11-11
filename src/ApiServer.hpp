// SPDX-License-Identifier: GPL-3.0-or-later
//
// Small HTTP API used by the desktop app (see docs/http-api.md).
// All endpoints except GET /api/info require "Authorization: Bearer <token>".

#pragma once

#include <string>

class ApiServer
{
public:
    void start(const std::string& token);
    void stop();
    void loop();
    bool running() const { return running_; }

private:
    bool running_ = false;
};

// KamTape PS5 - API requests on a background thread, collected by the frame.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kt/http.hpp"
#include "kt/worker.hpp"

#include <map>
#include <set>
#include <mutex>
#include <string>

namespace hui::kt
{

class ApiClient
{
  public:
    // Starts a GET; returns its ticket (never 0).
    int fetch(const std::string &url);
    // The answer for a ticket once it has come back (and forgets it).
    bool take(int ticket, http::Response *response);
    bool busy() const;
    // The page that asked is gone: drop its answer when it comes.
    void forget(int ticket);

  private:
    mutable std::mutex mutex_;
    std::map<int, http::Response> done_;
    std::set<int> forgotten_;
    int next_ = 1;
    int pending_ = 0;
    JobWorker worker_; // last: joined first, while the rest still exists
};

} // namespace hui::kt

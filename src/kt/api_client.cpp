// KamTape PS5 - API requests on a background thread, collected by the frame.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/api_client.hpp"

#include <utility>

namespace hui::kt
{

int ApiClient::fetch(const std::string &url)
{
    int ticket = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ticket = next_++;
        ++pending_;
    }
    worker_.queue([this, ticket, url] {
        http::Response response = http::get(url, 2u << 20);
        std::lock_guard<std::mutex> lock(mutex_);
        if (forgotten_.erase(ticket) == 0)
            done_[ticket] = std::move(response);
        --pending_;
    });
    return ticket;
}

bool ApiClient::take(int ticket, http::Response *response)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = done_.find(ticket);
    if (it == done_.end())
        return false;
    *response = std::move(it->second);
    done_.erase(it);
    return true;
}

void ApiClient::forget(int ticket)
{
    if (ticket == 0)
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (done_.erase(ticket) == 0)
        forgotten_.insert(ticket);
}

bool ApiClient::busy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_ > 0;
}

} // namespace hui::kt

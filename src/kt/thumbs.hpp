// KamTape PS5 - video thumbnails, downloaded and kept as GL textures
// (fetched and decoded on worker threads, uploaded on the main thread).
// Adapted from Flashpoint PS5's image cache.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "kt/worker.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hui::kt
{

struct Texture
{
    std::uint32_t id = 0; // 0 while loading or when there is no image
    int width = 0;
    int height = 0;
};

class Thumbs
{
  public:
    Thumbs();
    ~Thumbs();
    Thumbs(const Thumbs &) = delete;
    Thumbs &operator=(const Thumbs &) = delete;

    // The texture for a picture's URL, starting the download the first time.
    Texture get(const std::string &url);
    // Uploads finished decodes and frees the least recently used textures.
    void pump();
    // Forgets queued downloads (e.g. after a page flip) so new ones go first.
    void cancel_pending();

  private:
    struct Decoded
    {
        std::string rel;
        std::vector<unsigned char> rgba;
        int width = 0;
        int height = 0;
    };
    struct Entry
    {
        Texture texture;
        bool loading = false;
        bool failed = false;
        int attempts = 0;                              // failures so far
        std::chrono::steady_clock::time_point retry_at; // when a failed one is asked again
        std::uint64_t used = 0;
    };

    int next_worker_ = 0;
    std::uint64_t clock_ = 0;
    std::unordered_map<std::string, Entry> entries_;
    std::mutex mutex_;
    std::vector<Decoded> decoded_;
    std::atomic<std::uint64_t> generation_{0};
    JobWorker workers_[2]; // last: joined first, while the rest still exists
};

} // namespace hui::kt

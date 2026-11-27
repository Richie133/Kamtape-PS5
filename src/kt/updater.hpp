// KamTape PS5 - updates from the app's own GitHub releases.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Once per launch the app asks GitHub for the repository's releases and looks
// for one newer than itself: a release whose notes say its contentVersion
// (the release workflow writes it, with the zip's SHA-256) and that carries
// PPSA52005.zip. Installing hands that offer to the boilerplate's self-update
// kit (src/third_party/update_check/self_update.*): the app downloads the zip
// and streams it to the self-update helper (third_party/self_update_helper,
// shipped as self-updater.elf), which it sends to the console's payload loader
// on port 9021. The helper checks the size and SHA-256, unpacks the release
// next to the installed folder and, once the app has closed, puts the new
// files in place. With no payload loader listening, the install fails before
// anything is touched, and the app points to the release page instead.

#pragma once

#include "kt/release.hpp"
#include "kt/worker.hpp"
#include "third_party/update_check/self_update.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace hui::kt
{

class Updater
{
  public:
    enum class State
    {
        idle,
        checking,
        up_to_date,
        available,
        unknown, // no answer (offline, rate-limited, unreadable): say nothing unasked
    };

    // param_path: the running app's sce_sys/param.json.
    explicit Updater(std::string param_path);
    ~Updater();
    Updater(const Updater &) = delete;
    Updater &operator=(const Updater &) = delete;

    void check(); // asks GitHub in the background
    State state();
    const std::string &installed() const { return installed_; } // contentVersion
    Release release();                                          // when state() is available
    std::string why();                                          // when unknown

    // The install. start() says whether the job started; then poll() every frame:
    // it keeps returning the last status (a failure's reason too) after the job ends.
    bool start();
    self_update_status poll();
    bool installing() const { return job_started_; }
    bool apply();  // in SELF_UPDATE_READY: 1 when the helper waits for the app to close
    void cancel(); // before apply; never blocks: poll() collects the job once it has stopped
    void clear();  // forget a failure's status once the user has seen it

  private:
    std::string param_path_;
    std::string installed_;
    std::mutex mutex_;
    State state_ = State::idle;
    Release release_;
    std::string why_;
    std::unique_ptr<self_update_job> job_;
    bool job_started_ = false;
    self_update_status last_{};
    JobWorker worker_; // last: joined first
};

} // namespace hui::kt

// KamTape PS5 - updates from the app's own GitHub releases.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kt/updater.hpp"

#include "core/save_file.hpp"
#include "kt/http.hpp"
#include "platform/ps5/system.hpp"
#include "third_party/update_check/update_check.h"

#include <cstdio>
#include <cstring>

namespace hui::kt
{

// ---- the updater ----------------------------------------------------------------------

Updater::Updater(std::string param_path) : param_path_(std::move(param_path))
{
    std::string title;
    if (!read_param(param_path_, &title, &installed_))
        installed_.clear();
}

Updater::~Updater()
{
    // an install still running ends with the app (the helper undoes its work when the
    // connection drops); its thread may still use the job, so the job is left to the process
    if (job_started_ && job_)
    {
        self_update_cancel(job_.get());
        (void)job_.release();
    }
}

void Updater::check()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::checking)
            return;
        state_ = State::checking;
    }
    if (installed_.empty())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = State::unknown;
        why_ = "The app's own version couldn't be read";
        return;
    }
    const std::string installed = installed_;
    worker_.queue([this, installed] {
        const http::Response response = http::get(kReleasesApi, 1u << 20);
        std::string why;
        Release release;
        State state = State::unknown;
        if (!response.ok())
        {
            why = response.status == 403 || response.status == 429 ? "GitHub is busy, try again later"
                  : response.status > 0                            ? "GitHub answered HTTP " + std::to_string(response.status)
                                                                   : "No connection to GitHub";
        }
        else
        {
            release = pick_release(response.body, installed, &why);
            state = !release.tag.empty() ? State::available : why.empty() ? State::up_to_date : State::unknown;
        }
        sys::log("[KT] update check: %s %s", release.tag.empty() ? "none" : release.tag.c_str(), why.c_str());
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = state;
        release_ = std::move(release);
        why_ = std::move(why);
    });
}

Updater::State Updater::state()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

Release Updater::release()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return release_;
}

std::string Updater::why()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return why_;
}

bool Updater::start()
{
    if (job_started_)
        return false;
    const Release r = release();
    if (r.tag.empty())
        return false;
    self_update_offer offer{};
    std::snprintf(offer.title, sizeof(offer.title), "%s", kTitleId);
    std::snprintf(offer.name, sizeof(offer.name), "%s", kAppName);
    std::snprintf(offer.installed, sizeof(offer.installed), "%s", installed_.c_str());
    std::snprintf(offer.available, sizeof(offer.available), "%s", r.content_version.c_str());
    std::snprintf(offer.version, sizeof(offer.version), "%s", r.version.c_str());
    std::snprintf(offer.artifact, sizeof(offer.artifact), "%s", r.zip_url.c_str());
    std::snprintf(offer.sha256, sizeof(offer.sha256), "%s", r.sha256.c_str());
    std::snprintf(offer.page, sizeof(offer.page), "%s", r.page.c_str());
    offer.size = r.size;
    job_ = std::make_unique<self_update_job>();
    std::memset(job_.get(), 0, sizeof(self_update_job));
    last_ = self_update_status{};
    last_.phase = SELF_UPDATE_STARTING;
    job_started_ = self_update_start(job_.get(), self_update_console(), &offer) == 1;
    if (!job_started_)
    {
        last_.phase = SELF_UPDATE_FAILED;
        std::snprintf(last_.error, sizeof(last_.error), "%s", "The update couldn't start");
    }
    sys::log("[KT] update %s -> %s started=%d", offer.installed, offer.available, job_started_ ? 1 : 0);
    return job_started_;
}

self_update_status Updater::poll()
{
    if (job_started_)
    {
        self_update_poll(job_.get(), &last_);
        if (last_.phase == SELF_UPDATE_FAILED || last_.phase == SELF_UPDATE_CANCELLED)
        {
            // its thread has ended (or is just returning): collect it so a retry can start
            self_update_finish(job_.get());
            job_started_ = false;
            if (last_.phase == SELF_UPDATE_FAILED)
                sys::log("[KT] update failed: %s", last_.error);
        }
    }
    const self_update_status out = last_;
    if (last_.phase == SELF_UPDATE_CANCELLED)
        last_ = self_update_status{}; // said once; nothing is left to show
    return out;
}

void Updater::clear()
{
    if (!job_started_)
        last_ = self_update_status{};
}

bool Updater::apply()
{
    return job_started_ && self_update_apply(job_.get()) == 1;
}

void Updater::cancel()
{
    if (job_started_)
        self_update_cancel(job_.get());
}

} // namespace hui::kt

//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "third_party/starboard/rdk/shared/libcobalt.h"

#include <cstring>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/semaphore.h"
#include "starboard/once.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#include "third_party/starboard/rdk/shared/rdkservices.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"

using namespace third_party::starboard::rdk::shared;

namespace
{

struct APIContext
{
  APIContext()
    : mutex_()
    , condition_(mutex_)
  { }

  void OnInitialize()
  {
    starboard::ScopedLock lock(mutex_);
    running_ = (nullptr != Application::Get());
    condition_.Broadcast();
  }

  void OnTeardown()
  {
    starboard::ScopedLock lock(mutex_);
    running_ = false;
  }

  void SendLink(const char* link)
  {
    starboard::ScopedLock lock(mutex_);
    WaitForApp(lock);
    Application::Get()->Link(link);
  }

  void RequestSuspend() {
    starboard::ScopedLock lock(mutex_);
    WaitForApp(lock);
    starboard::Semaphore sem;
#if SB_API_VERSION >= 13
    Application::Get()->Freeze(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#else
    Application::Get()->Suspend(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#endif
    sem.Take();
  }

  void RequestResume() {
    starboard::ScopedLock lock(mutex_);
    WaitForApp(lock);
    starboard::Semaphore sem;
#if SB_API_VERSION >= 13
    Application::Get()->Focus(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#else
    Application::Get()->Unpause(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#endif
    sem.Take();
  }

  void RequestPause() {
    starboard::ScopedLock lock(mutex_);
    WaitForApp(lock);
    starboard::Semaphore sem;
#if SB_API_VERSION >= 13
    Application::Get()->Blur(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#else
    Application::Get()->Pause(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#endif
    sem.Take();
  }

  void RequestUnpause() {
    starboard::ScopedLock lock(mutex_);
    WaitForApp(lock);
    starboard::Semaphore sem;
#if SB_API_VERSION >= 13
    Application::Get()->Focus(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#else
    Application::Get()->Unpause(
      &sem,
      [](void* ctx) {
        reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
      });
#endif
    sem.Take();
  }

  void RequestQuit()
  {
    starboard::ScopedLock lock(mutex_);
    stop_request_cb_ = nullptr;
    stop_request_cb_data_ = nullptr;
    if (running_)
        Application::Get()->Stop(0);
  }

  void SetStopRequestHandler(SbRdkCallbackFunc cb, void* user_data)
  {
    starboard::ScopedLock lock(mutex_);
    stop_request_cb_ = cb;
    stop_request_cb_data_ = user_data;
  }

  void RequestStop()
  {
    SbRdkCallbackFunc cb;
    void* user_data;
    int should_invoke_default = 1;

    mutex_.Acquire();
    cb = stop_request_cb_;
    user_data = stop_request_cb_data_;
    mutex_.Release();

    if (cb) {
      should_invoke_default = cb(user_data);
    }

    if (should_invoke_default) {
      RequestQuit();
    }
  }

  void SetConcealRequestHandler(SbRdkCallbackFunc cb, void* user_data)
  {
    starboard::ScopedLock lock(mutex_);
    conceal_request_cb_ = cb;
    conceal_request_cb_data_ = user_data;
  }

  void RequestConceal()
  {
    SbRdkCallbackFunc cb;
    void* user_data;
    int should_invoke_default = 1;

    mutex_.Acquire();
    cb = conceal_request_cb_;
    user_data = conceal_request_cb_data_;
    mutex_.Release();

    if (cb) {
      should_invoke_default = cb(user_data);
    }

    if (should_invoke_default) {
#if SB_API_VERSION >= 13
      Application::Get()->Conceal(NULL, NULL);
#else
      Application::Get()->Suspend(NULL, NULL);
#endif
    }
  }

  void SetCobaltExitStrategy(const char* strategy)
  {
    if (running_) {
      SB_LOG(WARNING) << "Ignore exit strategy change, app is already running.";
      return;
    }

    // Supported values(src/cobalt/extension/configuration.h): stop, suspend, noexit.
    if (strncmp(strategy, "suspend", 7) == 0)
      exit_strategy_ = "suspend";
    else if (strncmp(strategy, "noexit", 6) == 0)
      exit_strategy_ = "noexit";
    else
      exit_strategy_ = "stop";
  }

  const char* GetCobaltExitStrategy()
  {
    return exit_strategy_.c_str();
  }

private:
  void WaitForApp(starboard::ScopedLock &)
  {
    while ( running_ == false )
      condition_.Wait();
  }

  bool running_ { false };
  starboard::Mutex mutex_;
  starboard::ConditionVariable condition_;
  SbRdkCallbackFunc stop_request_cb_ { nullptr };
  void* stop_request_cb_data_ { nullptr };
  SbRdkCallbackFunc conceal_request_cb_ { nullptr };
  void* conceal_request_cb_data_ { nullptr };
  std::string exit_strategy_ { "stop" };
};

SB_ONCE_INITIALIZE_FUNCTION(APIContext, GetContext);

}  // namespace

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace libcobalt_api {

void Initialize()
{
  GetContext()->OnInitialize();
}

void Teardown()
{
  GetContext()->OnTeardown();
}

}  // namespace libcobalt_api
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

extern "C" {

void SbRdkHandleDeepLink(const char* link) {
  GetContext()->SendLink(link);
}

void SbRdkSuspend() {
  GetContext()->RequestSuspend();
}

void SbRdkResume() {
  GetContext()->RequestResume();
}

void SbRdkPause() {
  GetContext()->RequestPause();
}

void SbRdkUnpause() {
  GetContext()->RequestUnpause();
}

void SbRdkQuit() {
  GetContext()->RequestQuit();
}

void SbRdkSetSetting(const char* key, const char* json) {
  if (!key || key[0] == '\0' || !json)
    return;

  if (strcmp(key, "accessibility") == 0) {
    Accessibility::SetSettings(json);
  }
  else if (strcmp(key, "systemproperties") == 0) {
    SystemProperties::SetSettings(json);
  }
}

int SbRdkGetSetting(const char* key, char** out_json) {
  if (!key || key[0] == '\0' || !out_json || *out_json != nullptr)
    return -1;

  bool result = false;
  std::string tmp;

  if (strcmp(key, "accessibility") == 0) {
    result = Accessibility::GetSettings(tmp);
  }
  else if (strcmp(key, "systemproperties") == 0) {
    result = SystemProperties::GetSettings(tmp);
  }

  if (result && !tmp.empty()) {
    char *out = (char*)malloc(tmp.size() + 1);
    memcpy(out, tmp.c_str(), tmp.size());
    out[tmp.size()] = '\0';
    *out_json = out;
    return 0;
  }

  return -1;
}

void SbRdkSetStopRequestHandler(SbRdkCallbackFunc cb, void* user_data) {
  GetContext()->SetStopRequestHandler(cb, user_data);
}

void SbRdkRequestStop() {
  GetContext()->RequestStop();
}

void SbRdkSetConcealRequestHandler(SbRdkCallbackFunc cb, void* user_data) {
  GetContext()->SetConcealRequestHandler(cb, user_data);
}

void SbRdkRequestConceal() {
  GetContext()->RequestConceal();
}

void SbRdkSetCobaltExitStrategy(const char* strategy) {
  GetContext()->SetCobaltExitStrategy(strategy);
}

const char* SbRdkGetCobaltExitStrategy() {
  return GetContext()->GetCobaltExitStrategy();
}

}  // extern "C"

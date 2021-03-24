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

#include "third_party/starboard/rdk/shared/rdkservices.h"

#include <string>
#include <algorithm>

#include <websocket/JSONRPCLink.h>

#include <interfaces/json/JsonData_HDRProperties.h>
#include <interfaces/json/JsonData_PlayerProperties.h>
#include <interfaces/json/JsonData_DeviceIdentification.h>

#ifdef HAS_SECURITY_AGENT
#include <securityagent/securityagent.h>
#endif

#include "starboard/atomic.h"
#include "starboard/event.h"
#include "starboard/once.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/accessibility.h"

#include "third_party/starboard/rdk/shared/accessibility_data.h"
#include "third_party/starboard/rdk/shared/log_override.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"

using namespace  WPEFramework;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

const uint32_t kDefaultTimeoutMs = 100;
const char kDisplayInfoCallsign[] = "DisplayInfo.1";
const char kPlayerInfoCallsign[] = "PlayerInfo.1";
const char kDeviceIdentificationCallsign[] = "DeviceIdentification.1";
const char kNetworkCallsign[] = "org.rdk.Network.1";
const char kTTSCallsign[] = "org.rdk.TextToSpeech.1";

const uint32_t kPriviligedRequestErrorCode = -32604U;

class ServiceLink {
  ::starboard::scoped_ptr<JSONRPC::LinkType<Core::JSON::IElement>> link_;
  std::string callsign_;

#ifdef HAS_SECURITY_AGENT
  static Core::OptionalType<std::string> getToken() {
    if (getenv("THUNDER_SECURITY_OFF") != nullptr)
      return { };

    const uint32_t kMaxBufferSize = 2 * 1024;
    const std::string payload = "https://www.youtube.com";

    Core::OptionalType<std::string> token;
    std::vector<uint8_t> buffer;
    buffer.resize(kMaxBufferSize);

    for(int i = 0; i < 5; ++i) {
      uint32_t inputLen = std::min(kMaxBufferSize, payload.length());
      ::memcpy (buffer.data(), payload.c_str(), inputLen);

      int outputLen = GetToken(kMaxBufferSize, inputLen, buffer.data());
      SB_DCHECK(outputLen != 0);

      if (outputLen > 0) {
        token = std::string(reinterpret_cast<const char*>(buffer.data()), outputLen);
        break;
      }
      else if (outputLen < 0) {
        uint32_t rc = -outputLen;
        if (rc == Core::ERROR_TIMEDOUT && i < 5) {
          SB_LOG(ERROR) << "Failed to get token, trying again. rc = " << rc << " ( " << Core::ErrorToString(rc) << " )";
          continue;
        }
        SB_LOG(ERROR) << "Failed to get token, give up. rc = " << rc << " ( " << Core::ErrorToString(rc) << " )";
      }
      break;
    }
    return token;
  }
#endif

  static std::string buildQuery() {
    std::string query;
#ifdef HAS_SECURITY_AGENT
    static const auto token = getToken();
    if (token.IsSet() && !token.Value().empty())
      query = "token=" + token.Value();
#endif
    return query;
  }

public:
  static bool enableEnvOverrides() {
    static bool enable_env_overrides = ([]() {
      std::string envValue;
      if ((Core::SystemInfo::GetEnvironment("COBALT_ENABLE_OVERRIDES", envValue) == true) && (envValue.empty() == false)) {
        return envValue.compare("1") == 0 || envValue.compare("true") == 0;
      }
      return false;
    })();
    return enable_env_overrides;
  }

  ServiceLink(const std::string callsign) : callsign_(callsign) {
    if (getenv("THUNDER_ACCESS") != nullptr)
      link_.reset(new JSONRPC::LinkType<Core::JSON::IElement>(callsign, nullptr, false, buildQuery()));
  }

  template <typename PARAMETERS>
  uint32_t Get(const uint32_t waitTime, const string& method, PARAMETERS& sendObject) {
    if (enableEnvOverrides()) {
      std::string envValue;
      std::string envName = Core::JSONRPC::Message::Callsign(callsign_) + "_" + method;
      envName.erase(std::remove(envName.begin(), envName.end(), '.'), envName.end());
      if (Core::SystemInfo::GetEnvironment(envName, envValue) == true) {
        return sendObject.FromString(envValue) ? Core::ERROR_NONE : Core::ERROR_GENERAL;
      }
    }
    if (!link_)
      return Core::ERROR_UNAVAILABLE;
    return link_->template Get<PARAMETERS>(waitTime, method, sendObject);
  }

  template <typename PARAMETERS, typename HANDLER, typename REALOBJECT>
  uint32_t Dispatch(const uint32_t waitTime, const string& method, const PARAMETERS& parameters, const HANDLER& callback, REALOBJECT* objectPtr) {
    if (!link_)
      return Core::ERROR_UNAVAILABLE;
    return link_->template Dispatch<PARAMETERS, HANDLER, REALOBJECT>(waitTime, method, parameters, callback, objectPtr);
  }

  template <typename HANDLER, typename REALOBJECT>
  uint32_t Dispatch(const uint32_t waitTime, const string& method, const HANDLER& callback, REALOBJECT* objectPtr) {
    if (!link_)
      return Core::ERROR_UNAVAILABLE;
    return link_->template Dispatch<void, HANDLER, REALOBJECT>(waitTime, method, callback, objectPtr);
  }

  template <typename INBOUND, typename METHOD, typename REALOBJECT>
  uint32_t Subscribe(const uint32_t waitTime, const string& eventName, const METHOD& method, REALOBJECT* objectPtr) {
    if (!link_)
      return enableEnvOverrides() ? Core::ERROR_NONE : Core::ERROR_UNAVAILABLE;
    return link_->template Subscribe<INBOUND, METHOD, REALOBJECT>(waitTime, eventName, method, objectPtr);
  }

  void Unsubscribe(const uint32_t waitTime, const string& eventName) {
    if (!link_)
      return;
    return link_->Unsubscribe(waitTime, eventName);
  }
};

struct DeviceIdImpl {
  DeviceIdImpl() {
    JsonData::DeviceIdentification::DeviceidentificationData data;
    uint32_t rc = ServiceLink(kDeviceIdentificationCallsign)
      .Get(2000, "deviceidentification", data);
    if (Core::ERROR_NONE == rc) {
      chipset = data.Chipset.Value();
      firmware_version = data.Firmwareversion.Value();
      std::replace(chipset.begin(), chipset.end(), ' ', '-');
    }
    if (Core::ERROR_NONE != rc) {
      #if defined(SB_PLATFORM_CHIPSET_MODEL_NUMBER_STRING)
      chipset = SB_PLATFORM_CHIPSET_MODEL_NUMBER_STRING;
      #endif
      #if defined(SB_PLATFORM_FIRMWARE_VERSION_STRING)
      firmware_version = SB_PLATFORM_FIRMWARE_VERSION_STRING;
      #endif
    }
  }
  std::string chipset;
  std::string firmware_version;
};

SB_ONCE_INITIALIZE_FUNCTION(DeviceIdImpl, GetDeviceIdImpl);

struct TextToSpeechImpl {
private:
  ::starboard::atomic_bool is_enabled_ { false };
  int64_t speech_id_ { -1 };
  int32_t speech_request_num_ { 0 };
  ServiceLink tts_link_ { kTTSCallsign };
  ::starboard::Mutex mutex_;
  ::starboard::ConditionVariable condition_ { mutex_ };

  struct IsTTSEnabledInfo : public Core::JSON::Container {
    IsTTSEnabledInfo()
      : Core::JSON::Container() {
      Add(_T("isenabled"), &IsEnabled);
    }
    IsTTSEnabledInfo(const IsTTSEnabledInfo&) = delete;
    IsTTSEnabledInfo& operator=(const IsTTSEnabledInfo&) = delete;

    Core::JSON::Boolean IsEnabled;
  };

  struct SpeakResult : public Core::JSON::Container {
    SpeakResult()
      : Core::JSON::Container()
      , SpeechId(-1) {
      Add(_T("speechid"), &SpeechId);
    }
    SpeakResult(const SpeakResult&) = delete;
    SpeakResult& operator=(const SpeakResult&) = delete;

    Core::JSON::DecSInt64 SpeechId;
  };

  struct StateInfo : public Core::JSON::Container {
    StateInfo()
      : Core::JSON::Container()
      , State(false) {
      Add(_T("state"), &State);
    }
    StateInfo(const StateInfo& other)
      : Core::JSON::Container()
      , State(other.State) {
      Add(_T("state"), &State);
    }
    StateInfo& operator=(const StateInfo&) = delete;

    Core::JSON::Boolean State;
  };

  void OnCancelResult(const Core::JSON::String&, const Core::JSONRPC::Error*) {
  }

  void OnStateChanged(const StateInfo& info) {
    is_enabled_.store( info.State.Value() );
  }

  void OnSpeakResult(const SpeakResult& result, const Core::JSONRPC::Error* err) {
    ::starboard::ScopedLock lock(mutex_);
    if (err) {
      SB_LOG(ERROR)
          << "TTS speak request failed. Error code: "
          << err->Code.Value()
          << " message: "
          << err->Text.Value();
      speech_id_ = -1;
    }
    else {
      speech_id_ = result.SpeechId;
    }
    --speech_request_num_;
    condition_.Broadcast();
  }

public:
  TextToSpeechImpl() {
    uint32_t rc;
    rc = tts_link_.Subscribe<StateInfo>(kDefaultTimeoutMs, "onttsstatechanged", &TextToSpeechImpl::OnStateChanged, this);
    if (Core::ERROR_NONE != rc) {
      SB_LOG(ERROR)
          << "Failed to subscribe to '" << kTTSCallsign
          << ".onttsstatechanged' event, rc=" << rc
          << " ( " << Core::ErrorToString(rc) << " )";
    }

    IsTTSEnabledInfo info;
    rc = tts_link_.Get(kDefaultTimeoutMs, "isttsenabled", info);
    if (Core::ERROR_NONE == rc) {
      is_enabled_.store( info.IsEnabled.Value() );
    }
  }

  void Speak(const std::string &text) {
    if (!is_enabled_.load())
      return;

    JsonObject params;
    params.Set(_T("text"), text);

    uint64_t rc = tts_link_.Dispatch(kDefaultTimeoutMs, "speak", params, &TextToSpeechImpl::OnSpeakResult, this);
    if (Core::ERROR_NONE == rc) {
      ::starboard::ScopedLock lock(mutex_);
      ++speech_request_num_;
    }
  }

  void Cancel() {
    if (!is_enabled_.load())
      return;

    int64_t speechId = -1;

    {
      ::starboard::ScopedLock lock(mutex_);
      if (speech_request_num_ != 0) {
        if (!condition_.WaitTimed(kSbTimeMillisecond) || speech_request_num_ != 0)
          return;
      }
      speechId = speech_id_;
    }

    if (speechId < 0)
      return;

    JsonObject params;
    params.Set(_T("speechid"), speechId);

    tts_link_.Dispatch(kDefaultTimeoutMs, "cancel", params, &TextToSpeechImpl::OnCancelResult, this);
  }

  bool IsEnabled() const {
    return is_enabled_.load();
  }
};

SB_ONCE_INITIALIZE_FUNCTION(TextToSpeechImpl, GetTextToSpeech);

struct AccessibilityImpl {
private:
  ::starboard::Mutex mutex_;
  SbAccessibilityDisplaySettings display_settings_ { };
  SbAccessibilityCaptionSettings caption_settings_ { };

public:
  AccessibilityImpl() {
    SbMemorySet(&display_settings_, 0, sizeof(display_settings_));
    SbMemorySet(&caption_settings_, 0, sizeof(caption_settings_));

    if (ServiceLink::enableEnvOverrides()) {
      std::string envValue;
      if (Core::SystemInfo::GetEnvironment("AccessibilitySettings_json", envValue) == true) {
        SetSettings(envValue);

        std::string test;
        bool r = GetSettings(test);
        SB_LOG(INFO) << "Initialized from 'AccessibilitySettings_json',"
                     << " env variable json: '" << envValue << "',"
                     << " conversion result: " << r << ","
                     << " accessibility setting json: '" << test << "'";
      }
    }
  }

  void SetSettings(const std::string& json) {

    SB_LOG(INFO) << "Updating accessibility settings: " << json;

    JsonData::Accessibility::AccessibilityData settings;
    Core::OptionalType<Core::JSON::Error> error;
    if ( !settings.FromString(json, error) ) {
      SB_LOG(ERROR) << "Failed to parse accessibility settings, error: "
                    << (error.IsSet() ? Core::JSON::ErrorDisplayMessage(error.Value()): "Unknown");
      return;
    }

    ::starboard::ScopedLock lock(mutex_);

    SbMemorySet(&display_settings_, 0, sizeof(display_settings_));
    SbMemorySet(&caption_settings_, 0, sizeof(caption_settings_));

    const auto& cc = settings.ClosedCaptions;

    caption_settings_.supports_is_enabled = true;
    caption_settings_.supports_set_enabled = false;
    caption_settings_.is_enabled = cc.IsEnabled.Value();

    if (cc.BackgroundColor.IsSet()) {
      caption_settings_.background_color = cc.BackgroundColor.Value();
      caption_settings_.background_color_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.BackgroundOpacity.IsSet()) {
      caption_settings_.background_opacity = cc.BackgroundOpacity.Value();
      caption_settings_.background_opacity_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.CharacterEdgeStyle.IsSet()) {
      caption_settings_.character_edge_style = cc.CharacterEdgeStyle.Value();
      caption_settings_.character_edge_style_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.FontColor.IsSet()) {
      caption_settings_.font_color = cc.FontColor.Value();
      caption_settings_.font_color_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.FontFamily.IsSet()) {
      caption_settings_.font_family = cc.FontFamily.Value();
      caption_settings_.font_family_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.FontOpacity.IsSet()) {
      caption_settings_.font_opacity = cc.FontOpacity.Value();
      caption_settings_.font_opacity_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.FontSize.IsSet()) {
      caption_settings_.font_size = cc.FontSize.Value();
      caption_settings_.font_size_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.WindowColor.IsSet()) {
      caption_settings_.window_color = cc.WindowColor.Value();
      caption_settings_.window_color_state = kSbAccessibilityCaptionStateSet;
    }
    if (cc.WindowOpacity.IsSet()) {
      caption_settings_.window_opacity = cc.WindowOpacity.Value();
      caption_settings_.window_opacity_state = kSbAccessibilityCaptionStateSet;
    }

    if (settings.TextDisplay.IsHighContrastTextEnabled.IsSet()) {
      display_settings_.has_high_contrast_text_setting = true;
      display_settings_.is_high_contrast_text_enabled =
        settings.TextDisplay.IsHighContrastTextEnabled.Value();
    }
  }

  bool GetSettings(std::string& out_json) {
    JsonData::Accessibility::AccessibilityData settings;

    {
      ::starboard::ScopedLock lock(mutex_);
      if (caption_settings_.supports_is_enabled) {
        auto& cc = settings.ClosedCaptions;
        cc.IsEnabled = caption_settings_.is_enabled;
        if (caption_settings_.background_color_state)
          cc.BackgroundColor = caption_settings_.background_color;
        if (caption_settings_.background_opacity_state)
          cc.BackgroundOpacity = caption_settings_.background_opacity;
        if (caption_settings_.character_edge_style_state)
          cc.CharacterEdgeStyle = caption_settings_.character_edge_style;
        if (caption_settings_.font_color_state)
          cc.FontColor = caption_settings_.font_color;
        if (caption_settings_.font_family_state)
          cc.FontFamily = caption_settings_.font_family;
        if (caption_settings_.font_opacity_state)
          cc.FontOpacity = caption_settings_.font_opacity;
        if (caption_settings_.font_size_state)
          cc.FontSize = caption_settings_.font_size;
        if (caption_settings_.window_color_state)
          cc.WindowColor = caption_settings_.window_color;
        if (caption_settings_.window_opacity_state)
          cc.WindowOpacity = caption_settings_.window_opacity;
      }

      if (display_settings_.has_high_contrast_text_setting)
        settings.TextDisplay.IsHighContrastTextEnabled = display_settings_.is_high_contrast_text_enabled;
    }

    return settings.ToString(out_json);
  }

  bool GetCaptionSettings(SbAccessibilityCaptionSettings* out) const {
    if (out) {
      ::starboard::ScopedLock lock(mutex_);
      SbMemoryCopy(out, &caption_settings_,  sizeof(caption_settings_));
      return true;
    }
    return false;
  }

  bool GetDisplaySettings(SbAccessibilityDisplaySettings* out) const {
    if (out) {
      ::starboard::ScopedLock lock(mutex_);
      SbMemoryCopy(out, &display_settings_,  sizeof(display_settings_));
      return true;
    }
    return false;
  }

};

SB_ONCE_INITIALIZE_FUNCTION(AccessibilityImpl, GetAccessibility);

struct SystemPropertiesImpl {
  struct SystemPropertiesData : public Core::JSON::Container {
    SystemPropertiesData()
      : Core::JSON::Container() {
      Add(_T("modelname"), &ModelName);
      Add(_T("brandname"), &BrandName);
      Add(_T("modelyear"), &ModelYear);
      Add(_T("chipsetmodelnumber"), &ChipsetModelNumber);
      Add(_T("firmwareversion"), &FirmwareVersion);
      Add(_T("integratorname"), &IntegratorName);
      Add(_T("friendlyname"), &FriendlyName);
    }
    SystemPropertiesData(const SystemPropertiesData&) = delete;
    SystemPropertiesData& operator=(const SystemPropertiesData&) = delete;

    Core::JSON::String ModelName;
    Core::JSON::String BrandName;
    Core::JSON::String ModelYear;
    Core::JSON::String ChipsetModelNumber;
    Core::JSON::String FirmwareVersion;
    Core::JSON::String IntegratorName;
    Core::JSON::String FriendlyName;
  };

  void SetSettings(const std::string& json) {
    ::starboard::ScopedLock lock(mutex_);
    Core::OptionalType<Core::JSON::Error> error;
    if ( !props_.FromString(json, error) ) {
      props_.Clear();
      SB_LOG(ERROR) << "Failed to parse systemproperties settings, error: "
                    << (error.IsSet() ? Core::JSON::ErrorDisplayMessage(error.Value()): "Unknown");
      return;
    }
  }

  bool GetSettings(std::string& out_json) const {
    ::starboard::ScopedLock lock(mutex_);
    return props_.ToString(out_json);
  }

  bool GetModelName(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.ModelName.IsSet() && !props_.ModelName.Value().empty()) {
      out = props_.ModelName.Value();
      return true;
    }
    return false;
  }

  bool GetBrandName(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.BrandName.IsSet() && !props_.BrandName.Value().empty()) {
      out = props_.BrandName.Value();
      return true;
    }
    return false;
  }

  bool GetModelYear(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.ModelYear.IsSet() && !props_.ModelYear.Value().empty()) {
      out = props_.ModelYear.Value();
      return true;
    }
    return false;
  }

  bool GetChipset(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.ChipsetModelNumber.IsSet() && !props_.ChipsetModelNumber.Value().empty()) {
      out = props_.ChipsetModelNumber.Value();
      return true;
    }
    return false;
  }

  bool GetFirmwareVersion(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.FirmwareVersion.IsSet() && !props_.FirmwareVersion.Value().empty()) {
      out = props_.FirmwareVersion.Value();
      return true;
    }
    return false;
  }

  bool GetIntegratorName(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.IntegratorName.IsSet() && !props_.IntegratorName.Value().empty()) {
      out = props_.IntegratorName.Value();
      return true;
    }
    return false;
  }

  bool GetFriendlyName(std::string &out) const {
    ::starboard::ScopedLock lock(mutex_);
    if (props_.FriendlyName.IsSet() && !props_.FriendlyName.Value().empty()) {
      out = props_.FriendlyName.Value();
      return true;
    }
    return false;
  }

private:
  ::starboard::Mutex mutex_;
  SystemPropertiesData props_;
};

SB_ONCE_INITIALIZE_FUNCTION(SystemPropertiesImpl, GetSystemProperties);

}  // namespace

struct DisplayInfo::Impl {
  Impl();
  ~Impl();
  ResolutionInfo GetResolution() {
    Refresh();
    return resolution_info_;
  }
  bool HasHDRSupport() {
    Refresh();
    return has_hdr_support_;
  }
  float GetDiagonalSizeInInches() {
    Refresh();
    return diagonal_size_in_inches_;
  }
private:
  void Refresh();
  void OnUpdated(const Core::JSON::String&);

  ServiceLink display_info_;
  ResolutionInfo resolution_info_ { };
  bool has_hdr_support_ { false };
  float diagonal_size_in_inches_ { 0.f };
  ::starboard::atomic_bool needs_refresh_ { true };
  ::starboard::atomic_bool did_subscribe_ { false };
};

DisplayInfo::Impl::Impl()
  : display_info_(kDisplayInfoCallsign) {
  Refresh();
}

DisplayInfo::Impl::~Impl() {
  display_info_.Unsubscribe(kDefaultTimeoutMs, "updated");
}

void DisplayInfo::Impl::Refresh() {
  if (!needs_refresh_.load())
    return;

  uint32_t rc;

  if (!did_subscribe_.load()) {
    bool old_val = did_subscribe_.exchange(true);
    if (old_val == false) {
      rc = display_info_.Subscribe<Core::JSON::String>(kDefaultTimeoutMs, "updated", &DisplayInfo::Impl::OnUpdated, this);
      if (Core::ERROR_UNAVAILABLE == rc || kPriviligedRequestErrorCode == rc) {
        needs_refresh_.store(false);
        SB_LOG(ERROR) << "Failed to subscribe to '" << kDisplayInfoCallsign
                      << ".updated' event, rc=" << rc
                      << " ( " << Core::ErrorToString(rc) << " )";
        return;
      }
      if (Core::ERROR_NONE != rc) {
        did_subscribe_.store(false);
        SB_LOG(ERROR) << "Failed to subscribe to '" << kDisplayInfoCallsign
                      << ".updated' event, rc=" << rc
                      << " ( " << Core::ErrorToString(rc) << " )."
                      << " Going to try again next time.";
        return;
      }
    }
  }

  needs_refresh_.store(false);

  Core::JSON::String resolution;
  rc = ServiceLink(kPlayerInfoCallsign).Get(kDefaultTimeoutMs, "resolution", resolution);
  if (Core::ERROR_NONE == rc && resolution.IsSet()) {
    if (resolution.Value().find("Resolution2160") != std::string::npos) {
      resolution_info_ = ResolutionInfo { 3840 , 2160 };
    } else {
      resolution_info_ = ResolutionInfo { 1920 , 1080 };
    }
  } else {
    resolution_info_ = ResolutionInfo { 1920 , 1080 };
    SB_LOG(ERROR) << "Failed to get 'resolution', rc=" << rc << " ( " << Core::ErrorToString(rc) << " )";
  }

  Core::JSON::DecUInt16 widthincentimeters, heightincentimeters;
  rc = display_info_.Get(kDefaultTimeoutMs, "widthincentimeters", widthincentimeters);
  if (Core::ERROR_NONE != rc) {
    widthincentimeters.Clear();
    SB_LOG(ERROR) << "Failed to get 'DisplayInfo.widthincentimeters', rc=" << rc << " ( " << Core::ErrorToString(rc) << " )";
  }

  rc = display_info_.Get(kDefaultTimeoutMs, "heightincentimeters", heightincentimeters);
  if (Core::ERROR_NONE != rc) {
    heightincentimeters.Clear();
    SB_LOG(ERROR) << "Failed to get 'DisplayInfo.heightincentimeters', rc=" << rc << " ( " << Core::ErrorToString(rc) << " )";
  }

  if (widthincentimeters && heightincentimeters) {
    diagonal_size_in_inches_ = sqrtf(powf(widthincentimeters, 2) + powf(heightincentimeters, 2)) / 2.54f;
  } else {
    diagonal_size_in_inches_ = 0.f;
  }

  auto detectHdr10Support = [&]()
  {
    using Caps = Core::JSON::ArrayType<Core::JSON::EnumType<Exchange::IHDRProperties::HDRType>>;

    Caps tvcapabilities;
    rc = display_info_.Get(kDefaultTimeoutMs, "tvcapabilities", tvcapabilities);
    if (Core::ERROR_NONE != rc) {
      SB_LOG(ERROR) << "Failed to get 'tvcapabilities', rc=" << rc << " ( " << Core::ErrorToString(rc) << " )";
      return false;
    }

    bool tvHasHDR10 = false;
    {
      Caps::Iterator index(tvcapabilities.Elements());
      while (index.Next() && !tvHasHDR10)
        tvHasHDR10 = (index.Current() == Exchange::IHDRProperties::HDR_10);
    }
    if (false == tvHasHDR10) {
      SB_LOG(INFO) << "No HDR10 in TV caps";
      return false;
    }

    Caps stbcapabilities;
    rc = display_info_.Get(kDefaultTimeoutMs, "stbcapabilities", stbcapabilities);
    if (Core::ERROR_NONE != rc) {
      SB_LOG(ERROR) << "Failed to get 'stbcapabilities', rc=" << rc << " ( " << Core::ErrorToString(rc) << " )";
      return false;
    }

    bool stbHasHDR10 = false;
    {
      Caps::Iterator index(stbcapabilities.Elements());
      while (index.Next() == true && stbHasHDR10 == false)
        stbHasHDR10 = (index.Current() == Exchange::IHDRProperties::HDR_10);
    }
    if (false == stbHasHDR10) {
      SB_LOG(INFO) << "No HDR10 in STB caps";
      return false;
    }

    return stbHasHDR10;
  };

  has_hdr_support_ = detectHdr10Support();

  SB_LOG(INFO) << "Display info updated, resolution: "
               << resolution_info_.Width
               << 'x'
               << resolution_info_.Height
               << ", has hdr: "
               << (has_hdr_support_ ? "yes" : "no")
               << ", diagonal size in inches: "
               << diagonal_size_in_inches_;
}

void DisplayInfo::Impl::OnUpdated(const Core::JSON::String&) {
  if (needs_refresh_.load() == false) {
    needs_refresh_.store(true);
    SbEventSchedule([](void* data) {
      Application::Get()->DisplayInfoChanged();
    }, nullptr, 0);
  }
}

DisplayInfo::DisplayInfo() : impl_(new Impl) {
}

DisplayInfo::~DisplayInfo() {
}

ResolutionInfo DisplayInfo::GetResolution() const {
  return impl_->GetResolution();
}

float DisplayInfo::GetDiagonalSizeInInches() const {
  return impl_->GetDiagonalSizeInInches();
}

bool DisplayInfo::HasHDRSupport() const {
  return impl_->HasHDRSupport();
}

std::string DeviceIdentification::GetChipset() {
  return GetDeviceIdImpl()->chipset;
}

std::string DeviceIdentification::GetFirmwareVersion() {
  return GetDeviceIdImpl()->firmware_version;
}

bool NetworkInfo::IsConnectionTypeWireless() {
  JsonObject data;
  uint32_t rc = ServiceLink(kNetworkCallsign).Get(kDefaultTimeoutMs, "getDefaultInterface", data);
  if (Core::ERROR_NONE == rc) {
    std::string connection_type = data.Get("interface").Value();
    SB_LOG(INFO) << "ConnectionType: " << connection_type;
    return (0 == connection_type.compare("WIFI"));
  }
  SB_LOG(INFO) << "Failed to get default interface, rc: " << rc;
  return false;
}

void TextToSpeech::Speak(const std::string& text) {
  GetTextToSpeech()->Speak(text);
}

bool TextToSpeech::IsEnabled() {
  return GetTextToSpeech()->IsEnabled();
}

void TextToSpeech::Cancel() {
  GetTextToSpeech()->Cancel();
}

bool Accessibility::GetCaptionSettings(SbAccessibilityCaptionSettings* out) {
  return GetAccessibility()->GetCaptionSettings(out);
}

bool Accessibility::GetDisplaySettings(SbAccessibilityDisplaySettings* out) {
  return GetAccessibility()->GetDisplaySettings(out);
}

void Accessibility::SetSettings(const std::string& json) {
  GetAccessibility()->SetSettings(json);
}

bool Accessibility::GetSettings(std::string& out_json) {
  return GetAccessibility()->GetSettings(out_json);
}

void SystemProperties::SetSettings(const std::string& json) {
  GetSystemProperties()->SetSettings(json);
}

bool SystemProperties::GetSettings(std::string& out_json) {
  return GetSystemProperties()->GetSettings(out_json);
}

bool SystemProperties::GetChipset(std::string &out) {
  return GetSystemProperties()->GetChipset(out);
}

bool SystemProperties::GetFirmwareVersion(std::string &out) {
  return GetSystemProperties()->GetFirmwareVersion(out);
}

bool SystemProperties::GetIntegratorName(std::string &out) {
  return GetSystemProperties()->GetIntegratorName(out);
}

bool SystemProperties::GetBrandName(std::string &out) {
  return GetSystemProperties()->GetBrandName(out);
}

bool SystemProperties::GetModelName(std::string &out) {
  return GetSystemProperties()->GetModelName(out);
}

bool SystemProperties::GetModelYear(std::string &out) {
  return GetSystemProperties()->GetModelYear(out);
}

bool SystemProperties::GetFriendlyName(std::string &out) {
  return GetSystemProperties()->GetFriendlyName(out);
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

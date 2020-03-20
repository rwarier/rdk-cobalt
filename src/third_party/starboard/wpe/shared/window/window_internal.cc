// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/starboard/wpe/shared/window/window_internal.h"

#include <linux/input.h>

#include <algorithm>
#include <memory>

#include "starboard/common/log.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/once.h"
#include "third_party/starboard/wpe/shared/application_wpe.h"

namespace third_party {
namespace starboard {
namespace wpe {
namespace shared {
namespace window {

namespace {

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;

// YouTube Technical Requirement 2018 (2016/11/1 - Initial draft)
// 9.5 The device MUST dispatch the following key events, as appropriate:
//  * Window.keydown
//      * After a key is held down for 500ms, the Window.keydown event
//        MUST repeat every 50ms until a user stops holding the key down.
//  * Window.keyup
constexpr SbTime kKeyHoldTime = 500 * kSbTimeMillisecond;
constexpr SbTime kKeyRepeatTime = 50 * kSbTimeMillisecond;

#define KEY_INFO_BUTTON 0xbc

// Converts an input_event code into an SbKey.
SbKey KeyCodeToSbKey(uint16_t code) {
  switch (code) {
    case KEY_BACKSPACE:
      return kSbKeyBack;
    case KEY_DELETE:
      return kSbKeyDelete;
    case KEY_TAB:
      return kSbKeyTab;
    case KEY_LINEFEED:
    case KEY_ENTER:
    case KEY_KPENTER:
      return kSbKeyReturn;
    case KEY_CLEAR:
      return kSbKeyClear;
    case KEY_SPACE:
      return kSbKeySpace;
    case KEY_HOME:
      return kSbKeyHome;
    case KEY_END:
      return kSbKeyEnd;
    case KEY_PAGEUP:
      return kSbKeyPrior;
    case KEY_PAGEDOWN:
      return kSbKeyNext;
    case KEY_LEFT:
      return kSbKeyLeft;
    case KEY_RIGHT:
      return kSbKeyRight;
    case KEY_DOWN:
      return kSbKeyDown;
    case KEY_UP:
      return kSbKeyUp;
    case KEY_ESC:
      return kSbKeyEscape;
    case KEY_KATAKANA:
    case KEY_HIRAGANA:
    case KEY_KATAKANAHIRAGANA:
      return kSbKeyKana;
    case KEY_HANGEUL:
      return kSbKeyHangul;
    case KEY_HANJA:
      return kSbKeyHanja;
    case KEY_HENKAN:
      return kSbKeyConvert;
    case KEY_MUHENKAN:
      return kSbKeyNonconvert;
    case KEY_ZENKAKUHANKAKU:
      return kSbKeyDbeDbcschar;
    case KEY_A:
      return kSbKeyA;
    case KEY_B:
      return kSbKeyB;
    case KEY_C:
      return kSbKeyC;
    case KEY_D:
      return kSbKeyD;
    case KEY_E:
      return kSbKeyE;
    case KEY_F:
      return kSbKeyF;
    case KEY_G:
      return kSbKeyG;
    case KEY_H:
      return kSbKeyH;
    case KEY_I:
      return kSbKeyI;
    case KEY_J:
      return kSbKeyJ;
    case KEY_K:
      return kSbKeyK;
    case KEY_L:
      return kSbKeyL;
    case KEY_M:
      return kSbKeyM;
    case KEY_N:
      return kSbKeyN;
    case KEY_O:
      return kSbKeyO;
    case KEY_P:
      return kSbKeyP;
    case KEY_Q:
      return kSbKeyQ;
    case KEY_R:
      return kSbKeyR;
    case KEY_S:
      return kSbKeyS;
    case KEY_T:
      return kSbKeyT;
    case KEY_U:
      return kSbKeyU;
    case KEY_V:
      return kSbKeyV;
    case KEY_W:
      return kSbKeyW;
    case KEY_X:
      return kSbKeyX;
    case KEY_Y:
      return kSbKeyY;
    case KEY_Z:
      return kSbKeyZ;

    case KEY_0:
      return kSbKey0;
    case KEY_1:
      return kSbKey1;
    case KEY_2:
      return kSbKey2;
    case KEY_3:
      return kSbKey3;
    case KEY_4:
      return kSbKey4;
    case KEY_5:
      return kSbKey5;
    case KEY_6:
      return kSbKey6;
    case KEY_7:
      return kSbKey7;
    case KEY_8:
      return kSbKey8;
    case KEY_9:
      return kSbKey9;

    case KEY_NUMERIC_0:
    case KEY_NUMERIC_1:
    case KEY_NUMERIC_2:
    case KEY_NUMERIC_3:
    case KEY_NUMERIC_4:
    case KEY_NUMERIC_5:
    case KEY_NUMERIC_6:
    case KEY_NUMERIC_7:
    case KEY_NUMERIC_8:
    case KEY_NUMERIC_9:
      return static_cast<SbKey>(kSbKey0 + (code - KEY_NUMERIC_0));

    case KEY_KP0:
      return kSbKeyNumpad0;
    case KEY_KP1:
      return kSbKeyNumpad1;
    case KEY_KP2:
      return kSbKeyNumpad2;
    case KEY_KP3:
      return kSbKeyNumpad3;
    case KEY_KP4:
      return kSbKeyNumpad4;
    case KEY_KP5:
      return kSbKeyNumpad5;
    case KEY_KP6:
      return kSbKeyNumpad6;
    case KEY_KP7:
      return kSbKeyNumpad7;
    case KEY_KP8:
      return kSbKeyNumpad8;
    case KEY_KP9:
      return kSbKeyNumpad9;

    case KEY_KPASTERISK:
      return kSbKeyMultiply;
    case KEY_KPDOT:
      return kSbKeyDecimal;
    case KEY_KPSLASH:
      return kSbKeyDivide;
    case KEY_KPPLUS:
    case KEY_EQUAL:
      return kSbKeyOemPlus;
    case KEY_COMMA:
      return kSbKeyOemComma;
    case KEY_KPMINUS:
    case KEY_MINUS:
      return kSbKeyOemMinus;
    case KEY_DOT:
      return kSbKeyOemPeriod;
    case KEY_SEMICOLON:
      return kSbKeyOem1;
    case KEY_SLASH:
      return kSbKeyOem2;
    case KEY_GRAVE:
      return kSbKeyOem3;
    case KEY_LEFTBRACE:
      return kSbKeyOem4;
    case KEY_BACKSLASH:
      return kSbKeyOem5;
    case KEY_RIGHTBRACE:
      return kSbKeyOem6;
    case KEY_APOSTROPHE:
      return kSbKeyOem7;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return kSbKeyShift;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return kSbKeyControl;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return kSbKeyMenu;
    case KEY_PAUSE:
      return kSbKeyPause;
    case KEY_CAPSLOCK:
      return kSbKeyCapital;
    case KEY_NUMLOCK:
      return kSbKeyNumlock;
    case KEY_SCROLLLOCK:
      return kSbKeyScroll;
    case KEY_SELECT:
      return kSbKeySelect;
    case KEY_PRINT:
      return kSbKeyPrint;
    case KEY_INSERT:
      return kSbKeyInsert;
    case KEY_HELP:
      return kSbKeyHelp;
    case KEY_MENU:
      return kSbKeyApps;
    case KEY_FN_F1:
    case KEY_FN_F2:
    case KEY_FN_F3:
    case KEY_FN_F4:
    case KEY_FN_F5:
    case KEY_FN_F6:
    case KEY_FN_F7:
    case KEY_FN_F8:
    case KEY_FN_F9:
    case KEY_FN_F10:
    case KEY_FN_F11:
    case KEY_FN_F12:
      return static_cast<SbKey>(kSbKeyF1 + (code - KEY_FN_F1));

    // For supporting multimedia buttons on a USB keyboard.
    case KEY_BACK:
      return kSbKeyBrowserBack;
    case KEY_FORWARD:
      return kSbKeyBrowserForward;
    case KEY_REFRESH:
      return kSbKeyBrowserRefresh;
    case KEY_STOP:
      return kSbKeyBrowserStop;
    case KEY_SEARCH:
      return kSbKeyBrowserSearch;
    case KEY_FAVORITES:
      return kSbKeyBrowserFavorites;
    case KEY_HOMEPAGE:
      return kSbKeyBrowserHome;
    case KEY_MUTE:
      return kSbKeyVolumeMute;
    case KEY_VOLUMEDOWN:
      return kSbKeyVolumeDown;
    case KEY_VOLUMEUP:
      return kSbKeyVolumeUp;
    case KEY_NEXTSONG:
      return kSbKeyMediaNextTrack;
    case KEY_PREVIOUSSONG:
      return kSbKeyMediaPrevTrack;
    case KEY_STOPCD:
      return kSbKeyMediaStop;
    case KEY_PLAYPAUSE:
      return kSbKeyMediaPlayPause;
    case KEY_MAIL:
      return kSbKeyMediaLaunchMail;
    case KEY_CALC:
      return kSbKeyMediaLaunchApp2;
    case KEY_WLAN:
      return kSbKeyWlan;
    case KEY_POWER:
      return kSbKeyPower;
    case KEY_BRIGHTNESSDOWN:
      return kSbKeyBrightnessDown;
    case KEY_BRIGHTNESSUP:
      return kSbKeyBrightnessUp;

    case KEY_INFO_BUTTON:
      return kSbKeyF1;

    case KEY_REWIND:
      return kSbKeyMediaRewind;
    case KEY_FASTFORWARD:
      return kSbKeyMediaFastForward;

    case KEY_RED:
      return kSbKeyRed;
    case KEY_GREEN:
      return kSbKeyGreen;
    case KEY_YELLOW:
      return kSbKeyYellow;
    case KEY_BLUE:
      return kSbKeyBlue;

  }
  SB_DLOG(WARNING) << "Unknown code: 0x" << std::hex << code;
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

// Get a SbKeyLocation from an input_event.code.
SbKeyLocation KeyCodeToSbKeyLocation(uint16_t code) {
  switch (code) {
    case KEY_LEFTALT:
    case KEY_LEFTCTRL:
    case KEY_LEFTMETA:
    case KEY_LEFTSHIFT:
      return kSbKeyLocationLeft;
    case KEY_RIGHTALT:
    case KEY_RIGHTCTRL:
    case KEY_RIGHTMETA:
    case KEY_RIGHTSHIFT:
      return kSbKeyLocationRight;
  }

  return kSbKeyLocationUnspecified;
}

SbKeyModifiers KeyCodeToSbKeyModifiers(uint16_t code) {
  switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return kSbKeyModifiersCtrl;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return kSbKeyModifiersShift;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return kSbKeyModifiersAlt;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return kSbKeyModifiersMeta;
  }
  return kSbKeyModifiersNone;
}

}  // namespace

class EssInput
{
public:
  void SetSbWindow(SbWindow window) { window_ = window; }
  SbWindow GetSbWindow() const { return window_; }
  void OnKeyPressed(unsigned int key);
  void OnKeyReleased(unsigned int key);

private:
  void CreateKey(unsigned int key, SbInputEventType type, bool repeatable);
  void CreateRepeatKey();
  void DeleteRepeatKey();
  void OnKeyboardHandleKey(unsigned int key, SbInputEventType type);
  bool UpdateModifiers(unsigned int key, SbInputEventType type);

  unsigned int key_modifiers_ { 0 };
  SbWindow window_ { kSbWindowInvalid };

  unsigned key_repeat_key_ { 0 };
  int key_repeat_state_ { 0 };
  SbEventId key_repeat_event_id_ { kSbEventIdInvalid };
  SbTime key_repeat_interval_ { kKeyHoldTime };
};

void EssInput::CreateKey(unsigned int key, SbInputEventType type, bool repeatable) {
  unsigned int modifiers = key_modifiers_;
  if (modifiers == kSbKeyModifiersCtrl) {  // only Ctrl is set
    switch(key) {
      case KEY_L: key = KEY_BACKSPACE; modifiers = 0; break;
      case KEY_F: key = KEY_FASTFORWARD; modifiers = 0; break;
      case KEY_W: key = KEY_REWIND; modifiers = 0; break;
      case KEY_P: key = KEY_PLAYPAUSE; modifiers = 0; break;
      case KEY_0: key = KEY_RED; modifiers = 0; break;
      case KEY_1: key = KEY_GREEN; modifiers = 0; break;
      case KEY_2: key = KEY_YELLOW; modifiers = 0; break;
      case KEY_3: key = KEY_BLUE; modifiers = 0; break;
      default: break;
    }
  }

  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));
  data->timestamp = SbTimeGetMonotonicNow();
  data->window = GetSbWindow();
  data->type = type;
  data->device_type = kSbInputDeviceTypeRemote;
  data->device_id = 1;  // kKeyboardDeviceId;
  data->key = KeyCodeToSbKey(key);
  data->key_location = KeyCodeToSbKeyLocation(key);
  data->key_modifiers = modifiers;

  Application::Get()->InjectInputEvent(data);

  DeleteRepeatKey();

  if (repeatable && type == kSbInputEventTypePress) {
    key_repeat_key_ = key;
    key_repeat_state_ = 1;
    key_repeat_event_id_ = SbEventSchedule(
      [](void* data) {
        EssInput* ess_input = reinterpret_cast<EssInput*>(data);
        ess_input->CreateRepeatKey();
      },
      this, key_repeat_interval_);
  } else {
    key_repeat_interval_ = kKeyHoldTime;
  }
}

void EssInput::CreateRepeatKey() {
  if (!key_repeat_state_) {
    return;
  }
  if (key_repeat_interval_) {
    key_repeat_interval_ = kKeyRepeatTime;
  }
  CreateKey(key_repeat_key_, kSbInputEventTypePress, true);
}

void EssInput::DeleteRepeatKey() {
  key_repeat_state_ = 0;
  if (key_repeat_event_id_ != kSbEventIdInvalid) {
    SbEventCancel(key_repeat_event_id_);
    key_repeat_event_id_ = kSbEventIdInvalid;
  }
}

void EssInput::OnKeyboardHandleKey(unsigned int key, SbInputEventType type) {
  if (UpdateModifiers(key, type))
    return;

  bool repeatable =
    (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) ||
    ((key == KEY_F || key == KEY_W) && (key_modifiers_ == kSbKeyModifiersCtrl));

  if (type == kSbInputEventTypePress && repeatable && key == key_repeat_key_ && key_repeat_state_)
    return;

  if (repeatable) {
    CreateKey(key, type, true);
  } else {
    CreateKey(key, type, false);
  }
}

bool EssInput::UpdateModifiers(unsigned int key, SbInputEventType type) {
  SbKeyModifiers modifiers = KeyCodeToSbKeyModifiers(key);
  if (modifiers != kSbKeyModifiersNone) {
    if (type == kSbInputEventTypePress)
      key_modifiers_ |= modifiers;
    else
      key_modifiers_ &= ~modifiers;
    return true;
  }
  return false;
}

void EssInput::OnKeyPressed(unsigned int key) {
  OnKeyboardHandleKey(key, kSbInputEventTypePress);
}

void EssInput::OnKeyReleased(unsigned int key) {
  OnKeyboardHandleKey(key, kSbInputEventTypeUnpress);
}

class EssCtxWrapper {
public:
  EssCtxWrapper();
  ~EssCtxWrapper();

  EssCtx *GetCtx() const { return ctx_; }
  EssInput *GetEssInput() const { return input_handler_.get(); }
  void SetSbWindow(SbWindow window) { input_handler_->SetSbWindow(window); }
  int Width() const { return display_width_; }
  int Height() const { return display_height_; }
  void* NativeWindow() const { return native_window_; }
  void ResizeNativeWindow(int width, int height);

private:
  void OnTerminated();
  void OnKeyPressed(unsigned int key);
  void OnKeyReleased(unsigned int key);
  void OnDisplaySize(int width, int height);

  static EssTerminateListener terminateListener;
  static EssKeyListener keyListener;
  static EssSettingsListener settingsListener;

  EssCtx *ctx_ { nullptr };
  std::unique_ptr<EssInput> input_handler_ { nullptr };
  NativeWindowType native_window_ { 0 };
  int display_width_ {0};
  int display_height_ {0};
};

EssTerminateListener EssCtxWrapper::terminateListener = {
  //terminated
  [](void* data) { reinterpret_cast<EssCtxWrapper*>(data)->OnTerminated(); }
};

EssKeyListener EssCtxWrapper::keyListener = {
  // keyPressed
  [](void* data, unsigned int key) { reinterpret_cast<EssCtxWrapper*>(data)->OnKeyPressed(key); },
  // keyReleased
  [](void* data, unsigned int key) { reinterpret_cast<EssCtxWrapper*>(data)->OnKeyReleased(key); }
};

EssSettingsListener EssCtxWrapper::settingsListener = {
  // displaySize
  [](void *data, int width, int height ) { reinterpret_cast<EssCtxWrapper*>(data)->OnDisplaySize(width, height); },
  // displaySafeArea
  nullptr
};

void EssCtxWrapper::OnTerminated() {
  Application::Get()->Stop(0);
}

void EssCtxWrapper::OnKeyPressed(unsigned int key) {
  GetEssInput()->OnKeyPressed(key);
}

void EssCtxWrapper::OnKeyReleased(unsigned int key) {
  GetEssInput()->OnKeyReleased(key);
}

void EssCtxWrapper::OnDisplaySize(int width, int height) {
  ResizeNativeWindow(width, height);
}

void EssCtxWrapper::ResizeNativeWindow(int width, int height) {
  if (display_width_ == width && display_height_ == height)
    return;

  display_width_ = width;
  display_height_ = height;

  EssContextResizeWindow(ctx_, display_width_, display_height_);
}

EssCtxWrapper::EssCtxWrapper() : input_handler_(new EssInput) {
  bool error = false;
  ctx_ = EssContextCreate();

  if ( !EssContextInit(ctx_) ) {
    error = true;
  }
  else if ( !EssContextSetTerminateListener(ctx_, this, &terminateListener) ) {
    error = true;
  }
  else if ( !EssContextSetKeyListener(ctx_, this, &keyListener) ) {
    error = true;
  }
  else if ( !EssContextSetSettingsListener(ctx_, this, &settingsListener) ) {
    error = true;
  }
  else if ( !EssContextSetKeyRepeatInitialDelay(ctx_, INT_MAX) )  {
    error = true;
  }
  else if ( !EssContextSetKeyRepeatPeriod(ctx_, INT_MAX) ) {
    error = true;
  }
  else if ( !EssContextGetDisplaySize(ctx_, &display_width_, &display_height_) ) {
    error= true;
  }
  else if ( !EssContextSetInitialWindowSize(ctx_, display_width_, display_height_) ) {
    error = true;
  }
  else if ( !EssContextCreateNativeWindow(ctx_, display_width_, display_height_, &native_window_) ) {
    error = true;
  }
  else if ( !EssContextStart(ctx_) ) {
    error = true;
  }

  if ( error ) {
    const char *detail= EssContextGetLastErrorDetail(ctx_);
    fprintf(stderr, "Essos error: (%s)\n", detail);
  }
}

EssCtxWrapper::~EssCtxWrapper() {
  EssContextDestroy(ctx_);
}

SB_ONCE_INITIALIZE_FUNCTION(EssCtxWrapper, GetEssCtxWrapper);

EssCtx *GetEssCtx() {
  return GetEssCtxWrapper()->GetCtx();
}

}  // namespace window
}  // namespace shared
}  // namespace wpe
}  // namespace starboard
}  // namespace third_party

using namespace third_party::starboard::wpe::shared;

SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* options) {
  auto width = options && options->size.width > 0
    ? options->size.width
    : window::kDefaultWidth;

  auto height = options && options->size.height > 0
    ? options->size.height
    : window::kDefaultHeight;

  auto* env_width = std::getenv("COBALT_RESOLUTION_WIDTH");
  if ( env_width ) {
    width = atoi(env_width);
  }

  auto* env_height = std::getenv("COBALT_RESOLUTION_HEIGHT");
  if ( env_height ) {
    height = atoi(env_height);
  }

  window::GetEssCtxWrapper()->ResizeNativeWindow(width, height);
  window::GetEssCtxWrapper()->SetSbWindow(this);
}

SbWindowPrivate::~SbWindowPrivate() {
  window::GetEssCtxWrapper()->SetSbWindow(nullptr);
}

void* SbWindowPrivate::Native() const {
  return window::GetEssCtxWrapper()->NativeWindow();
}

int SbWindowPrivate::Width() const {
  return window::GetEssCtxWrapper()->Width();
}

int SbWindowPrivate::Height() const {
  return window::GetEssCtxWrapper()->Height();
}

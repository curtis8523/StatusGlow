// Lightweight NeoPixel effects engine used by the firmware.

#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

enum EffectMode : uint16_t {
  FX_MODE_STATIC = 0,
  FX_MODE_BREATH = 1,
  FX_MODE_COLOR_WIPE = 2,
  FX_MODE_THEATER_CHASE = 3,
  FX_MODE_SCAN = 4,
  FX_MODE_BLINK = 5,
  FX_MODE_FADE = 6,
  FX_MODE_RAINBOW = 7,
  FX_MODE_RAINBOW_CYCLE = 8,
  FX_MODE_COMET = 9,
  FX_MODE_RUNNING_LIGHTS = 10,
  FX_MODE_DUAL_SCAN = 11,
  FX_MODE_TWINKLE = 12,
  FX_MODE_SPARKLE = 13,
  FX_MODE_CONFETTI = 14,
  FX_MODE_FIRE_FLICKER = 15,
  FX_MODE_COLOR_WIPE_INVERSE = 16,
  FX_MODE_COLOR_WIPE_RANDOM = 17,
  FX_MODE_FILLER_UP = 18,
};

#ifndef BLACK
#define BLACK 0x000000
#endif
#ifndef WHITE
#define WHITE 0xFFFFFF
#endif
#ifndef RED
#define RED 0xFF0000
#endif
#ifndef GREEN
#define GREEN 0x00FF00
#endif
#ifndef BLUE
#define BLUE 0x0000FF
#endif
#ifndef YELLOW
#define YELLOW 0xFFFF00
#endif
#ifndef ORANGE
#define ORANGE 0xFFA500
#endif
#ifndef PURPLE
#define PURPLE 0x800080
#endif
#ifndef PINK
#define PINK 0xFF1493
#endif

class LedEffects {
public:
  LedEffects(uint16_t count, uint8_t pin, neoPixelType type)
  : strip(count, pin, type) {
    _count = count;
  }

  ~LedEffects() {
    if (_aux) { delete [] _aux; _aux = nullptr; }
  }

  void init() {
    strip.begin();
    strip.setBrightness(255);
    strip.clear();
    strip.show();
  }

  void start() { /* no-op for NeoPixel */ }

  void setLength(uint16_t n) {
    _count = n;
    strip.updateLength(n);
    strip.setBrightness(255);
    strip.clear();
    strip.show();
    _needsRefresh = true;
    resizeAux();
  }

  void setPixelType(bool isRGBW) {
    _isRGBW = isRGBW;
    neoPixelType type = isRGBW ? (NEO_GRBW + NEO_KHZ800) : (NEO_GRB + NEO_KHZ800);
    strip.updateType(type);
    strip.setBrightness(255);
    strip.clear();
    strip.show();
    _needsRefresh = true;
  }

  bool getPixelTypeRGBW() const { return _isRGBW; }

  uint16_t length() const { return _count; }

  void setBrightness(uint8_t b) {
    if (_bri == b) return;
    _bri = b;
    _needsRefresh = true;
  }
  uint8_t getBrightness() const { return _bri; }

  void setGamma(float gamma) {
    if (isnan(gamma) || gamma < 0.1f) gamma = 0.1f;
    if (gamma > 5.0f) gamma = 5.0f;
    if (fabsf(_gamma - gamma) < 0.001f) return;
    _gamma = gamma;
    _needsRefresh = true;
  }
  float getGamma() const { return _gamma; }

  void setSegment(uint8_t /*segment*/, uint16_t start, uint16_t end, uint16_t mode, uint32_t color, uint16_t speed, bool reverse) {
    if (end > _count) end = _count;
    _p_segStart = start; _p_segEnd = end;
    _p_mode = (EffectMode)mode; _p_color = color; _p_speed = speed; _p_reverse = reverse;
    _hasPending = true;
  }

  void trigger() { /* compatibility no-op; pending config is applied in service() */ }

  void service() {
    if (_hasPending) {
      _segStart = _p_segStart; _segEnd = _p_segEnd;
      _mode = _p_mode; _color = _p_color; _speed = _p_speed; _reverse = _p_reverse;
      _startedMs = millis(); _lastFrameMs = 0; _pos = 0; _dir = 1; _phase = 0;
      // Reset per-mode state on (re)apply
      if (_mode == FX_MODE_FILLER_UP) {
        _fillerFill = 0;            // number of filled LEDs
        _fillerDropAccum = 0.0f;     // distance traveled within current region
        _fillerFilling = true;       // currently filling (true) or un-filling (false)
        _fillerLastMs = _startedMs;  // seed timing
      }
      _hasPending = false;
      renderFrame(true);
      return;
    }
    renderFrame(false);
  }

  uint16_t getModeCount() const { return 19; }
  const char* getModeName(uint16_t id) const {
    switch ((EffectMode)id) {
      case FX_MODE_STATIC: return "Static";
      case FX_MODE_BREATH: return "Breath";
      case FX_MODE_COLOR_WIPE: return "Color Wipe";
      case FX_MODE_THEATER_CHASE: return "Theater Chase";
      case FX_MODE_SCAN: return "Scan";
      case FX_MODE_BLINK: return "Blink";
      case FX_MODE_FADE: return "Fade";
      case FX_MODE_RAINBOW: return "Rainbow";
      case FX_MODE_RAINBOW_CYCLE: return "Rainbow Cycle";
      case FX_MODE_COMET: return "Comet";
      case FX_MODE_RUNNING_LIGHTS: return "Running Lights";
      case FX_MODE_DUAL_SCAN: return "Dual Scan";
      case FX_MODE_TWINKLE: return "Twinkle";
      case FX_MODE_SPARKLE: return "Sparkle";
      case FX_MODE_CONFETTI: return "Confetti";
      case FX_MODE_FIRE_FLICKER: return "Fire Flicker";
      case FX_MODE_COLOR_WIPE_INVERSE: return "Color Wipe Inverse";
      case FX_MODE_COLOR_WIPE_RANDOM: return "Color Wipe Random";
      case FX_MODE_FILLER_UP: return "Filler Up";
      default: return "Unknown";
    }
  }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    // Always use RGB color format (RGBW compatibility handled by strip type)
    return Adafruit_NeoPixel::Color(r, g, b);
  }
  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // RGBW color format
    return Adafruit_NeoPixel::Color(r, g, b, w);
  }

  Adafruit_NeoPixel strip;

private:
  uint16_t _count = 0;
  uint8_t _bri = 255;
  bool _isRGBW = false;          // Track current LED type (RGB vs RGBW)
  uint16_t _segStart = 0, _segEnd = 0;
  EffectMode _mode = FX_MODE_STATIC;
  uint32_t _color = WHITE;
  uint16_t _speed = 3000;
  bool _reverse = false;
  uint16_t _p_segStart = 0, _p_segEnd = 0;
  EffectMode _p_mode = FX_MODE_STATIC;
  uint32_t _p_color = WHITE;
  uint16_t _p_speed = 3000;
  bool _p_reverse = false;
  bool _hasPending = false;
  bool _needsRefresh = true;
  float _gamma = 2.2f;
  unsigned long _startedMs = 0;
  unsigned long _lastFrameMs = 0;
  int _pos = 0; int _dir = 1; int _phase = 0;
  uint8_t* _aux = nullptr;
  uint8_t _wipeIndex = 0;
  uint32_t _wipeColor = WHITE;
  // Filler Up state
  uint16_t _fillerFill = 0;      // current filled height (0..segLen)
  float _fillerDropAccum = 0.0f; // accumulated drop distance within current region (in LEDs)
  bool _fillerFilling = true;    // true when filling, false when un-filling
  unsigned long _fillerLastMs = 0; // last timestamp for drop advancement

  inline uint16_t segLen() const { return (_segEnd > _segStart) ? (_segEnd - _segStart) : 0; }

  void clearSeg() {
    for (uint16_t i = _segStart; i < _segEnd; i++) strip.setPixelColor(i, 0);
  }

  void fillSeg(uint32_t c) {
    for (uint16_t i = _segStart; i < _segEnd; i++) setPixelColorScaled(i, c);
  }

  uint32_t scaleColor(uint32_t c, float f) {
    f *= ((float)_bri / 255.0f);
    if (f <= 0.0f) return 0;
    if (f > 1.0f) f = 1.0f;
    float corrected = (_gamma <= 0.101f) ? f : powf(f, _gamma);
    if (corrected < 0.0f) corrected = 0.0f;
    if (corrected > 1.0f) corrected = 1.0f;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    if (_isRGBW) {
      uint8_t w = (c >> 24) & 0xFF;
      return Color((uint8_t)(r * corrected), (uint8_t)(g * corrected), (uint8_t)(b * corrected), (uint8_t)(w * corrected));
    } else {
      return Color((uint8_t)(r * corrected), (uint8_t)(g * corrected), (uint8_t)(b * corrected));
    }
  }

  void resizeAux() {
    if (_aux) { delete [] _aux; _aux = nullptr; }
    if (_count > 0) { _aux = new uint8_t[_count]; memset(_aux, 0, _count); }
  }

  uint32_t wheel(uint8_t pos) const {
    pos = 255 - pos;
    if (pos < 85) {
      return Color(255 - pos * 3, 0, pos * 3);
    }
    if (pos < 170) {
      pos -= 85;
      return Color(0, pos * 3, 255 - pos * 3);
    }
    pos -= 170;
    return Color(pos * 3, 255 - pos * 3, 0);
  }

  void dimAll(uint8_t amount) {
    uint16_t n = segLen();
    for (uint16_t i = 0; i < n; i++) {
      uint16_t p = _segStart + i;
      uint32_t c = strip.getPixelColor(p);
      uint8_t r = (c >> 16) & 0xFF;
      uint8_t g = (c >> 8) & 0xFF;
      uint8_t b = c & 0xFF;
    #ifdef RGBW_STRIP
      uint8_t w = (c >> 24) & 0xFF;
      r = (uint8_t)((r * (255 - amount)) >> 8);
      g = (uint8_t)((g * (255 - amount)) >> 8);
      b = (uint8_t)((b * (255 - amount)) >> 8);
      w = (uint8_t)((w * (255 - amount)) >> 8);
      strip.setPixelColor(p, Color(r,g,b,w));
    #else
      r = (uint8_t)((r * (255 - amount)) >> 8);
      g = (uint8_t)((g * (255 - amount)) >> 8);
      b = (uint8_t)((b * (255 - amount)) >> 8);
      strip.setPixelColor(p, Color(r,g,b));
    #endif
    }
  }

  inline void setPixelColorScaled(uint16_t p, uint32_t c) {
    strip.setPixelColor(p, scaleColor(c, 1.0f));
  }

  inline void setPixelScaled(uint16_t p, float f, uint32_t c) {
    if (p >= _segStart && p < _segEnd) {
      if (f <= 0.0f) return;
      if (f > 1.0f) f = 1.0f;
      uint32_t sc = scaleColor(c, f);
      strip.setPixelColor(p, sc);
    }
  }

  inline void addPixelScaled(uint16_t p, float f, uint32_t c) {
    if (p < _segStart || p >= _segEnd || f <= 0.0f) return;
    uint32_t sc = scaleColor(c, f);
    uint32_t cur = strip.getPixelColor(p);
    uint16_t r = ((cur >> 16) & 0xFF) + ((sc >> 16) & 0xFF);
    uint16_t g = ((cur >> 8) & 0xFF) + ((sc >> 8) & 0xFF);
    uint16_t b = (cur & 0xFF) + (sc & 0xFF);
    if (_isRGBW) {
      uint16_t w = ((cur >> 24) & 0xFF) + ((sc >> 24) & 0xFF);
      strip.setPixelColor(p, Color(min<uint16_t>(r, 255), min<uint16_t>(g, 255), min<uint16_t>(b, 255), min<uint16_t>(w, 255)));
    } else {
      strip.setPixelColor(p, Color(min<uint16_t>(r, 255), min<uint16_t>(g, 255), min<uint16_t>(b, 255)));
    }
  }

  void renderSoftDot(float pos, uint32_t color, float radius, bool additive = true, float intensity = 1.0f) {
    if (radius <= 0.0f || segLen() == 0) return;
    int start = max<int>(0, (int)floorf(pos - radius));
    int end = min<int>((int)segLen() - 1, (int)ceilf(pos + radius));
    for (int i = start; i <= end; i++) {
      float dist = fabsf(pos - (float)i);
      float f = 1.0f - (dist / radius);
      if (f <= 0.0f) continue;
      f = f * f;
      f *= intensity;
      uint16_t p = _segStart + (uint16_t)i;
      if (additive) addPixelScaled(p, f, color);
      else setPixelScaled(p, f, color);
    }
  }

  uint16_t getFrameIntervalMs() const {
    switch (_mode) {
      case FX_MODE_STATIC:
        return 1000;
      case FX_MODE_BLINK:
        return constrain(_speed / 12, 20, 80);
      case FX_MODE_COLOR_WIPE:
      case FX_MODE_COLOR_WIPE_INVERSE:
      case FX_MODE_COLOR_WIPE_RANDOM:
      case FX_MODE_THEATER_CHASE:
      case FX_MODE_FILLER_UP: {
        uint16_t n = max<uint16_t>(segLen(), 1);
        return constrain(_speed / max<uint16_t>(n * 6, 1), 6, 24);
      }
      default:
        return constrain(_speed / 240, 4, 16);
    }
  }

  void renderFrame(bool force) {
    if (!force && _mode == FX_MODE_STATIC && !_needsRefresh) return;
    unsigned long now = millis();
    uint16_t frameMs = getFrameIntervalMs();
    if (!force && (now - _lastFrameMs) < frameMs) return;
    _lastFrameMs = now;
    switch (_mode) {
      case FX_MODE_STATIC: renderStatic(); break;
      case FX_MODE_BREATH: renderBreath(now); break;
      case FX_MODE_COLOR_WIPE: renderColorWipe(now); break;
      case FX_MODE_THEATER_CHASE: renderTheaterChase(now); break;
      case FX_MODE_SCAN: renderScan(now); break;
      case FX_MODE_BLINK: renderBlink(now); break;
      case FX_MODE_FADE: renderFade(now); break;
      case FX_MODE_RAINBOW: renderRainbow(now, false); break;
      case FX_MODE_RAINBOW_CYCLE: renderRainbow(now, true); break;
      case FX_MODE_COMET: renderComet(now); break;
      case FX_MODE_RUNNING_LIGHTS: renderRunningLights(now); break;
      case FX_MODE_DUAL_SCAN: renderDualScan(now); break;
      case FX_MODE_TWINKLE: renderTwinkle(now); break;
      case FX_MODE_SPARKLE: renderSparkle(now); break;
      case FX_MODE_CONFETTI: renderConfetti(now); break;
      case FX_MODE_FIRE_FLICKER: renderFireFlicker(now); break;
      case FX_MODE_COLOR_WIPE_INVERSE: renderColorWipe(now, true); break;
      case FX_MODE_COLOR_WIPE_RANDOM: renderColorWipeRandom(now); break;
      case FX_MODE_FILLER_UP: renderFillerUp(now); break;
      default: renderStatic(); break;
    }
    strip.show();
    _needsRefresh = false;
  }

  void renderStatic() {
    fillSeg(_color);
  }

  void renderBreath(unsigned long now) {
    float period = max<uint16_t>(_speed, 1000);
    float t = (float)((now - _startedMs) % (unsigned long)period) / period;
    float s = (cosf(t * 6.28318f) * -0.5f) + 0.5f;
    float eased = s * s * (3.0f - 2.0f * s);
    float f = 0.14f + (0.86f * eased);
    clearSeg();
    for (uint16_t i = _segStart; i < _segEnd; i++) setPixelScaled(i, f, _color);
  }

  void renderColorWipe(unsigned long now, bool inverse=false) {
    uint16_t n = segLen(); if (n == 0) return;
    unsigned long elapsed = now - _startedMs;
    uint16_t stepMs = max<uint16_t>(_speed / max<uint16_t>(n, 1), 15);
    int idx = (int)(elapsed / stepMs);
    if (inverse ^ _reverse) idx = n - 1 - (idx % (n + 1));
    clearSeg();
    int limit = min<int>(idx, n);
    for (int i = 0; i < limit; i++) {
      uint16_t p = _segStart + ((inverse ^ _reverse) ? (n - 1 - i) : i);
      setPixelColorScaled(p, _color);
    }
  }

  void renderTheaterChase(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    uint16_t stepMs = max<uint16_t>(_speed / 50, 30);
    int offset = (now / stepMs) % 3;
    clearSeg();
    for (uint16_t i = 0; i < n; i++) {
      uint16_t j = _reverse ? (n - 1 - i) : i;
      if ((j + offset) % 3 == 0) {
        setPixelColorScaled(_segStart + i, _color);
      } else if ((j + offset + 1) % 3 == 0) {
        setPixelScaled(_segStart + i, 0.18f, _color);
      }
    }
  }

  void renderScan(unsigned long now) {
    uint16_t n = segLen(); if (n < 1) return;
    clearSeg();
    if (n == 1) { setPixelColorScaled(_segStart, _color); return; }
    float period = (float)max<uint16_t>(_speed, 300);
    float path = (float)(2 * (int)n - 2);
    float t = fmodf(((now - _startedMs) % (unsigned long)period) / period * path, path);
    float pos = (t <= (n - 1)) ? t : (2 * (n - 1) - t);
    if (_reverse) pos = (n - 1) - pos;
    renderSoftDot(pos, _color, 3.4f, false);
  }

  void renderDualScan(unsigned long now) {
    uint16_t n = segLen(); if (n < 1) return;
    clearSeg();
    if (n == 1) { setPixelColorScaled(_segStart, _color); return; }
    float period = (float)max<uint16_t>(_speed, 300);
    float path = (float)(2 * (int)n - 2);
    float t = fmodf(((now - _startedMs) % (unsigned long)period) / period * path, path);
    float pos = (t <= (n - 1)) ? t : (2 * (n - 1) - t);
    float posA = _reverse ? ((n - 1) - pos) : pos;
    float posB = (n - 1) - posA;
    renderSoftDot(posA, _color, 2.8f, true);
    renderSoftDot(posB, _color, 2.8f, true);
  }

  void renderBlink(unsigned long now) {
    unsigned long period = max<uint16_t>(_speed, 300);
    bool on = ((now - _startedMs) % period) < (period / 2);
    fillSeg(on ? _color : 0);
  }

  void renderFade(unsigned long now) {
    float period = max<uint16_t>(_speed, 1000);
    float t = (float)((now - _startedMs) % (unsigned long)period) / period;
    float f = (sinf((t * 2.0f - 1.0f) * 1.5708f) + 1.0f) * 0.5f;
    clearSeg();
    for (uint16_t i = _segStart; i < _segEnd; i++) setPixelScaled(i, f, _color);
  }

  void renderRainbow(unsigned long now, bool cycle) {
    uint16_t n = segLen(); if (n == 0) return;
    uint16_t stepMs = max<uint16_t>(_speed / 100, 5);
    uint32_t offset = (now - _startedMs) / stepMs;
    for (uint16_t i = 0; i < n; i++) {
      uint16_t idx = cycle ? ((i * 256 / n) + offset) & 0xFF : ((i + offset) & 0xFF);
      uint32_t c = wheel(idx);
      setPixelColorScaled(_segStart + (_reverse ? (n - 1 - i) : i), c);
    }
  }

  void renderComet(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    clearSeg();
    float period = (float)max<uint16_t>(_speed, 300);
    float t = ((now - _startedMs) % (unsigned long)period) / period;
    float pos = t * (float)(n - 1);
    if (_reverse) pos = (float)(n - 1) - pos;
    float tailLen = 5.8f;
    for (uint16_t i = 0; i < n; i++) {
      float pixelPos = (float)i;
      float behind = _reverse ? (pixelPos - pos) : (pos - pixelPos);
      if (behind <= 0.0f || behind > tailLen) continue;
      float mix = 1.0f - (behind / tailLen);
      mix = mix * mix * (3.0f - 2.0f * mix);
      addPixelScaled(_segStart + i, 0.48f * mix, _color);
    }
    renderSoftDot(pos, _color, 2.2f, true, 1.0f);
  }

  void renderRunningLights(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    uint16_t period = max<uint16_t>(_speed, 100);
    // Use _speed as the time for one complete wave cycle
    float t = (now - _startedMs) * (6.28318f / (float)period); // 2*PI / period
    for (uint16_t i = 0; i < n; i++) {
      float v = (sinf((i * 0.3f) + t) + 1.0f) * 0.5f;
      uint32_t c = scaleColor(_color, v);
      strip.setPixelColor(_segStart + (_reverse ? (n - 1 - i) : i), c);
    }
  }

  // Filler Up: A drop flies from start to end, collecting color. Each trip is shorter.
  // After all color is collected, it does the same with black. Uses smooth sub-pixel positioning.
  void renderFillerUp(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    uint32_t T = (uint32_t)max<uint16_t>(_speed, 1); // total cycle duration (ms)

    // Determine velocity so that traversing lengths n, n-1, ..., 1 takes T/2 ms
    // v (led/ms) = n(n+1)/T
    float v = ((float)n * (float)(n + 1)) / (float)T;

    // Advance drop accumulator by elapsed time
    unsigned long dt = now - _fillerLastMs;
    _fillerLastMs = now;
    if (dt > 200) dt = 200; // avoid long-jump artifacts
    _fillerDropAccum += v * (float)dt; // in LEDs

    auto regionLen = [&]() -> uint16_t { return (uint16_t)(n - _fillerFill); };

    // Consume full traversals; when drop reaches the boundary, grow collected region
    uint16_t rlen = regionLen();
    if (rlen == 0) {
      // switch phase immediately
      _fillerFilling = !_fillerFilling;
      _fillerFill = 0;
      _fillerDropAccum = 0.0f;
      rlen = regionLen();
    }
    while (rlen > 0 && _fillerDropAccum >= (float)rlen) {
      _fillerDropAccum -= (float)rlen;
      _fillerFill++;
      if (_fillerFill >= n) {
        // completed half-cycle; switch phase
        _fillerFilling = !_fillerFilling;
        _fillerFill = 0;
        _fillerDropAccum = 0.0f;
      }
      rlen = regionLen();
    }

    // Render frame baseline and collected region
    if (_fillerFilling) {
      // Phase 1: collect color at the end; background is black
      fillSeg(BLACK);
      if (_fillerFill > 0) {
        if (!_reverse) {
          for (uint16_t i = 0; i < _fillerFill; i++) {
            setPixelColorScaled(_segStart + (n - 1 - i), _color);
          }
        } else {
          for (uint16_t i = 0; i < _fillerFill; i++) {
            setPixelColorScaled(_segStart + i, _color);
          }
        }
      }
      // Drop flies in across the uncollected region toward the end with smooth interpolation
      if (rlen > 0) {
        float dropPos = min<float>(_fillerDropAccum, (float)(rlen - 1));
        int i0 = (int)floorf(dropPos);
        float frac = dropPos - (float)i0;
        uint16_t p0 = !_reverse ? (_segStart + i0) : (_segStart + (n - 1 - i0));
        uint16_t p1 = !_reverse ? (_segStart + min<int>(i0 + 1, rlen - 1)) : (_segStart + (n - 1 - min<int>(i0 + 1, rlen - 1)));
        setPixelScaled(p0, 1.0f, _color);
        if (p1 != p0) setPixelScaled(p1, frac, _color);
      }
    } else {
      // Phase 2: collect black at the end; background is full color
      fillSeg(_color);
      if (_fillerFill > 0) {
        if (!_reverse) {
          for (uint16_t i = 0; i < _fillerFill; i++) {
            setPixelColorScaled(_segStart + (n - 1 - i), BLACK);
          }
        } else {
          for (uint16_t i = 0; i < _fillerFill; i++) {
            setPixelColorScaled(_segStart + i, BLACK);
          }
        }
      }
      // Black drop flies in across the remaining colored region toward the end with smooth interpolation
      if (rlen > 0) {
        float dropPos = min<float>(_fillerDropAccum, (float)(rlen - 1));
        int i0 = (int)floorf(dropPos);
        float frac = dropPos - (float)i0;
        uint16_t p0 = !_reverse ? (_segStart + i0) : (_segStart + (n - 1 - i0));
        uint16_t p1 = !_reverse ? (_segStart + min<int>(i0 + 1, rlen - 1)) : (_segStart + (n - 1 - min<int>(i0 + 1, rlen - 1)));
        // Create smooth black drop: p1 is fully black (head), p0 fades from color to black
        if (p1 != p0) {
          setPixelScaled(p0, frac, _color); // trailing edge: more black as drop advances
          setPixelColorScaled(p1, BLACK);    // drop head is fully black
        } else {
          setPixelScaled(p0, 1.0f - frac, _color); // single pixel fades to black
        }
      }
    }
  }

  void renderTwinkle(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    dimAll(40);
    // Use speed to control twinkle frequency: higher speed = slower twinkling
    uint16_t period = max<uint16_t>(_speed, 100);
    uint16_t chance = constrain(100000 / period, 1, 100); // slower = less frequent
    if (random(100) < chance) {
      uint16_t i = _segStart + random(n);
      setPixelColorScaled(i, _color);
    }
  }

  void renderSparkle(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    clearSeg();
    // Use speed to control sparkle frequency: higher speed = slower sparkling
    uint16_t period = max<uint16_t>(_speed, 100);
    uint16_t chance = constrain(100000 / period, 1, 100);
    if (random(100) < chance) {
      uint8_t sparks = 1 + (random(100) < 30 ? 1 : 0);
      for (uint8_t s = 0; s < sparks; s++) {
        uint16_t i = _segStart + random(n);
        setPixelColorScaled(i, _color);
      }
    }
  }

  void renderConfetti(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    // Use speed to control confetti spawn rate: higher speed = slower confetti
    uint16_t period = max<uint16_t>(_speed, 100);
    uint16_t dimRate = constrain(200000 / period, 10, 80); // slower = less dimming
    dimAll(dimRate);
    // Spawn probability: faster speed = more confetti per frame
    uint16_t chance = constrain(100000 / period, 5, 100);
    if (random(100) < chance) {
      uint16_t i = _segStart + random(n);
      setPixelColorScaled(i, _color);
    }
  }

  void renderFireFlicker(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    // Use speed to control flicker intensity: higher speed = slower, gentler flicker
    uint16_t period = max<uint16_t>(_speed, 100);
    uint8_t maxFlicker = constrain(120000 / period, 20, 120); // slower = less flicker variation
    for (uint16_t i = 0; i < n; i++) {
      uint8_t flicker = random(maxFlicker);
      float f = 1.0f - (flicker / 255.0f);
      uint32_t c = scaleColor(_color, f);
      strip.setPixelColor(_segStart + i, c);
    }
  }

  void renderColorWipeRandom(unsigned long now) {
    uint16_t n = segLen(); if (n == 0) return;
    unsigned long elapsed = now - _startedMs;
    uint16_t stepMs = max<uint16_t>(_speed / max<uint16_t>(n, 1), 15);
    int idx = (int)(elapsed / stepMs);
    if ((uint16_t)idx != _wipeIndex) {
      _wipeIndex = (uint16_t)idx;
      _wipeColor = wheel(random(256));
    }
    clearSeg();
    int limit = min<int>(idx, n);
    for (int i = 0; i < limit; i++) {
      uint16_t p = _segStart + (_reverse ? (n - 1 - i) : i);
      setPixelColorScaled(p, _wipeColor);
    }
  }
};

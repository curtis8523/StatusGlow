# StatusGlow UI - Quick Reference Guide

## 🎨 Visual Design Elements

### Brand Header (All Pages)
```
┌─────────────────────────────────────────────────┐
│  [Animated Purple Gradient Background]         │
│                                                 │
│      ⚪ StatusGlow ⚪                           │
│      INTELLIGENT PRESENCE INDICATOR SYSTEM      │
│                                                 │
└─────────────────────────────────────────────────┘
```

### Live Top Bar
```
┌─────────────────────────────────────────────────┐
│ CPU [50%] Heap [35%] Wi‑Fi [████] [Theme]       │
└─────────────────────────────────────────────────┘
```

### Navigation Tabs
```
┌─────┬─────┬─────┬─────┬─────┐
│ Home│Config│Effects│Logs│  FW │  ← Active tab has accent border
└─────┴─────┴─────┴─────┴─────┘
```

---

## 🎯 Key Improvements

### Before → After

#### Typography
```
BEFORE: system-ui, Arial, sans-serif (generic)
AFTER:  Inter (body), Orbitron (headers) - professional custom fonts
```

#### Headers
```
BEFORE: <h2>StatusGlow - v1.0.0</h2>
AFTER:  ⚪ StatusGlow ⚪
        INTELLIGENT PRESENCE INDICATOR SYSTEM
        (with gradient background and glow animation)
```

#### Status Pills
```
BEFORE: [50%]  (plain text)
AFTER:  [50%]  (colored pill with glow effect)
         ╰─── Green/Amber/Red based on threshold
```

#### Buttons
```
BEFORE: [Save]  (flat, basic)
AFTER:  [Save]  (shadow, hover lift, smooth transitions)
```

#### Color Swatch
```
BEFORE: [■]  20x20px, 1px border
AFTER:  [■]  32x32px, glows matching LED color
```

---

## 📊 Section Organization (Home Page)

```
1. 🎨 Display Status
   ├─ Current effect name
   ├─ Effect parameters
   └─ Glowing color swatch

2. 📊 Live Metrics
   ├─ Polling interval
   ├─ LED count
   ├─ System uptime (tabular nums)
   ├─ CPU usage (colored pill)
   └─ RAM usage (colored pill)

3. 📡 Network
   ├─ Wi‑Fi RSSI + signal bars
   ├─ SSID
   ├─ IP address
   └─ Full network status

4. 💻 System Information
   ├─ CPU frequency
   ├─ Firmware version (styled badge)
   ├─ Free heap
   └─ Min free heap

5. 💾 Memory Usage
   ├─ Flash (gradient progress bar)
   └─ RAM (gradient progress bar)

6. ⚠️ Danger Zone
   └─ Factory reset (improved modal)
```

---

## 🌈 Color System

### Status Pill Colors
```
┌────────────┬─────────┬──────────────┐
│ State      │ Color   │ Glow         │
├────────────┼─────────┼──────────────┤
│ OK         │ Indigo  │ Blue glow    │
│ Warning    │ Amber   │ Orange glow  │
│ Critical   │ Red     │ Red glow     │
└────────────┴─────────┴──────────────┘
```

### Theme Colors
```
Light Theme:
  Background: #f5f7fa (soft blue-grey)
  Accent:     #6366f1 (indigo)
  
Dark Theme:
  Background: #0f172a (deep navy)
  Accent:     #818cf8 (lighter indigo)
```

---

## ⚡ Interactive Elements

### Button States
```
Default:  [Button]          ← shadow-sm
Hover:    [Button] ↑        ← shadow, lift -1px, accent border
Active:   [Button] ↓        ← pressed +1px
```

### Danger Buttons
```
[Reboot Device]     ← Red background
[Factory Reset]     ← Red background with enhanced glow on hover
```

### Cards
```
Default:  ┌──────┐  ← shadow
          │      │
Hover:    │      │  ← shadow-lg (enhanced depth)
          └──────┘
```

---

## 📱 Responsive Behavior

### Desktop (>768px)
- Header: 2.5rem font
- Card padding: 1.5rem
- Row layout: horizontal (label → value)

### Mobile (≤768px)
- Header: 1.75rem font
- Card padding: 1rem
- Row layout: stacked (label above value)
- Reduced spacing

---

## 🎭 Animations

### Brand Header
```
Pulse animation (4s cycle):
  0% opacity: 0.2  ┐
                   │ Background gradient overlay
 50% opacity: 0.4  │ creates "breathing" effect
                   │
100% opacity: 0.2  ┘
```

### Status Glow Dots
```
Glow animation (2s cycle):
  0% scale: 1.0, opacity: 1.0  ┐
                                │ Dots pulse in/out
 50% scale: 1.1, opacity: 0.6  │ simulating glow
                                │
100% scale: 1.0, opacity: 1.0  ┘
```

---

## 🔤 Font Usage

### Page Title
- **Font**: Orbitron 900
- **Size**: 2.5rem (desktop), 1.75rem (mobile)
- **Effect**: Text shadow + glow
- **Color**: White (#ffffff)

### Section Headers (h3)
- **Font**: Orbitron 700
- **Size**: 1.25rem
- **Color**: Accent color (indigo)

### Body Text
- **Font**: Inter 400
- **Size**: 1rem (base)
- **Line Height**: 1.6

### Labels
- **Font**: Inter 600
- **Size**: 0.9rem
- **Color**: Muted (slate)

### Monospace (Logs, Code)
- **Font**: ui-monospace, system monospace
- **Background**: Dark terminal style
- **Color**: Green text on dark background

---

## 💡 Design Philosophy

1. **Professional** - Looks like a production web app, not hobbyist project
2. **Branded** - Consistent "StatusGlow" identity throughout
3. **Informative** - Clear visual hierarchy guides attention
4. **Delightful** - Subtle animations and glows add personality
5. **Accessible** - High contrast, readable fonts, semantic HTML

---

## 🎯 User Impact

### Visual Improvements
✅ Instant brand recognition with header on every page
✅ Easier scanning with emoji icons and colored pills
✅ Better understanding of status with visual indicators (glows, bars)
✅ Modern, trustworthy appearance

### Functional Improvements
✅ Logical section organization
✅ Clearer visual hierarchy
✅ Enhanced readability with custom fonts
✅ Better touch targets for mobile users
✅ Improved feedback on interactions (hover, active states)

---

## 📐 CSS Architecture

### Design System
- **Spacing**: 0.5rem increments (0.5, 1, 1.5, 2rem)
- **Border Radius**: 0.35rem (small), 0.5rem (medium), 0.75rem (large), 999px (pill)
- **Shadows**: sm, md (default), lg
- **Transitions**: 0.2s ease on all interactive elements

### CSS Variables
All values centralized in `:root` and `[data-theme='dark']`:
- Colors: `--accent`, `--bg`, `--fg`, `--muted`, etc.
- Shadows: `--shadow-sm`, `--shadow`, `--shadow-lg`
- Gradients: `--gradient-primary`, `--gradient-status`

This enables easy theming and consistent updates across the entire UI.

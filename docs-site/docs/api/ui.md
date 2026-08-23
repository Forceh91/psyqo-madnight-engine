---
title: UI
sidebar_position: 10
---

# UI

`src/ui/hud/` and `src/ui/menu/` — a non-interactive HUD overlay system, and an interactive `Menu` scene base class built on the same underlying elements.

## HUDElement

`src/ui/hud/hud_element.hh`

Base class for the two concrete HUD element types below. Not used directly.

```cpp
class HUDElement {
protected:
  bool m_isEnabled = false;
  eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> m_name;
  psyqo::Rect m_rect = {0};

public:
  HUDElement(const char *name, psyqo::Rect rect);
  void Enable();
  void Disable();
  eastl::fixed_string<char, GAMEPLAY_HUD_MAX_NAME_LEN> &name();
};
```

## TextHUDElement

`src/ui/hud/text_hud_element.hh`

```cpp
class TextHUDElement final : public HUDElement {
public:
  TextHUDElement();
  TextHUDElement(const char *name, psyqo::Rect rect);
  TextHUDElement(const char *name, psyqo::Rect rect, psyqo::Color colour);

  void SetFont(psyqo::Font<100> *font);
  void SetDisplayText(const char *displayText);
  void SetColour(const psyqo::Color colour);
  void SetPositionSize(psyqo::Rect rect);
  void Render(const psyqo::Rect &parentRect);
  void Render(const psyqo::Rect &parentRect, psyqo::Font<100> *defaultFont);
};
```

Defaults to [`COLOUR_WHITE`](./render#colour-constants) if no colour is set. If no font is set via `SetFont`, the `Render(parentRect, defaultFont)` overload falls back to whatever font is passed in — typically `Renderer::Instance().SystemFont()` — and then **keeps** that fallback font for future renders, rather than re-resolving it every frame.

### Usage

A debug/status readout that updates every frame — format into a stack buffer and push it in with `SetDisplayText`:

```cpp
m_posText = m_hud.AddTextHUDElement(TextHUDElement("POS", {.pos = {5, 0}, .size = {100, 100}}));
m_posText->SetFont(Renderer::Instance().SystemFont());

// per-frame:
char pos[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
snprintf(pos, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "POS %.2f,%.2f,%.2f", playerPos.x, playerPos.y, playerPos.z);
m_posText->SetDisplayText(pos);
m_hud.Render();
```

## SpriteHUDElement

`src/ui/hud/sprite_hud_element.hh`

```cpp
class SpriteHUDElement final : public HUDElement {
public:
  SpriteHUDElement();
  SpriteHUDElement(const char *name, psyqo::Rect rect);
  SpriteHUDElement(const char *name, psyqo::Rect rect, const char *textureName, psyqo::PrimPieces::UVCoords uv);

  void Render(const psyqo::Rect &parentRect);
  void SetSize(const psyqo::Vertex& size);
  void SetUV(const psyqo::PrimPieces::UVCoords uv);
};
```

### Usage

A stat bar that animates its width in — a flat-colour sprite (single-pixel UV) scaled by `SetSize`, with the width lerped from its previous value to the new one over a fixed duration:

```cpp
auto *capacityBar = m_menu.AddSpriteHUDElement(SpriteHUDElement("CS_CAPACITY_BAR", {165, 100, 0, 10}, "UI/CS_UI.TIM", {0, 129}));

// per-frame, once a new stat value has been picked:
uint8_t LerpStatValue(uint8_t oldValue, uint8_t newValue) {
    auto delta = (Renderer::Instance().GPU().now() - m_switchTime) / MICROSECONDS_IN_A_MILLISECOND;
    if (delta >= LERP_TIME_MS) return newValue;
    auto t = (1.0_fp * delta) / LERP_TIME_MS;
    return Lerp(1.0_fp * oldValue, 1.0_fp * newValue, t).integer();
}

capacityBar->SetSize({static_cast<int16_t>(remap(LerpStatValue(lastCapacity, currentCapacity), MAX_STAT_VALUE, 128)), 10});
```

Only the width changes here — height stays fixed at `10`, and a small `remap()` helper (linearly rescaling one range to another) converts the stat's own value range into a pixel width. Neither `remap()` nor `MICROSECONDS_IN_A_MILLISECOND` are part of the engine — both are small helpers defined game-side; `GPU().now()` (see [`Renderer`](./render#renderer)) and [`Lerp`](./math#vector) are the only engine calls doing real work here.

## GameplayHUD

`src/ui/hud/gameplay_hud.hh`

A non-interactive HUD you overlay on top of gameplay — health, lives, a timer, etc. It can't accept input to select between options; use [`Menu`](#menu) for that. You'd typically have one active at a time, though switching between a few during gameplay is fine.

```cpp
class GameplayHUD final {
public:
  GameplayHUD();
  GameplayHUD(const char *name, psyqo::Rect rect);

  void Enable();
  void Disable();
  bool IsEnabled(void);
  void Destroy(void); // disables and frees all held text/sprite elements
  void Render(void);

  // don't lose track of the returned pointer!
  TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement);
  void RemoveTextHUDElement(TextHUDElement *element);

  // limited to 40 elements
  SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement);
  void RemoveSpriteHUDElement(SpriteHUDElement *element);
};
```

Backed by fixed-capacity vectors (50 text elements, 40 sprite elements), embedded in the class by value rather than allocated separately: a `GameplayHUD` instance is about 13.7 KB as a result. `Add*HUDElement` moves the element in and returns a stable pointer into that storage, which you hold onto to update or later remove it. Overflow is disabled on both vectors, so exceeding either capacity isn't a rejected call: `push_back` writes straight past the end of the fixed storage with no bounds check left in a release build, which is undefined behaviour rather than a dropped element. [`PerfMonitor`](./core#perfmonitor) is built on top of this class.

### Usage

```cpp
GameplayHUD hud("Gameplay HUD", {.pos = {5, 5}, .size = {100, 20}});
auto *healthText = hud.AddTextHUDElement(TextHUDElement("Health", {.pos = {0, 0}, .size = {100, 10}}));
hud.Enable();

// per-frame:
healthText->SetDisplayText("HP: 80/100");
hud.Render();
```

### Internals

- Every element's position is relative to the HUD's own `m_rect` — `GameplayHUD::Render` passes its own rect down to each child's `Render(parentRect)` call, so an element's on-screen position is `parentRect.pos + element.m_rect.pos`.

## Menu

`src/ui/menu/menu.hh`

The interactive counterpart to `GameplayHUD` — a `psyqo::Scene` subclass, so activating a menu pushes it as a scene on top of whatever's currently showing. By default it doesn't clear the frame buffer, so the previous render (e.g. paused gameplay) stays visible underneath — makes it a natural fit for a pause menu.

```cpp
class Menu : public psyqo::Scene {
public:
  Menu() = default;
  Menu(const char *name, psyqo::Rect posSizeRect);

  bool IsEnabled(void);
  void Activate(void);
  void Deactivate(void); // also triggered by the configured back/cancel button
  void Destroy(void);    // deactivates, pops the scene, and frees all held elements/items

  void SetControllerBindings(const MenuControllerBinds &bindings); // uses defaults if not called
  // Override only the buttons that trigger menu item input callbacks, on top
  // of the default up/down/confirm/cancel. Avoid duplicating those defaults.
  void SetCustomInputCallbackButtons(const eastl::array<psyqo::AdvancedPad::Button, 16> &customBindings);

  void SetOnFrame(eastl::function<void(uint32_t)> callback);
  void SetOnActivate(eastl::function<void(void)> callback);
  void SetOnDeactivate(eastl::function<void(void)> callback);
  void SetOnDestroy(eastl::function<void(void)> callback);

  TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement);
  void RemoveTextHUDElement(TextHUDElement *element);
  SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement);
  void RemoveSpriteHUDElement(SpriteHUDElement *element);

  MenuItem *AddMenuItem(const MenuItem &item);
  MenuItem *AddMenuItem(const char *name, const char *displayText, const psyqo::Rect posSize);
  void AddMenuItems(const eastl::span<MenuItem> &items);
  void SetDefaultFont(psyqo::Font<100> *font);
  void SetSelectedMenuItem(uint8_t ix); // clamped to a valid index
};
```

- Capacities: up to `MENU_MAX_TEXT_ELEMENTS` (20) text elements, `MENU_MAX_SPRITE_ELEMENTS` (50) sprite elements, `MENU_MAX_MENU_ITEMS` (10) [`MenuItem`](#menuitem)s. All three are fixed-capacity vectors with overflow disabled and embedded by value (a `Menu` instance is about 13.2 KB as a result); exceeding any of them is undefined behaviour rather than a dropped element or rejected call, same as [`GameplayHUD`](#gameplayhud).
- `MoveSelectedMenuItemPrev`/`Next` are now `private` — navigation happens through the bound up/down buttons (or your own custom-bound buttons); they're no longer meant to be called directly from game code.
- A disabled `MenuItem` is automatically skipped: if the currently-selected item is disabled when the menu renders, it advances to the next item that frame rather than rendering nothing as selected.

### Usage

The pause-menu pattern from the engine's own `GameplayScene` — bind `Start` to open the menu, and let the menu's own default bindings handle navigation and closing it:

```cpp
Menu pauseMenu("Pause", {.pos = {80, 60}, .size = {160, 100}});
pauseMenu.AddMenuItem("resume", "Resume", {.pos = {0, 0}, .size = {100, 10}})
    ->SetOnConfirm([&]{ pauseMenu.Deactivate(); });
pauseMenu.AddMenuItem("quit", "Quit", {.pos = {0, 15}, .size = {100, 10}})
    ->SetOnConfirm([]{ /* ... */ });

g_madnightEngine.m_input.setOnEvent([&](auto event) {
    if (event.type == psyqo::AdvancedPad::Event::ButtonReleased && event.button == psyqo::AdvancedPad::Button::Start)
        pauseMenu.Activate();
});
```

### Internals

- Backing out via the bound cancel button (`Triangle` by default) doesn't call `Deactivate()` immediately — it just sets a flag that's checked at the top of the *next* `frame()` call, so there's a one-frame delay. Calling `Deactivate()` yourself (e.g. from an `OnConfirm` callback, as above) pops the scene immediately.
- `Deactivate()` just pops the scene, leaving the menu's elements/items intact for next time; `Destroy()` additionally frees all held text/sprite elements and menu items — use it for a menu you won't reopen (e.g. a one-shot results screen), not a pause menu you expect to reactivate.
- `ProcessInputs` never checks `event.pad`: on a multitap, any connected pad drives the menu, not just the one you might expect. The pause-menu sample above binds without checking it either.

Menus aren't limited to "list of items, navigate up/down" — a single `MenuItem` with `SetOnInputCallback` can act as a left/right selector over a completely different data set instead of a list of item states, using `SetCustomInputCallbackButtons` to opt those buttons in:

```cpp
m_menu.SetCustomInputCallbackButtons({psyqo::AdvancedPad::Button::Left, psyqo::AdvancedPad::Button::Right});

auto *cartName = m_menu.AddMenuItem(MenuItem("CART_NAME", {165, 50, 0, 0}));
cartName->SetText(CART_DATA[0].name());
cartName->SetOnInputCallback([this](const psyqo::AdvancedPad::Button button) {
    if (button == psyqo::AdvancedPad::Button::Left)
        m_selectedIx = m_selectedIx == 0 ? NUM_CARTS - 1 : m_selectedIx - 1;
    if (button == psyqo::AdvancedPad::Button::Right)
        m_selectedIx = (m_selectedIx + 1) % NUM_CARTS;

    cartName->SetText(CART_DATA[m_selectedIx].name());
});
```

Also worth using `SetOnFrame` for menu-driven per-frame logic (e.g. animating a preview model or lerping stat bars, as in [`SpriteHUDElement`'s usage example](#spritehudelement)) — the callback runs from the menu's own scene `frame()`, so it fires while the menu is active without you needing a separate scene subclass.

### MenuControllerBinds

`src/ui/menu/menu_controller_binds.hh`

```cpp
struct MenuControllerBinds {
  psyqo::AdvancedPad::Event onEventType;
  psyqo::AdvancedPad::Button menuItemNext;
  psyqo::AdvancedPad::Button menuItemPrev;
  psyqo::AdvancedPad::Button menuItemConfirm;
  psyqo::AdvancedPad::Button menuItemBackCancel;
  eastl::array<psyqo::AdvancedPad::Button, 16> menuItemCustom;
};
```

Default bindings (used if `SetControllerBindings` is never called): `Down`/`Up` to move selection, `Cross` to confirm, `Triangle` to back out, on `ButtonReleased`.

### MenuItem

`src/ui/menu/menu_item.hh`

A single selectable entry in a `Menu`, combining a `TextHUDElement` and `SpriteHUDElement` with confirm/input callbacks.

```cpp
class MenuItem {
public:
  MenuItem(const char *name, psyqo::Rect posSizeRect);
  MenuItem(const char *name, const char *text, psyqo::Rect posSizeRect);
  MenuItem(const char *name, const char *text, psyqo::Rect posSizeRect, psyqo::Color defaultTextColour, psyqo::Color selectedTextColour);

  void Enable();
  void Disable();
  bool IsEnabled() const;
  void Render(const psyqo::Rect parentRect, const bool isSelected, psyqo::Font<100> *fallbackFont);

  void SetSpriteElement(const SpriteHUDElement &sprite);
  void SetFont(psyqo::Font<100> *font);
  void SetTextElement(const TextHUDElement &text);
  void SetText(const char *text);
  void SetTextColour(const psyqo::Color colour);
  void SetPositionSize(psyqo::Rect rect);

  void SetOnConfirm(eastl::function<void(void)> callback);
  // requires a matching entry in `menuItemCustom` (see `Menu::SetControllerBindings`)
  void SetOnInputCallback(eastl::function<void(const psyqo::AdvancedPad::Button)> callback);
};
```

Defaults to [`COLOUR_WHITE`](./render#colour-constants) for unselected text and [`COLOUR_YELLOW`](./render#colour-constants) for the selected item — override both via the four-argument constructor or `SetTextColour`.

### Internals

- `Render` draws both the item's text and, if one has been set via `SetSpriteElement`, its sprite, both positioned relative to the parent `Menu`'s rect.
- Selection changes the item's appearance, not just its text colour.

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

Defaults to [`COLOUR_WHITE`](./render#colour-constants) if no colour is set. If no font is set via `SetFont`, the `Render(parentRect, defaultFont)` overload falls back to whatever font is passed in — typically `Renderer::Instance().SystemFont()`.

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
  void Render(void);

  // don't lose track of the returned pointer!
  TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement);
  void RemoveTextHUDElement(TextHUDElement *element);

  // limited to 40 elements
  SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement);
  void RemoveSpriteHUDElement(SpriteHUDElement *element);
};
```

Backed by fixed-capacity vectors (50 text elements, 40 sprite elements) — `Add*HUDElement` moves the element in and returns a stable pointer into that storage, which you hold onto to update or later remove it. [`PerfMonitor`](./core#perfmonitor) is built on top of this class.

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

  void SetControllerBindings(const MenuControllerBinds &bindings); // uses defaults if not called
  // Override only the buttons that trigger menu item input callbacks, on top
  // of the default up/down/confirm/cancel. Avoid duplicating those defaults.
  void SetCustomInputCallbackButtons(const eastl::array<psyqo::AdvancedPad::Button, 16> &customBindings);

  void SetOnFrame(eastl::function<void(uint32_t)> callback);
  void SetOnActivate(eastl::function<void(void)> callback);
  void SetOnDeactivate(eastl::function<void(void)> callback);

  TextHUDElement *AddTextHUDElement(TextHUDElement &&textElement);
  void RemoveTextHUDElement(TextHUDElement *element);
  SpriteHUDElement *AddSpriteHUDElement(SpriteHUDElement &&spriteElement);
  void RemoveSpriteHUDElement(SpriteHUDElement *element);

  MenuItem *AddMenuItem(const MenuItem &item);
  MenuItem *AddMenuItem(const char *name, const char *displayText, const psyqo::Rect posSize);
  void AddMenuItems(const eastl::span<MenuItem> &items);
  void SetDefaultFont(psyqo::Font<100> *font);

  uint8_t MoveSelectedMenuItemPrev();
  uint8_t MoveSelectedMenuItemNext();
};
```

- Capacities: up to `MENU_MAX_TEXT_ELEMENTS` (20) text elements, `MENU_MAX_SPRITE_ELEMENTS` (50) sprite elements, `MENU_MAX_MENU_ITEMS` (10) [`MenuItem`](#menuitem)s.
- `MoveSelectedMenuItemPrev`/`Next` wrap around the item list and update the internal selection index — call these from your controller-binds handling, or rely on the defaults below.

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

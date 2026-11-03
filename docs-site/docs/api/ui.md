---
title: UI
sidebar_position: 10
---

# UI

`src/ui/hud/` and `src/ui/menu/` have two kinds of screen UI. A `GameplayHUD` draws text and sprites on top of gameplay and takes no input. A `Menu` is a scene you push on top of gameplay that the player can move through with the pad.

Unlike the managers, you create and own these yourself, usually as members of your scene.

## GameplayHUD

`src/ui/hud/gameplay_hud.hh`

A group of text and sprite elements drawn relative to the HUD's own position. Use it for health, a timer, a debug readout, anything that only displays.

```cpp
// a scene member
GameplayHUD m_hud = GameplayHUD("HUD", {.pos = {5, 10}, .size = {100, 100}});
TextHUDElement *m_scoreText = nullptr;

// once, in start() when reason is StartReason::Create
m_scoreText = m_hud.AddTextHUDElement(TextHUDElement("SCORE", {.pos = {5, 0}, .size = {100, 100}}));

// every frame, after Renderer::Render
char score[GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN];
snprintf(score, GAMEPLAY_HUD_ELEMENT_MAX_STR_LEN, "Score: %d", m_score);
m_scoreText->SetDisplayText(score);
m_hud.Render();
```

This is how the engine's own `PerfMonitor` and `GameplayScene` build their HUDs. Call `Render()` after `Renderer::Render` each frame, or the 3D scene will be drawn over it.

A HUD made with a name and a rect starts enabled. `Disable()` hides it and `Enable()` shows it again, and `IsEnabled()` tells you which. The default constructor makes a disabled HUD at 0,0.

`Add*HUDElement` moves the element into the HUD and returns a pointer to its copy. Keep that pointer to update or remove the element later.

```cpp
m_hud.RemoveTextHUDElement(m_scoreText);
m_hud.Destroy(); // disable and remove every element
```

Limits:

- Up to 50 text elements and 40 sprite elements. There is no check: adding more than that writes past the end of the HUD.
- Removing an element shifts the ones added after it down, so pointers you hold to those now point at the wrong element. If you need to hide one, call `Disable()` on it instead.
- Text is drawn before sprites, so sprites land on top of text.

## TextHUDElement

`src/ui/hud/text_hud_element.hh`

One line of text. Its position is relative to the HUD or menu that holds it; the size is not used for text.

```cpp
auto *lives = m_hud.AddTextHUDElement(TextHUDElement("LIVES", {.pos = {5, 15}, .size = {100, 10}}, COLOUR_YELLOW));
lives->SetDisplayText("Lives: 3");
lives->SetColour(COLOUR_WHITE);
```

The colour defaults to [`COLOUR_WHITE`](./render#colour-constants). If you never call `SetFont`, the element uses the menu's default font, or the system font (`Renderer::Instance().SystemFont()`), and keeps using it from then on. `SetPositionSize` moves it, and `Enable()`/`Disable()` show and hide it.

Limits:

- `SetDisplayText` expects a null-terminated string. A `string_view` cut from the middle of a longer string shows the rest of that string too.

## SpriteHUDElement

`src/ui/hud/sprite_hud_element.hh`

A rectangle cut from a loaded texture. The rect gives the position, and its size is both the on-screen size and the size of the region taken from the texture; sprites are not scaled. The UV is the top-left corner of that region within the texture's VRAM page.

```cpp
auto *bar = m_hud.AddSpriteHUDElement(
    SpriteHUDElement("HEALTH_BAR", {.pos = {5, 30}, .size = {64, 8}}, "UI/HUD.TIM", {0, 0}));

// later, shrink the bar to match the player's health
bar->SetSize({static_cast<int16_t>(m_health * 64 / 100), 8});
```

`SetSize` changes the size, which crops rather than stretches, and `SetUV` changes which part of the texture is used.

Limits:

- The texture is looked up once, when the element is constructed, so load it first. An element whose texture was not loaded draws nothing.
- A hard load frees every texture, so build sprite elements again after one.
- The renderer draws at most 40 sprites per frame in total, shared between every HUD, menu and anything else calling `Renderer::RenderSprite`. Any past that are dropped.

## Menu

`src/ui/menu/menu.hh`

A menu is a `psyqo::Scene`. `Enable()` pushes it on top of the current scene and `Disable()` pops it. It does not clear the screen, so the last gameplay frame stays visible behind it, which makes it a good fit for a pause menu.

```cpp
// a scene member
Menu m_pauseMenu = Menu("Pause", {.pos = {80, 60}, .size = {160, 100}});

// once, in start() when reason is StartReason::Create
m_pauseMenu.AddMenuItem("resume", "Resume", {.pos = {0, 0}, .size = {100, 10}})
    ->SetOnConfirm([this]() { m_pauseMenu.Disable(); });
m_pauseMenu.AddMenuItem("quit", "Quit", {.pos = {0, 15}, .size = {100, 10}})
    ->SetOnConfirm([]() { /* back to the title screen */ });

// in your input handler
if (event.type == psyqo::AdvancedPad::Event::ButtonReleased && event.button == psyqo::AdvancedPad::Button::Start) {
  m_pauseMenu.Enable();
}
```

The engine's `GameplayScene` opens its menu with Start in the same way.

By default Down and Up move the selection, Cross confirms the selected item, and Triangle closes the menu, all on button release. Selection wraps at both ends, and disabled items are skipped and not drawn. `SetSelectedMenuItem(ix)` picks an item yourself.

While the menu is open it owns the pad: it replaces the input handler when it opens and clears it when it closes. The scene underneath is resumed with `start(StartReason::Resume)`, so set your input handler in `start()` before any check for `StartReason::Create`, as `GameplayScene` does, or the pad stops working once the menu closes.

Limits:

- Up to 10 items, 20 text elements and 50 sprite elements. There is no check: adding more writes past the end.
- `Disable()` and `Destroy()` always pop the top scene, even if the menu is not open. Only call them while the menu is showing. Calling `Enable()` twice pushes it twice.
- Closing with the cancel button takes effect on the next frame, not straight away.
- Any connected pad drives the menu.

### Text, sprites and callbacks

A menu holds plain text and sprite elements as well as items, added the same way as on a [`GameplayHUD`](#gameplayhud). `SetDefaultFont` sets the font for any text that has none of its own.

`SetOnFrame` runs a function every frame while the menu is open, which is where you update anything the menu shows. `SetOnEnable`, `SetOnDisable` and `SetOnDestroy` run when the menu opens, closes and is destroyed.

```cpp
m_pauseMenu.SetOnFrame([this](uint32_t deltaTime) { UpdatePreview(deltaTime); });
m_pauseMenu.SetOnDisable([this]() { m_paused = false; });
```

`Disable()` keeps the menu's contents for next time. `Destroy()` closes it and removes every element and item, for a menu you will not open again.

### Changing the buttons

`SetControllerBindings` replaces the default buttons with a `MenuControllerBinds` (`src/ui/menu/menu_controller_binds.hh`).

```cpp
m_pauseMenu.SetControllerBindings({
    .onEventType = {.type = psyqo::AdvancedPad::Event::ButtonReleased},
    .menuItemNext = psyqo::AdvancedPad::Button::Right,
    .menuItemPrev = psyqo::AdvancedPad::Button::Left,
    .menuItemConfirm = psyqo::AdvancedPad::Button::Cross,
    .menuItemBackCancel = psyqo::AdvancedPad::Button::Circle,
});
```

Only the `type` of `onEventType` is used: set it to `ButtonPressed` or `ButtonReleased`. `SetControllerBindings` also replaces the custom buttons below, so call `SetCustomInputCallbackButtons` after it.

## MenuItem

`src/ui/menu/menu_item.hh`

One selectable line in a menu. Its position is relative to the menu. The selected item's text is drawn in [`COLOUR_YELLOW`](./render#colour-constants) and the rest in `COLOUR_WHITE`. Pass two colours to the constructor to change those.

```cpp
m_pauseMenu.AddMenuItem(MenuItem("options", "Options", {.pos = {0, 30}, .size = {100, 10}}, COLOUR_WHITE, COLOUR_YELLOW));
```

`SetText` changes the label. `Disable()` hides the item and makes the menu skip it; `Enable()` brings it back. `SetSpriteElement` adds a sprite drawn with the item, positioned relative to it.

`SetTextColour` has no lasting effect, because the item sets its colour from the two constructor colours every time it is drawn.

### Left and right on an item

An item can react to other buttons as well as confirm, for example a left/right selector. List the buttons with `SetCustomInputCallbackButtons`, then handle them in `SetOnInputCallback`. The callback only runs for the selected item.

```cpp
m_menu.SetCustomInputCallbackButtons({psyqo::AdvancedPad::Button::Left, psyqo::AdvancedPad::Button::Right});

auto *difficulty = m_menu.AddMenuItem("difficulty", "Normal", {.pos = {0, 0}, .size = {100, 10}});
difficulty->SetOnInputCallback([this, difficulty](const psyqo::AdvancedPad::Button button) {
  if (button == psyqo::AdvancedPad::Button::Left && m_difficulty > 0) m_difficulty--;
  else if (button == psyqo::AdvancedPad::Button::Right && m_difficulty < 2) m_difficulty++;
  else return;
  difficulty->SetText(DIFFICULTY_NAMES[m_difficulty]);
});
```

Always check which button you were given. The list has 16 slots, and any you leave out are `Select`, so pressing Select calls the callback too, even if you never set custom buttons.

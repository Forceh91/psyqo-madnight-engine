#pragma once

#include <psyqo/gpu.hh>

static constexpr uint8_t DEBUG_MENU_OPTION_COUNT = 2;

class DebugMenu final {
	bool m_isEnabled = false;
	uint8_t m_raycastDistance = 3;
	uint8_t m_selectedDebugOption = 0;
	uint32_t m_startDebugMenuOpenCapture = 0;
	uint8_t m_debugMenuOpenCapturedInputs = 0;
	bool m_displayDebugHUD = true;

	void ToggleEnabled(void);
	void ResetInputCapture(void);

  public:
	void Init(void);
	void Process(void);
	void Draw(psyqo::GPU& gpu);
	bool IsEnabled() { return m_isEnabled; }
	uint8_t RaycastDistance() { return m_raycastDistance; }
	bool DisplayDebugHUD() { return m_displayDebugHUD; }
};

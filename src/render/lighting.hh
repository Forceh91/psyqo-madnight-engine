#include "psyqo/matrix.hh"
#include "psyqo/primitives/common.hh"
#include <cstdint>

static constexpr psyqo::Color DEFAULT_CLEAR_COLOR = {.r = 0, .g = 0, .b = 0};

class Lighting {
  public:
	static Lighting &instance() {
		static Lighting s_instance;
		return s_instance;
	}
	
	const bool& IsSimpleFogEnabled(void) const { return m_isSimpleFogEnabled; }
	void EnableSimpleFog(void) { m_isSimpleFogEnabled = true; }
	void DisableSimpleFog(void) { m_isSimpleFogEnabled = false; }
	
	const psyqo::Color& GetAmbient(void) const { return m_ambient; }
	void SetAmbient(psyqo::Color colour) { m_ambient = colour; }

	const psyqo::Color& GetFogColour(void) const { return m_fogColour; }
	void SetFogColour(psyqo::Color colour) { m_fogColour = colour; }

	const bool* LightsEnabled(void) const { return m_enabledLights; }
	const bool ShouldApplyGTELighting(void) const {
		// if at least one light is enabled, we need to apply NC
		for (int i = 0; i < 3; i++) {
			if (m_enabledLights[i])
				return true;
		}

		// got through the loop with no lights enabled
		return false;
	}
	const bool IsLightEnabled(uint8_t ix) const { return m_enabledLights[ix]; }
	
	// make sure you use `SetLightDirMatrix` and `SetLightColourMatrix` when you turn these on (ix = 0 - 2)
	void SetLightEnabled(uint8_t ix, bool enabled) { m_enabledLights[ix] = enabled; }
	// make sure you use `SetLightDirMatrix` and `SetLightColourMatrix` when you turn these on
	void SetLightsEnabled(bool enabled) {
		for (int i = 0; i < 3; i++) {
			m_enabledLights[i] = enabled;
		}
	}

    // LLM: Three directional lights, in world space, each a pure primary.
    // Rows are light directions, unit length, in world space. Screen Z runs
    // INTO the display, so a light that reaches a camera-facing surface has a
    // negative z: front normals point back out at the viewer.
	const psyqo::Matrix33& GetLightDirMatrix(void) const { return m_lightDirs; }
    // LLM: Three directional lights, in world space, each a pure primary.
    // Rows are light directions, unit length, in world space. Screen Z runs
    // INTO the display, so a light that reaches a camera-facing surface has a
    // negative z: front normals point back out at the viewer.
	void SetLightDirMatrix(const psyqo::Matrix33& llm) { m_lightDirs = llm; }

	// LCM: rows are channels, COLUMNS are lights. Column i is light i's colour.
	const psyqo::Matrix33& GetLightColourMatrix(void) const { return m_lightColours; }
	// LCM: rows are channels, COLUMNS are lights. Column i is light i's colour.
	void SetLightColourMatrix(const psyqo::Matrix33& lcm) { m_lightColours = lcm; }	
  private:
	Lighting() = default;
	
	bool m_isSimpleFogEnabled = false;
	// clear colour IS fog colour
	psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR;	
	psyqo::Color m_ambient = {128, 128, 128};

	// GTE lighting
	bool m_enabledLights[3] = {false, false, false};
	psyqo::Matrix33 m_lightDirs = {0};
	psyqo::Matrix33 m_lightColours = {0};	
};

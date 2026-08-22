#include <psyqo/primitives/common.hh>

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
	
	const psyqo::Color& GetAmbientColour(void) const { return m_ambient; }
	void SetAmbientColour(psyqo::Color colour) { m_ambient = colour; }

	const psyqo::Color& GetFogColour(void) const { return m_fogColour; }
	void SetFogColour(psyqo::Color colour) { m_fogColour = colour; }

	private:
	Lighting() = default;
	
	bool m_isSimpleFogEnabled = false;
	// clear colour IS fog colour
	psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR;	
	psyqo::Color m_ambient = {128, 128, 128};
};

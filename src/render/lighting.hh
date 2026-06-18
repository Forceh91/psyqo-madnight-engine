#include "psyqo/primitives/common.hh"

static constexpr psyqo::Color DEFAULT_CLEAR_COLOR = {.r = 0, .g = 0, .b = 0};

class Lighting {
  public:
	static Lighting &instance() {
		static Lighting s_instance;
		return s_instance;
	}

	psyqo::Color m_ambient = {128, 128, 128};

	// clear colour IS fog colour
	psyqo::Color m_fogColour = DEFAULT_CLEAR_COLOR;
	bool m_isSimpleFogEnabled = false;

	void EnableSimpleFog(void) { m_isSimpleFogEnabled = true; }
	void DisableSimpleFog(void) { m_isSimpleFogEnabled = false; }

	void SetAmbient(psyqo::Color colour) { m_ambient = colour; }
	void SetFogColour(psyqo::Color colour) { m_fogColour = colour; }

  private:
	Lighting() = default;
};

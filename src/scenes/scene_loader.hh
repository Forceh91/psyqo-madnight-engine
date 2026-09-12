#pragma once
#include "../helpers/load_queue.hh"
#include <EASTL/string_view.h>
#include <EASTL/vector.h>
#include <psyqo/coroutine.hh>

class SceneLoader final {
  public:
	static psyqo::Coroutine<> LoadScene(const eastl::string_view& sceneFile, eastl::vector<LoadQueue>& queue);

  private:
};

#include	"scenemanager.h"
#include	"SceneClassFactory.h"
#include	"DebugUI.h"

// “o˜^‚³‚ê‚Ä‚¢‚éƒV[ƒ“‚ð‘S‚Ä”jŠü‚·‚é
void SceneManager::Dispose() 
{
	DebugUI::ClearDebugFunctions();
	// “o˜^‚³‚ê‚Ä‚¢‚é‚·‚×‚ÄƒV[ƒ“‚ÌI—¹ˆ—
	for (auto& s : m_scenes) 
	{
		s.second->dispose();
	}

	m_scenes.clear();
	m_currentSceneName.clear();
}

void SceneManager::SetCurrentScene(std::string currentscenename)
{
    if (currentscenename.empty() || currentscenename == m_currentSceneName)
    {
        return;
    }

    auto obj = SceneClassFactory::getInstance().create(currentscenename);
    if (!obj)
    {
        return;
    }

    DebugUI::ClearDebugFunctions();

    for (auto& scene : m_scenes)
    {
        scene.second->dispose();
    }
    m_scenes.clear();

    obj->init();
    m_currentSceneName = std::move(currentscenename);
    m_scenes.emplace(m_currentSceneName, std::move(obj));
}

void SceneManager::Init()
{
}

void SceneManager::Draw(uint64_t deltatime)
{
    IScene* scene = GetCurrentScene();
    if (scene)
    {
        scene->draw(deltatime);
    }
}

void SceneManager::Update(uint64_t deltatime)
{
    IScene* scene = GetCurrentScene();
    if (scene)
    {
        scene->update(deltatime);
    }
}

IScene* SceneManager::GetCurrentScene()
{
    auto it = m_scenes.find(m_currentSceneName);
    return it != m_scenes.end() ? it->second.get() : nullptr;
}
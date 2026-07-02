#include "BrowserDebugReporter.h"

#include <filesystem>
#include <fstream>
#include <iomanip>

void BrowserDebugReporter::Init(const std::wstring& outputDirectory)
{
    m_outputDirectory = outputDirectory;
    std::filesystem::create_directories(m_outputDirectory);
    WriteHtml();
}

void BrowserDebugReporter::Write(const PerformanceSnapshot& snapshot, const Combat::CombatDebugState& combatState)
{
    std::filesystem::create_directories(m_outputDirectory);
    const std::filesystem::path jsonPath = std::filesystem::path(m_outputDirectory) / L"debug_state.json";
    std::ofstream file(jsonPath);
    if (!file)
    {
        return;
    }

    file << std::fixed << std::setprecision(3);
    file << "{\n";
    file << "  \"frameMilliseconds\": " << snapshot.frameMilliseconds << ",\n";
    file << "  \"estimatedFps\": " << snapshot.estimatedFps << ",\n";
    file << "  \"fixedUpdateCount\": " << snapshot.fixedUpdateCount << ",\n";
    file << "  \"budget\": {\n";
    file << "    \"cpuMilliseconds\": " << snapshot.budget.cpuMilliseconds << ",\n";
    file << "    \"gpuMilliseconds\": " << snapshot.budget.gpuMilliseconds << ",\n";
    file << "    \"systemsMilliseconds\": " << snapshot.budget.systemsMilliseconds << ",\n";
    file << "    \"reserveMilliseconds\": " << snapshot.budget.reserveMilliseconds << "\n";
    file << "  },\n";
    file << "  \"combat\": {\n";
    file << "    \"currentAttackId\": \"" << combatState.currentAttackId << "\",\n";
    file << "    \"currentPhase\": \"" << Combat::ToString(combatState.currentPhase) << "\",\n";
    file << "    \"broadPhaseCandidateCount\": " << combatState.broadPhaseCandidateCount << ",\n";
    file << "    \"confirmedCollisionCount\": " << combatState.confirmedCollisionCount << "\n";
    file << "  }\n";
    file << "}\n";
}

void BrowserDebugReporter::WriteHtml() const
{
    const std::filesystem::path htmlPath = std::filesystem::path(m_outputDirectory) / L"index.html";
    std::ofstream file(htmlPath);
    if (!file)
    {
        return;
    }

    file <<
        "<!doctype html>\n"
        "<html lang=\"ja\">\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <title>DX11 Debug</title>\n"
        "  <style>\n"
        "    body{margin:0;font-family:Segoe UI,sans-serif;background:#101418;color:#edf2f4;}\n"
        "    main{max-width:840px;margin:0 auto;padding:32px;}\n"
        "    h1{font-size:24px;margin:0 0 20px;}\n"
        "    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;}\n"
        "    .tile{border:1px solid #2d3741;padding:16px;border-radius:8px;background:#151b21;}\n"
        "    .label{color:#9fb0bd;font-size:13px;}\n"
        "    .value{font-size:28px;margin-top:8px;}\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <main>\n"
        "    <h1>DX11 3D Game Debug</h1>\n"
        "    <div class=\"grid\" id=\"grid\"></div>\n"
        "  </main>\n"
        "  <script>\n"
        "    const labels={frameMilliseconds:'Frame ms',estimatedFps:'FPS',fixedUpdateCount:'Fixed Updates'};\n"
        "    async function refresh(){\n"
        "      const data=await fetch('debug_state.json?ts='+Date.now()).then(r=>r.json()).catch(()=>null);\n"
        "      if(!data)return;\n"
        "      const items=[['frameMilliseconds',data.frameMilliseconds],['estimatedFps',data.estimatedFps],['fixedUpdateCount',data.fixedUpdateCount],['CPU Budget',data.budget.cpuMilliseconds],['GPU Budget',data.budget.gpuMilliseconds],['Attack',data.combat.currentAttackId],['Phase',data.combat.currentPhase],['Broad Candidates',data.combat.broadPhaseCandidateCount],['Collisions',data.combat.confirmedCollisionCount]];\n"
        "      document.getElementById('grid').innerHTML=items.map(([k,v])=>`<section class=\"tile\"><div class=\"label\">${labels[k]||k}</div><div class=\"value\">${v}</div></section>`).join('');\n"
        "    }\n"
        "    refresh(); setInterval(refresh, 500);\n"
        "  </script>\n"
        "</body>\n"
        "</html>\n";
}

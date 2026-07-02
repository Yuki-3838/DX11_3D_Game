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
    file << "    \"confirmedCollisionCount\": " << combatState.confirmedCollisionCount << ",\n";
    file << "    \"playerHp\": " << combatState.playerHp << ",\n";
    file << "    \"playerStamina\": " << combatState.playerStamina << ",\n";
    file << "    \"enemyHp\": " << combatState.enemyHp << ",\n";
    file << "    \"distanceMeters\": " << combatState.distanceMeters << ",\n";
    file << "    \"playerGuarding\": " << (combatState.playerGuarding ? "true" : "false") << ",\n";
    file << "    \"enemyInRecovery\": " << (combatState.enemyInRecovery ? "true" : "false") << "\n";
    file << "  }\n";
    file << "}\n";

    const std::filesystem::path scriptPath = std::filesystem::path(m_outputDirectory) / L"debug_state.js";
    std::ofstream script(scriptPath);
    if (!script)
    {
        return;
    }

    script << std::fixed << std::setprecision(3);
    script << "window.__DX11_DEBUG_STATE__ = {\n";
    script << "  frameMilliseconds: " << snapshot.frameMilliseconds << ",\n";
    script << "  estimatedFps: " << snapshot.estimatedFps << ",\n";
    script << "  fixedUpdateCount: " << snapshot.fixedUpdateCount << ",\n";
    script << "  budget: {\n";
    script << "    cpuMilliseconds: " << snapshot.budget.cpuMilliseconds << ",\n";
    script << "    gpuMilliseconds: " << snapshot.budget.gpuMilliseconds << ",\n";
    script << "    systemsMilliseconds: " << snapshot.budget.systemsMilliseconds << ",\n";
    script << "    reserveMilliseconds: " << snapshot.budget.reserveMilliseconds << "\n";
    script << "  },\n";
    script << "  combat: {\n";
    script << "    currentAttackId: \"" << combatState.currentAttackId << "\",\n";
    script << "    currentPhase: \"" << Combat::ToString(combatState.currentPhase) << "\",\n";
    script << "    broadPhaseCandidateCount: " << combatState.broadPhaseCandidateCount << ",\n";
    script << "    confirmedCollisionCount: " << combatState.confirmedCollisionCount << ",\n";
    script << "    playerHp: " << combatState.playerHp << ",\n";
    script << "    playerStamina: " << combatState.playerStamina << ",\n";
    script << "    enemyHp: " << combatState.enemyHp << ",\n";
    script << "    distanceMeters: " << combatState.distanceMeters << ",\n";
    script << "    playerGuarding: " << (combatState.playerGuarding ? "true" : "false") << ",\n";
    script << "    enemyInRecovery: " << (combatState.enemyInRecovery ? "true" : "false") << "\n";
    script << "  }\n";
    script << "};\n";
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
        "    .status{margin:0 0 16px;color:#9fb0bd;font-size:14px;}\n"
        "    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;}\n"
        "    .tile{border:1px solid #2d3741;padding:16px;border-radius:8px;background:#151b21;}\n"
        "    .label{color:#9fb0bd;font-size:13px;}\n"
        "    .value{font-size:28px;margin-top:8px;}\n"
        "    .empty{border:1px solid #3b4652;background:#151b21;padding:16px;border-radius:8px;color:#dbe6ee;}\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <main>\n"
        "    <h1>DX11 3D Game Debug</h1>\n"
        "    <p class=\"status\" id=\"status\">Waiting for debug data...</p>\n"
        "    <div class=\"grid\" id=\"grid\"></div>\n"
        "  </main>\n"
        "  <script>\n"
        "    const labels={frameMilliseconds:'Frame ms',estimatedFps:'FPS',fixedUpdateCount:'Fixed Updates'};\n"
        "    const grid=document.getElementById('grid');\n"
        "    const status=document.getElementById('status');\n"
        "    function render(data,source){\n"
        "      const items=[['frameMilliseconds',data.frameMilliseconds],['estimatedFps',data.estimatedFps],['fixedUpdateCount',data.fixedUpdateCount],['CPU Budget',data.budget.cpuMilliseconds],['GPU Budget',data.budget.gpuMilliseconds],['Attack',data.combat.currentAttackId],['Phase',data.combat.currentPhase],['Player HP',data.combat.playerHp],['Player Stamina',data.combat.playerStamina],['Enemy HP',data.combat.enemyHp],['Distance m',data.combat.distanceMeters],['Guarding',data.combat.playerGuarding],['Enemy Recovery',data.combat.enemyInRecovery],['Broad Candidates',data.combat.broadPhaseCandidateCount],['Collisions',data.combat.confirmedCollisionCount]];\n"
        "      grid.innerHTML=items.map(([k,v])=>`<section class=\"tile\"><div class=\"label\">${labels[k]||k}</div><div class=\"value\">${v}</div></section>`).join('');\n"
        "      status.textContent=`Debug data loaded from ${source}. Keep the game running to update values.`;\n"
        "    }\n"
        "    function loadScriptState(){\n"
        "      return new Promise((resolve)=>{\n"
        "        const old=document.getElementById('debug-state-script'); if(old) old.remove();\n"
        "        const script=document.createElement('script'); script.id='debug-state-script'; script.src='debug_state.js?ts='+Date.now();\n"
        "        script.onload=()=>resolve(window.__DX11_DEBUG_STATE__||null); script.onerror=()=>resolve(null);\n"
        "        document.head.appendChild(script);\n"
        "      });\n"
        "    }\n"
        "    async function refresh(){\n"
        "      let data=await fetch('debug_state.json?ts='+Date.now()).then(r=>r.ok?r.json():null).catch(()=>null);\n"
        "      if(data){ render(data,'debug_state.json'); return; }\n"
        "      data=await loadScriptState();\n"
        "      if(data){ render(data,'debug_state.js'); return; }\n"
        "      status.textContent='No debug data yet. Run DX11_3D_Game.exe once, then reload this page.';\n"
        "      grid.innerHTML='<div class=\"empty\">debug_state.json/debug_state.js was not found or could not be loaded.</div>';\n"
        "    }\n"
        "    refresh(); setInterval(refresh, 500);\n"
        "  </script>\n"
        "</body>\n"
        "</html>\n";
}

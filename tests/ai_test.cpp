/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "aihelper.h"
#include <QTemporaryFile>
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testAiConfigSerialization()
{
    using namespace LeapKAi;

    AiConfig cfg;
    assert(!cfg.enabled);
    assert(!cfg.alwaysSendToAi);
    assert(cfg.backend == AiBackend::LocalLlamaCpp);
    assert(cfg.apiEndpoint == QStringLiteral("https://api.openai.com/v1"));
    assert(cfg.apiModel == QStringLiteral("gpt-4o-mini"));
    assert(cfg.apiProviderPreset == ApiProviderPreset::OpenAI);
    assert(cfg.shortcut == QStringLiteral("Meta+I"));

    // Modify values
    cfg.enabled = true;
    cfg.alwaysSendToAi = true;
    cfg.backend = AiBackend::RemoteOpenAiApi;
    cfg.apiEndpoint = QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai");
    cfg.apiModel = QStringLiteral("gemini-1.5-flash");
    cfg.apiKey = QStringLiteral("AIzaSyFakeKey123");
    cfg.apiProviderPreset = ApiProviderPreset::GoogleGemini;
    cfg.localModelPath = QStringLiteral("/custom/path/model.gguf");
    cfg.shortcut = QStringLiteral("Meta+Shift+A");

    // Serialize to JSON and back
    const QJsonObject json = cfg.toJson();
    const AiConfig restored = AiConfig::fromJson(json);

    assert(restored.enabled == true);
    assert(restored.alwaysSendToAi == true);
    assert(restored.backend == AiBackend::RemoteOpenAiApi);
    assert(restored.apiEndpoint == QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai"));
    assert(restored.apiModel == QStringLiteral("gemini-1.5-flash"));
    assert(restored.apiKey == QStringLiteral("AIzaSyFakeKey123"));
    assert(restored.apiProviderPreset == ApiProviderPreset::GoogleGemini);
    assert(restored.localModelPath == QStringLiteral("/custom/path/model.gguf"));
    assert(restored.shortcut == QStringLiteral("Meta+Shift+A"));

    std::cout << "testAiConfigSerialization passed!" << std::endl;
}

void testProviderPresets()
{
    using namespace LeapKAi;

    // OpenAI
    auto openai = providerInfo(ApiProviderPreset::OpenAI);
    assert(openai.name == QStringLiteral("OpenAI"));
    assert(openai.endpoint == QStringLiteral("https://api.openai.com/v1"));
    assert(openai.defaultModel == QStringLiteral("gpt-4o-mini"));

    // Gemini
    auto gemini = providerInfo(ApiProviderPreset::GoogleGemini);
    assert(gemini.name == QStringLiteral("Google Gemini"));
    assert(gemini.endpoint.contains(QStringLiteral("googleapis.com")));
    assert(gemini.defaultModel == QStringLiteral("gemini-1.5-flash"));

    // Groq
    auto groq = providerInfo(ApiProviderPreset::Groq);
    assert(groq.name.contains(QStringLiteral("Groq")));
    assert(groq.endpoint.contains(QStringLiteral("groq.com")));

    // OpenRouter
    auto openrouter = providerInfo(ApiProviderPreset::OpenRouter);
    assert(openrouter.name == QStringLiteral("OpenRouter"));
    assert(openrouter.endpoint.contains(QStringLiteral("openrouter.ai")));

    // Local Server
    auto local = providerInfo(ApiProviderPreset::LocalServer);
    assert(local.endpoint == QStringLiteral("http://localhost:11434/v1"));

    std::cout << "testProviderPresets passed!" << std::endl;
}

void testModelPathsAndFormatting()
{
    using namespace LeapKAi;

    assert(!defaultModelsDirectory().isEmpty());
    assert(defaultModelsDirectory().endsWith(QStringLiteral("/leapk/models")));
    assert(defaultModelPath().endsWith(QStringLiteral(".gguf")));

    // Byte formatting
    assert(formatBytes(0) == QStringLiteral("0 B"));
    assert(formatBytes(-50) == QStringLiteral("0 B"));
    assert(formatBytes(500 * 1024) == QStringLiteral("0.5 MB"));
    assert(formatBytes(1500 * 1024 * 1024ULL) == QStringLiteral("1.46 GB"));

    std::cout << "testModelPathsAndFormatting passed!" << std::endl;
}

void testModelDetection()
{
    using namespace LeapKAi;

    // Non-existent path
    assert(!isModelInstalled(QStringLiteral("/non/existent/path/model.gguf")));
    assert(modelFileSize(QStringLiteral("/non/existent/path/model.gguf")) == 0);

    // Empty file (<100MB sanity threshold)
    QTemporaryFile tempSmall;
    assert(tempSmall.open());
    tempSmall.write("tiny stub file");
    tempSmall.flush();
    assert(!isModelInstalled(tempSmall.fileName()));
    assert(modelFileSize(tempSmall.fileName()) > 0);

    // Simulated valid model (>100MB)
    QTemporaryFile tempModel;
    assert(tempModel.open());
    const QByteArray dummy(1024 * 1024, 'A'); // 1MB chunk
    for (int i = 0; i < 105; ++i) { // 105 MB
        tempModel.write(dummy);
    }
    tempModel.flush();

    assert(isModelInstalled(tempModel.fileName()));
    assert(modelFileSize(tempModel.fileName()) == 105 * 1024 * 1024);

    std::cout << "testModelDetection passed!" << std::endl;
}

void testLlamaCliDetection()
{
    using namespace LeapKAi;

    const QString cliPath = defaultLlamaCliPath();
    assert(!cliPath.isEmpty());
    assert(cliPath.contains(QStringLiteral("llama-cli")));

    std::cout << "testLlamaCliDetection passed!" << std::endl;
}

void testAiResponseParsing()
{
    using namespace LeapKAi;

    // 1. Single action with index
    {
        const QString json = QStringLiteral("{\"action\": \"focus\", \"index\": 2, \"reason\": \"Focus Konsole\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("focus"));
        assert(acts[0].indices.size() == 1 && acts[0].indices[0] == 2);
        assert(acts[0].reason == QStringLiteral("Focus Konsole"));
    }

    // 2. Tile with indices array
    {
        const QString json = QStringLiteral("{\"action\": \"tile\", \"indices\": [0, 2], \"preset\": \"columns-2\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("tile"));
        assert(acts[0].indices.size() == 2 && acts[0].indices[0] == 0 && acts[0].indices[1] == 2);
        assert(acts[0].preset == QStringLiteral("columns-2"));
    }

    // 3. Tile with index field containing array (model quirk)
    {
        const QString json = QStringLiteral("{\"action\": \"tile\", \"index\": [1, 3]}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].indices.size() == 2 && acts[0].indices[0] == 1 && acts[0].indices[1] == 3);
    }

    // 4. Fullscreen with explicit state
    {
        const QString json = QStringLiteral("{\"action\": \"fullscreen\", \"index\": 0, \"state\": true, \"reason\": \"Full screen\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("fullscreen"));
        assert(acts[0].indices[0] == 0);
        assert(acts[0].hasExplicitState && acts[0].state == true);
    }

    // 5. Move to new workspace
    {
        const QString json = QStringLiteral("{\"action\": \"move_workspace\", \"target_workspace\": \"new\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("move_workspace"));
        assert(acts[0].targetWorkspace == QStringLiteral("new"));
    }

    // 6. Move to named workspace with indices
    {
        const QString json = QStringLiteral("{\"action\": \"move_workspace\", \"indices\": [0, 1], \"workspace\": \"Coding\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 1);
        assert(acts[0].indices.size() == 2);
        assert(acts[0].targetWorkspace == QStringLiteral("Coding"));
    }

    // 7. Multi-action response
    {
        const QString json = QStringLiteral("{\"actions\": [{\"action\": \"fullscreen\", \"index\": 0}, {\"action\": \"focus\", \"index\": 0}], \"reason\": \"Done\"}");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 2);
        assert(acts[0].action == QStringLiteral("fullscreen"));
        assert(acts[1].action == QStringLiteral("focus"));
        assert(acts[0].reason == QStringLiteral("Done"));
    }

    // 8. Top-level array
    {
        const QString json = QStringLiteral("[{\"action\": \"close\", \"index\": 1}, {\"action\": \"close\", \"index\": 2}]");
        const auto acts = parseAiResponse(json);
        assert(acts.size() == 2);
        assert(acts[0].action == QStringLiteral("close") && acts[0].indices[0] == 1);
        assert(acts[1].action == QStringLiteral("close") && acts[1].indices[0] == 2);
    }

    // 9. Markdown fence wrapped
    {
        const QString raw = QStringLiteral("```json\n{\"action\": \"minimize\", \"index\": 1}\n```");
        const auto acts = parseAiResponse(raw);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("minimize"));
        assert(acts[0].indices[0] == 1);
    }

    // 10. Conversational wrapper
    {
        const QString raw = QStringLiteral("Certainly! Here is your command:\n{\"action\": \"restore\", \"index\": 3}\nHope that helps!");
        const auto acts = parseAiResponse(raw);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("restore"));
        assert(acts[0].indices[0] == 3);
    }

    // 11. Target app fallback
    {
        const QString raw = QStringLiteral("{\"action\": \"focus\", \"window\": \"firefox\"}");
        const auto acts = parseAiResponse(raw);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("focus"));
        assert(acts[0].targetApp == QStringLiteral("firefox"));
    }

    // 12. Invalid response
    {
        const QString raw = QStringLiteral("Sorry, I cannot do that.");
        const auto acts = parseAiResponse(raw);
        assert(acts.isEmpty());
    }

    // 13. Real llama-cli output with banner, echoed prompt containing guideline braces, and trailing text
    {
        const QString raw = QStringLiteral(
            "\n\nLoading model... \n\n"
            "build      : b12004-3c809f443\n"
            "model      : /home/greg/.local/share/leapk/models/gemma-2-2b-it-Q4_K_M.gguf\n\n"
            "> You are an intelligent window manager assistant for KDE Plasma LeapK.\n"
            "Windows on screen:\n"
            "[0] class=\"konsole org.kde.konsole\", title=\"~ : bash — Konsole\"\n"
            "[1] class=\"dolphin org.kde.dolphin\", title=\"Dolphin\"\n\n"
            "Supported actions:\n"
            "- focus: {\"action\": \"focus\", \"index\": <num>}\n"
            "- tile: {\"action\": \"tile\", \"indices\": [<num>, ...]}\n\n"
            "User command: Focus terminal\n"
            "JSON:\n"
            "```json\n"
            "{\"action\": \"focus\", \"index\": 0, \"reason\": \"Focus terminal\"}\n"
            "```\n\n\nExiting...\n"
        );
        const auto acts = parseAiResponse(raw);
        assert(acts.size() == 1);
        assert(acts[0].action == QStringLiteral("focus"));
        assert(acts[0].indices.size() == 1 && acts[0].indices[0] == 0);
        assert(acts[0].reason == QStringLiteral("Focus terminal"));
    }

    std::cout << "testAiResponseParsing passed!" << std::endl;
}

void testAiSystemPrompt()
{
    using namespace LeapKAi;

    QStringList wins;
    wins << QStringLiteral("[0] class=\"firefox\", title=\"Mozilla Firefox\" active");
    wins << QStringLiteral("[1] class=\"org.kde.konsole\", title=\"Terminal\"");

    QStringList ws;
    ws << QStringLiteral("[1] \"Coding\"");
    ws << QStringLiteral("[2] \"Chat\"");

    const QString prompt = buildAiSystemPrompt(wins, ws, QStringLiteral("Virtual desktops: 2"));
    assert(prompt.contains(QStringLiteral("Virtual desktops: 2")));
    assert(prompt.contains(QStringLiteral("Coding")));
    assert(prompt.contains(QStringLiteral("Mozilla Firefox")));
    assert(prompt.contains(QStringLiteral("move_workspace")));
    assert(prompt.contains(QStringLiteral("fullscreen")));

    std::cout << "testAiSystemPrompt passed!" << std::endl;
}

int main(int argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testAiConfigSerialization();
    testProviderPresets();
    testModelPathsAndFormatting();
    testModelDetection();
    testLlamaCliDetection();
    testAiResponseParsing();
    testAiSystemPrompt();

    std::cout << "All AI tests passed successfully!" << std::endl;
    return 0;
}

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
    assert(cfg.backend == AiBackend::LocalLlamaCpp);
    assert(cfg.apiEndpoint == QStringLiteral("https://api.openai.com/v1"));
    assert(cfg.apiModel == QStringLiteral("gpt-4o-mini"));
    assert(cfg.apiProviderPreset == ApiProviderPreset::OpenAI);
    assert(cfg.shortcut == QStringLiteral("Meta+I"));

    // Modify values
    cfg.enabled = true;
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

int main(int argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testAiConfigSerialization();
    testProviderPresets();
    testModelPathsAndFormatting();
    testModelDetection();
    testLlamaCliDetection();

    std::cout << "All AI tests passed successfully!" << std::endl;
    return 0;
}

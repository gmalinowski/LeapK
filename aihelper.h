// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QList>

namespace LeapKAi
{

enum class AiBackend {
    LocalLlamaCpp = 0,
    RemoteOpenAiApi = 1
};

enum class ApiProviderPreset {
    Custom = 0,
    OpenAI = 1,
    GoogleGemini = 2,
    Groq = 3,
    OpenRouter = 4,
    LocalServer = 5
};

struct ProviderInfo {
    QString name;
    QString endpoint;
    QString defaultModel;
    QString docsUrl;
};

inline ProviderInfo providerInfo(ApiProviderPreset preset)
{
    switch (preset) {
    case ApiProviderPreset::OpenAI:
        return {
            QStringLiteral("OpenAI"),
            QStringLiteral("https://api.openai.com/v1"),
            QStringLiteral("gpt-4o-mini"),
            QStringLiteral("https://platform.openai.com")
        };
    case ApiProviderPreset::GoogleGemini:
        return {
            QStringLiteral("Google Gemini"),
            QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai"),
            QStringLiteral("gemini-1.5-flash"),
            QStringLiteral("https://aistudio.google.com")
        };
    case ApiProviderPreset::Groq:
        return {
            QStringLiteral("Groq (Ultra-Fast)"),
            QStringLiteral("https://api.groq.com/openai/v1"),
            QStringLiteral("llama-3.2-3b-preview"),
            QStringLiteral("https://console.groq.com")
        };
    case ApiProviderPreset::OpenRouter:
        return {
            QStringLiteral("OpenRouter"),
            QStringLiteral("https://openrouter.ai/api/v1"),
            QStringLiteral("google/gemma-2-9b-it:free"),
            QStringLiteral("https://openrouter.ai")
        };
    case ApiProviderPreset::LocalServer:
        return {
            QStringLiteral("Local Server (Ollama / vLLM / LocalAI)"),
            QStringLiteral("http://localhost:11434/v1"),
            QStringLiteral("gemma2:2b"),
            QStringLiteral("http://localhost:11434")
        };
    case ApiProviderPreset::Custom:
    default:
        return {
            QStringLiteral("Custom / OpenAI-compatible"),
            QStringLiteral("https://api.openai.com/v1"),
            QStringLiteral("gpt-4o-mini"),
            QString()
        };
    }
}

constexpr const char *DEFAULT_MODEL_FILENAME = "gemma-2-2b-it-Q4_K_M.gguf";
constexpr const char *DEFAULT_MODEL_NAME = "Google Gemma 2 2B Instruct (Q4_K_M)";
constexpr const char *DEFAULT_MODEL_DOWNLOAD_URL =
    "https://huggingface.co/bartowski/gemma-2-2b-it-GGUF/resolve/main/gemma-2-2b-it-Q4_K_M.gguf";

inline QString defaultModelsDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + QStringLiteral("/leapk/models");
}

inline QString defaultModelPath()
{
    return defaultModelsDirectory() + QLatin1Char('/') + QLatin1String(DEFAULT_MODEL_FILENAME);
}

inline QString defaultLlamaCliPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString localLeapk = base + QStringLiteral("/leapk/bin/llama-cli");
    if (QFileInfo::exists(localLeapk)) {
        const QFileInfo fi(localLeapk);
        if (fi.isSymLink()) {
            return fi.canonicalFilePath();
        }
        return localLeapk;
    }
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("llama-cli"));
    if (!inPath.isEmpty()) {
        const QFileInfo fi(inPath);
        if (fi.isSymLink()) {
            return fi.canonicalFilePath();
        }
        return inPath;
    }
    return localLeapk;
}

inline bool isLlamaCliInstalled()
{
    const QString path = defaultLlamaCliPath();
    const QFileInfo fi(path);
    return fi.exists() && fi.isExecutable();
}

inline bool isModelInstalled(const QString &path)
{
    const QString targetPath = path.isEmpty() ? defaultModelPath() : path;
    const QFileInfo fi(targetPath);
    return fi.exists() && fi.isFile() && fi.size() > 100 * 1024 * 1024; // > 100MB minimum sanity check
}

inline qint64 modelFileSize(const QString &path)
{
    const QString targetPath = path.isEmpty() ? defaultModelPath() : path;
    const QFileInfo fi(targetPath);
    return fi.exists() ? fi.size() : 0;
}

inline QString formatBytes(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("0 B");
    }
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) {
        return QString::asprintf("%.2f GB", gb);
    }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QString::asprintf("%.1f MB", mb);
}

struct AiConfig {
    bool enabled = false;
    bool alwaysSendToAi = false;
    AiBackend backend = AiBackend::LocalLlamaCpp;
    QString localModelPath = defaultModelPath();
    QString apiEndpoint = QStringLiteral("https://api.openai.com/v1");
    QString apiModel = QStringLiteral("gpt-4o-mini");
    QString apiKey;
    ApiProviderPreset apiProviderPreset = ApiProviderPreset::OpenAI;
    QString shortcut = QStringLiteral("Meta+I");

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("enabled")] = enabled;
        obj[QStringLiteral("alwaysSendToAi")] = alwaysSendToAi;
        obj[QStringLiteral("backend")] = static_cast<int>(backend);
        obj[QStringLiteral("localModelPath")] = localModelPath;
        obj[QStringLiteral("apiEndpoint")] = apiEndpoint;
        obj[QStringLiteral("apiModel")] = apiModel;
        obj[QStringLiteral("apiKey")] = apiKey;
        obj[QStringLiteral("apiProviderPreset")] = static_cast<int>(apiProviderPreset);
        obj[QStringLiteral("shortcut")] = shortcut;
        return obj;
    }

    static AiConfig fromJson(const QJsonObject &obj)
    {
        AiConfig cfg;
        if (obj.contains(QStringLiteral("enabled"))) {
            cfg.enabled = obj[QStringLiteral("enabled")].toBool(false);
        }
        if (obj.contains(QStringLiteral("alwaysSendToAi"))) {
            cfg.alwaysSendToAi = obj[QStringLiteral("alwaysSendToAi")].toBool(false);
        }
        if (obj.contains(QStringLiteral("backend"))) {
            cfg.backend = static_cast<AiBackend>(obj[QStringLiteral("backend")].toInt(0));
        }
        if (obj.contains(QStringLiteral("localModelPath"))) {
            cfg.localModelPath = obj[QStringLiteral("localModelPath")].toString(defaultModelPath());
        }
        if (obj.contains(QStringLiteral("apiEndpoint"))) {
            cfg.apiEndpoint = obj[QStringLiteral("apiEndpoint")].toString(QStringLiteral("https://api.openai.com/v1"));
        }
        if (obj.contains(QStringLiteral("apiModel"))) {
            cfg.apiModel = obj[QStringLiteral("apiModel")].toString(QStringLiteral("gpt-4o-mini"));
        }
        if (obj.contains(QStringLiteral("apiKey"))) {
            cfg.apiKey = obj[QStringLiteral("apiKey")].toString();
        }
        if (obj.contains(QStringLiteral("apiProviderPreset"))) {
            cfg.apiProviderPreset = static_cast<ApiProviderPreset>(obj[QStringLiteral("apiProviderPreset")].toInt(1));
        }
        if (obj.contains(QStringLiteral("shortcut"))) {
            cfg.shortcut = obj[QStringLiteral("shortcut")].toString(QStringLiteral("Meta+I"));
        }
        return cfg;
    }
};

} // namespace LeapKAi

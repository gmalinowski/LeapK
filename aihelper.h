// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QStringList>

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

struct ParsedAiAction {
    QString action; // "focus", "tile", "float", "fullscreen", "maximize", "minimize", "restore", "close", "move_workspace", "switch_workspace", "stash", "unstash"
    QList<int> indices;
    QString targetApp;
    QString preset = QStringLiteral("columns-2");
    QString targetWorkspace; // e.g. "new", "1", "Chat"
    int targetDesktop = 0;
    bool hasExplicitState = false;
    bool state = true;
    QString reason;
};

inline ParsedAiAction parseSingleAiAction(const QJsonObject &obj)
{
    ParsedAiAction act;
    act.action = obj.value(QStringLiteral("action")).toString().trimmed().toLower();
    act.reason = obj.value(QStringLiteral("reason")).toString().trimmed();
    act.preset = obj.value(QStringLiteral("preset")).toString(QStringLiteral("columns-2")).trimmed();

    // Check indices / index / windows / window / app
    QJsonValue idxVal = obj.value(QStringLiteral("indices"));
    if (idxVal.isUndefined() || idxVal.isNull()) {
        idxVal = obj.value(QStringLiteral("index"));
    }
    if (idxVal.isUndefined() || idxVal.isNull()) {
        idxVal = obj.value(QStringLiteral("windows"));
    }
    if (idxVal.isUndefined() || idxVal.isNull()) {
        idxVal = obj.value(QStringLiteral("window"));
    }

    if (idxVal.isArray()) {
        const QJsonArray arr = idxVal.toArray();
        for (const QJsonValue &v : arr) {
            if (v.isDouble()) {
                act.indices.append(v.toInt());
            } else if (v.isString()) {
                bool ok = false;
                const int parsed = v.toString().toInt(&ok);
                if (ok) {
                    act.indices.append(parsed);
                } else if (act.targetApp.isEmpty()) {
                    act.targetApp = v.toString().trimmed();
                }
            }
        }
    } else if (idxVal.isDouble()) {
        act.indices.append(idxVal.toInt());
    } else if (idxVal.isString()) {
        bool ok = false;
        const int parsed = idxVal.toString().toInt(&ok);
        if (ok) {
            act.indices.append(parsed);
        } else {
            act.targetApp = idxVal.toString().trimmed();
        }
    }

    if (act.targetApp.isEmpty() && obj.contains(QStringLiteral("app"))) {
        act.targetApp = obj.value(QStringLiteral("app")).toString().trimmed();
    }

    // Parse state (e.g. for fullscreen / maximize)
    if (obj.contains(QStringLiteral("state"))) {
        act.hasExplicitState = true;
        act.state = obj.value(QStringLiteral("state")).toBool(true);
    }

    // Parse target_workspace / workspace / target
    if (obj.contains(QStringLiteral("target_workspace"))) {
        const QJsonValue wsVal = obj.value(QStringLiteral("target_workspace"));
        act.targetWorkspace = wsVal.isDouble() ? QString::number(wsVal.toInt()) : wsVal.toString().trimmed();
    } else if (obj.contains(QStringLiteral("workspace"))) {
        const QJsonValue wsVal = obj.value(QStringLiteral("workspace"));
        act.targetWorkspace = wsVal.isDouble() ? QString::number(wsVal.toInt()) : wsVal.toString().trimmed();
    } else if (obj.contains(QStringLiteral("target"))) {
        const QJsonValue wsVal = obj.value(QStringLiteral("target"));
        act.targetWorkspace = wsVal.isDouble() ? QString::number(wsVal.toInt()) : wsVal.toString().trimmed();
    }

    // Parse target_desktop / desktop
    if (obj.contains(QStringLiteral("target_desktop"))) {
        act.targetDesktop = obj.value(QStringLiteral("target_desktop")).toInt(0);
    } else if (obj.contains(QStringLiteral("desktop"))) {
        act.targetDesktop = obj.value(QStringLiteral("desktop")).toInt(0);
    }

    return act;
}

inline QString extractBalancedJson(const QString &str)
{
    const int lastBrace = str.lastIndexOf(QLatin1Char('}'));
    const int lastBracket = str.lastIndexOf(QLatin1Char(']'));

    if (lastBrace == -1 && lastBracket == -1) {
        return QString();
    }

    if (lastBracket > lastBrace) {
        // Array ends last: scan backwards to find matching '['
        int depth = 0;
        bool inStr = false;
        bool escaped = false;
        for (int i = lastBracket; i >= 0; --i) {
            const QChar c = str.at(i);
            if (c == QLatin1Char('"') && !escaped) {
                inStr = !inStr;
            } else if (!inStr) {
                if (c == QLatin1Char(']')) {
                    depth++;
                } else if (c == QLatin1Char('[')) {
                    depth--;
                    if (depth == 0) {
                        return str.mid(i, lastBracket - i + 1);
                    }
                }
            }
            escaped = (c == QLatin1Char('\\') && !escaped);
        }
    } else {
        // Object ends last: scan backwards to find matching '{'
        int depth = 0;
        bool inStr = false;
        bool escaped = false;
        for (int i = lastBrace; i >= 0; --i) {
            const QChar c = str.at(i);
            if (c == QLatin1Char('"') && !escaped) {
                inStr = !inStr;
            } else if (!inStr) {
                if (c == QLatin1Char('}')) {
                    depth++;
                } else if (c == QLatin1Char('{')) {
                    depth--;
                    if (depth == 0) {
                        return str.mid(i, lastBrace - i + 1);
                    }
                }
            }
            escaped = (c == QLatin1Char('\\') && !escaped);
        }
    }
    return QString();
}

inline QList<ParsedAiAction> parseAiResponse(const QString &text)
{
    QList<ParsedAiAction> result;
    if (text.trimmed().isEmpty()) {
        return result;
    }

    // 1. If output contains the prompt marker "JSON:", strip prompt banner & instructions
    QString candidate = text;
    const int jsonMarker = text.lastIndexOf(QStringLiteral("JSON:"));
    if (jsonMarker != -1) {
        candidate = text.mid(jsonMarker + 5).trimmed();
    }

    // 2. Extract balanced JSON block (object or array)
    QString jsonCandidate = extractBalancedJson(candidate);
    if (jsonCandidate.isEmpty() && candidate != text) {
        // Fallback to searching the full text
        jsonCandidate = extractBalancedJson(text);
    }

    if (jsonCandidate.isEmpty()) {
        return result;
    }

    const QByteArray jsonBytes = jsonCandidate.toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);

    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        for (const QJsonValue &v : arr) {
            if (v.isObject()) {
                result.append(parseSingleAiAction(v.toObject()));
            }
        }
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("actions")) && obj.value(QStringLiteral("actions")).isArray()) {
            const QJsonArray arr = obj.value(QStringLiteral("actions")).toArray();
            for (const QJsonValue &v : arr) {
                if (v.isObject()) {
                    auto act = parseSingleAiAction(v.toObject());
                    if (act.reason.isEmpty() && obj.contains(QStringLiteral("reason"))) {
                        act.reason = obj.value(QStringLiteral("reason")).toString();
                    }
                    result.append(act);
                }
            }
        } else {
            result.append(parseSingleAiAction(obj));
        }
    }

    return result;
}

inline QString buildAiSystemPrompt(const QStringList &winDescriptions, const QStringList &workspaces = {}, const QString &desktopInfo = {})
{
    QString prompt = QStringLiteral("You are an intelligent window manager assistant for KDE Plasma LeapK.\n");
    if (!desktopInfo.isEmpty()) {
        prompt += desktopInfo + QLatin1Char('\n');
    }
    if (!workspaces.isEmpty()) {
        prompt += QStringLiteral("Workspaces:\n") + workspaces.join(QLatin1Char('\n')) + QStringLiteral("\n\n");
    }
    prompt += QStringLiteral("Windows on screen:\n");
    if (winDescriptions.isEmpty()) {
        prompt += QStringLiteral("(none)\n");
    } else {
        prompt += winDescriptions.join(QLatin1Char('\n')) + QLatin1Char('\n');
    }

    prompt += QStringLiteral(
        "\nSupported actions:\n"
        "- focus: {\"action\": \"focus\", \"index\": <num>}\n"
        "- tile: {\"action\": \"tile\", \"indices\": [<num>, ...], \"preset\": \"columns-2\"|\"columns-3\"|\"master-stack\"|\"grid-4\"|\"monocle\"}\n"
        "- float: {\"action\": \"float\"}\n"
        "- fullscreen: {\"action\": \"fullscreen\", \"index\": <num>, \"state\": true|false}\n"
        "- maximize: {\"action\": \"maximize\", \"index\": <num>, \"state\": true|false}\n"
        "- minimize: {\"action\": \"minimize\", \"index\": <num>}\n"
        "- restore: {\"action\": \"restore\", \"index\": <num>}\n"
        "- close: {\"action\": \"close\", \"index\": <num>}\n"
        "- move_workspace: {\"action\": \"move_workspace\", \"indices\": [<num>, ...], \"target_workspace\": \"new\"|<num>|\"<name>\"}\n"
        "- switch_workspace: {\"action\": \"switch_workspace\", \"target_workspace\": <num>|\"<name>\"}\n"
        "- stash: {\"action\": \"stash\"}\n"
        "- unstash: {\"action\": \"unstash\"}\n\n"
        "Guidelines:\n"
        "- You may return a single JSON action object or {\"actions\": [ ... ]} for multi-step operations.\n"
        "- App class names may differ from common user terms (e.g. \"org.kde.konsole\" is terminal, \"org.kde.dolphin\" is file manager, \"code\" is editor). Map user terms to the best matching window.\n"
        "- If the user names an app (e.g. \"firefox\", \"focus terminal\", \"show konsole\"), return action \"focus\".\n"
        "- For tiling, identify the requested windows and return their indices in \"indices\" (e.g. [0, 1]).\n"
        "- If user asks to move windows to a new workspace (e.g. \"move windows to new workspace\"), return action \"move_workspace\" with \"target_workspace\": \"new\".\n"
        "- Respond ONLY with valid JSON (no markdown backticks, no conversational preamble, no text outside JSON).\n\n"
        "Examples:\n"
        "User: firefox and signal tilling\n"
        "JSON: {\"action\": \"tile\", \"indices\": [0, 1], \"preset\": \"columns-2\", \"reason\": \"Tile Firefox and Signal\"}\n\n"
        "User: move windows to new workspace\n"
        "JSON: {\"action\": \"move_workspace\", \"target_workspace\": \"new\", \"reason\": \"Move windows to new workspace\"}\n\n"
        "User: firefox full screen\n"
        "JSON: {\"action\": \"fullscreen\", \"index\": 0, \"state\": true, \"reason\": \"Make Firefox fullscreen\"}\n\n"
        "User: focus terminal\n"
        "JSON: {\"action\": \"focus\", \"index\": 2, \"reason\": \"Switch to terminal\"}\n"
    );

    return prompt;
}

} // namespace LeapKAi

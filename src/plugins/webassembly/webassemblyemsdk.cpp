// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "webassemblyemsdk.h"

#include <coreplugin/icore.h>
#include <coreplugin/settingsdatabase.h>

#include <utils/environment.h>
#include <utils/process.h>
#include <utils/hostosinfo.h>

#include <QCache>

using namespace Utils;

namespace WebAssembly::Internal::WebAssemblyEmSdk {

const char emSdkEnvTSFileKey[] = "WebAssembly/emSdkEnvTimeStampFile";
const char emSdkEnvTSKey[] = "WebAssembly/emSdkEnvTimeStamp";
const char emSdkEnvOutputKey[] = "WebAssembly/emSdkEnvOutput1";

const char emSdkVersionTSFileKey[] = "WebAssembly/emSdkVersionTimeStampFile";
const char emSdkVersionTSKey[] = "WebAssembly/emSdkVersionTimeStamp";
const char emSdkVersionKey[] = "WebAssembly/emSdkVersion1";

const FilePath timeStampFile(const FilePath &sdkRoot)
{
    return sdkRoot / ".emscripten";
}

static QString emSdkEnvOutput(const FilePath &sdkRoot)
{
    const FilePath tsFile = timeStampFile(sdkRoot); // ts == Timestamp
    if (!tsFile.exists())
        return {};
    const QDateTime ts = tsFile.lastModified();

    Core::SettingsDatabase *db = Core::ICore::settingsDatabase();
    if (db->value(emSdkEnvTSKey).toDateTime() == ts
        && FilePath::fromVariant(db->value(emSdkEnvTSFileKey)) == tsFile
        && db->contains(emSdkEnvOutputKey)) {
        return db->value(emSdkEnvOutputKey).toString();
    }

    const bool isWindows = sdkRoot.osType() == OsTypeWindows;
    const FilePath scriptFile = sdkRoot.pathAppended(QLatin1String("emsdk_env") +
                                                     (isWindows ? ".bat" : ".sh"));
    Process emSdkEnv;
    if (isWindows) {
        emSdkEnv.setCommand(CommandLine(scriptFile));
    } else {
        // File needs to be source'd, not executed.
        emSdkEnv.setCommand({sdkRoot.withNewPath("bash"), {"-c", ". " + scriptFile.path()}});
    }
    emSdkEnv.runBlocking();
    const QString result = emSdkEnv.allOutput();
    db->setValue(emSdkEnvTSFileKey, tsFile.toVariant());
    db->setValue(emSdkEnvTSKey, ts);
    db->setValue(emSdkEnvOutputKey, result);

    return result;
}

void parseEmSdkEnvOutputAndAddToEnv(const QString &output, Environment &env)
{
    const QStringList lines = output.split('\n');

    for (const QString &line : lines) {
        const QStringList prependParts = line.trimmed().split(" += ");
        if (prependParts.count() == 2)
            env.prependOrSetPath(FilePath::fromUserInput(prependParts.last()));

        const QStringList setParts = line.trimmed().split(" = ");
        if (setParts.count() == 2) {
            if (setParts.first() != "PATH") // Path was already extended above
                env.set(setParts.first(), setParts.last());
            continue;
        }
    }

    // QTCREATORBUG-26199: Wrapper scripts (e.g. emcc.bat) of older emsdks might not find python
    const QString emsdkPython = env.value("EMSDK_PYTHON");
    if (!emsdkPython.isEmpty())
        env.appendOrSetPath(FilePath::fromUserInput(emsdkPython).parentDir());
}

bool isValid(const FilePath &sdkRoot)
{
    return !version(sdkRoot).isNull();
}

void addToEnvironment(const FilePath &sdkRoot, Environment &env)
{
    if (sdkRoot.exists())
        parseEmSdkEnvOutputAndAddToEnv(emSdkEnvOutput(sdkRoot), env);
}

QVersionNumber version(const FilePath &sdkRoot)
{
    const FilePath tsFile = timeStampFile(sdkRoot); // ts == Timestamp
    if (!tsFile.exists())
        return {};
    const QDateTime ts = tsFile.lastModified();

    Core::SettingsDatabase *db = Core::ICore::settingsDatabase();
    if (db->value(emSdkVersionTSKey).toDateTime() == ts
        && FilePath::fromVariant(db->value(emSdkVersionTSFileKey)) == tsFile
        && db->contains(emSdkVersionKey)) {
        return QVersionNumber::fromString(db->value(emSdkVersionKey).toString());
    }

    Environment env = sdkRoot.deviceEnvironment();
    addToEnvironment(sdkRoot, env);
    QLatin1String scriptFile{sdkRoot.osType() == OsType::OsTypeWindows ? "emcc.bat" : "emcc"};
    FilePath script = sdkRoot.withNewPath(scriptFile).searchInDirectories(env.path());
    const CommandLine command(script, {"-dumpversion"});
    Process emcc;
    emcc.setCommand(command);
    emcc.setEnvironment(env);
    emcc.runBlocking();
    const QString versionStr = emcc.cleanedStdOut();
    const QVersionNumber result = QVersionNumber::fromString(versionStr);
    db->setValue(emSdkVersionTSFileKey, tsFile.toVariant());
    db->setValue(emSdkVersionTSKey, ts);
    db->setValue(emSdkVersionKey, result.toString());
    return result;
}

void clearCaches()
{
    Core::SettingsDatabase *db = Core::ICore::settingsDatabase();
    db->remove(emSdkEnvTSFileKey);
    db->remove(emSdkEnvTSKey);
    db->remove(emSdkEnvOutputKey);
    db->remove(emSdkVersionTSFileKey);
    db->remove(emSdkVersionTSKey);
    db->remove(emSdkVersionKey);
}

} // namespace WebAssembly::Internal::WebAssemblyEmSdk

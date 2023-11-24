// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "webassemblyplugin.h"

#ifdef WITH_TESTS
#include "webassembly_test.h"
#endif // WITH_TESTS
#include "webassemblyconstants.h"
#include "webassemblydevice.h"
#include "webassemblyqtversion.h"
#include "webassemblyrunconfiguration.h"
#include "webassemblytoolchain.h"
#include "webassemblytr.h"

#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <extensionsystem/iplugin.h>

#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitmanager.h>

#include <utils/infobar.h>

#include <QTimer>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace WebAssembly::Internal {

static void askUserAboutEmSdkSetup()
{
    const char setupWebAssemblyEmSdk[] = "SetupWebAssemblyEmSdk";

    if (!ICore::infoBar()->canInfoBeAdded(setupWebAssemblyEmSdk)
            || !WebAssemblyQtVersion::isQtVersionInstalled()
            || WebAssemblyToolChain::areToolChainsRegistered())
        return;

    InfoBarEntry info(setupWebAssemblyEmSdk,
                      Tr::tr("Setup Emscripten SDK for WebAssembly? "
                         "To do it later, select Edit > Preferences > Devices > WebAssembly."),
                      InfoBarEntry::GlobalSuppression::Enabled);
    info.addCustomButton(Tr::tr("Setup Emscripten SDK"), [setupWebAssemblyEmSdk] {
        ICore::infoBar()->removeInfo(setupWebAssemblyEmSdk);
        QTimer::singleShot(0, []() { ICore::showOptionsDialog(Constants::SETTINGS_ID); });
    });
    ICore::infoBar()->addInfo(info);
}

class WebAssemblyPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "WebAssembly.json")

public:
    void initialize() final
    {
        setupWebAssemblyToolchain();
        setupWebAssemblyDevice();
        setupWebAssemblyQtVersion();
        setupEmrunRunSupport();

#ifdef WITH_TESTS
        addTest<WebAssemblyTest>();
#endif // WITH_TESTS
    }
    void extensionsInitialized() final
    {
        connect(KitManager::instance(), &KitManager::kitsLoaded, this, [] {
            DeviceManager::instance()->addDevice(createWebAssemblyDevice());
            askUserAboutEmSdkSetup();
        });
    }
};

} // WebAssembly::Internal

#include "webassemblyplugin.moc"

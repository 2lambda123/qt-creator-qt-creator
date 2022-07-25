/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include <debugger/debuggeractions.h>
#include <debugger/debuggerconstants.h>
#include <debugger/debuggercore.h>
#include <debugger/debuggerinternalconstants.h>
#include <debugger/debuggertr.h>

#include <coreplugin/dialogs/ioptionspage.h>

#include <utils/layoutbuilder.h>

using namespace Core;
using namespace Utils;

namespace Debugger::Internal {

/////////////////////////////////////////////////////////////////////////
//
// GdbOptionsPage - harmless options
//
/////////////////////////////////////////////////////////////////////////

class GdbOptionsPage : public Core::IOptionsPage
{
public:
    GdbOptionsPage()
    {
        setId("M.Gdb");
        setDisplayName(Tr::tr("GDB"));
        setCategory(Constants::DEBUGGER_SETTINGS_CATEGORY);
        setSettings(&debuggerSettings()->page2);

        setLayouter([](QWidget *w) {
            using namespace Layouting;
            DebuggerSettings &s = *debuggerSettings();

            Group general {
                title(Tr::tr("General")),
                Column {
                    Row { s.gdbWatchdogTimeout, st },
                    s.skipKnownFrames,
                    s.useMessageBoxForSignals,
                    s.adjustBreakpointLocations,
                    s.useDynamicType,
                    s.loadGdbInit,
                    s.loadGdbDumpers,
                    s.intelFlavor,
                    s.usePseudoTracepoints,
                    s.useIndexCache,
                    st
                 }
            };

            Column commands {
                Group {
                    title(Tr::tr("Additional Startup Commands")),
                    Column { s.gdbStartupCommands }
                },
                Group {
                    title(Tr::tr("Additional Attach Commands")),
                    Column { s.gdbPostAttachCommands },
                },
                st
            };

            Row { general, commands }.attachTo(w);
        });
    }
};

/////////////////////////////////////////////////////////////////////////
//
// GdbOptionsPage2 - dangerous options
//
/////////////////////////////////////////////////////////////////////////

// The "Dangerous" options.
class GdbOptionsPage2 : public Core::IOptionsPage
{
public:
    GdbOptionsPage2()
    {
        setId("M.Gdb2");
        setDisplayName(Tr::tr("GDB Extended"));
        setCategory(Constants::DEBUGGER_SETTINGS_CATEGORY);
        setSettings(&debuggerSettings()->page3);

        setLayouter([](QWidget *w) {
            auto labelDangerous = new QLabel("<html><head/><body><i>" +
                Tr::tr("The options below give access to advanced "
                "or experimental functions of GDB.<br>Enabling them may negatively "
                "impact your debugging experience.") + "</i></body></html>");

            using namespace Layouting;
            DebuggerSettings &s = *debuggerSettings();

            Group extended {
                Title(Tr::tr("Extended")),
                Column {
                    labelDangerous,
                    s.targetAsync,
                    s.autoEnrichParameters,
                    s.breakOnWarning,
                    s.breakOnFatal,
                    s.breakOnAbort,
                    s.enableReverseDebugging,
                    s.multiInferior,
                }
            };

            Column { extended, st }.attachTo(w);
        });
    }
};

// Registration

void addGdbOptionPages(QList<IOptionsPage *> *opts)
{
    opts->push_back(new GdbOptionsPage);
    opts->push_back(new GdbOptionsPage2);
}

} // Debugger::Internal

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

#include "vcscommandpage.h"

#include "../vcsbaseplugin.h"
#include "../vcscommand.h"

#include <coreplugin/vcsmanager.h>

#include <projectexplorer/jsonwizard/jsonwizard.h>

#include <utils/algorithm.h>
#include <utils/commandline.h>
#include <utils/outputformatter.h>
#include <utils/qtcassert.h>
#include <utils/theme/theme.h>

#include <QAbstractButton>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace VcsBase {
namespace Internal {

static char VCSCOMMAND_VCSID[] = "vcsId";
static char VCSCOMMAND_RUN_MESSAGE[] = "trRunMessage";
static char VCSCOMMAND_REPO[] = "repository";
static char VCSCOMMAND_DIR[] = "baseDirectory";
static char VCSCOMMAND_EXTRA_ARGS[] = "extraArguments";
static char VCSCOMMAND_CHECKOUTNAME[] = "checkoutName";

static char VCSCOMMAND_JOBS[] = "extraJobs";
static char JOB_SKIP_EMPTY[] = "skipIfEmpty";
static char JOB_WORK_DIRECTORY[] = "directory";
static char JOB_COMMAND[] = "command";
static char JOB_ARGUMENTS[] = "arguments";
static char JOB_TIME_OUT[] = "timeoutFactor";
static char JOB_ENABLED[] = "enabled";

// ----------------------------------------------------------------------
// VcsCommandPageFactory:
// ----------------------------------------------------------------------

VcsCommandPageFactory::VcsCommandPageFactory()
{
    setTypeIdsSuffix(QLatin1String("VcsCommand"));
}

WizardPage *VcsCommandPageFactory::create(JsonWizard *wizard, Id typeId, const QVariant &data)
{
    Q_UNUSED(wizard)

    QTC_ASSERT(canCreate(typeId), return nullptr);

    QVariantMap tmp = data.toMap();

    auto page = new VcsCommandPage;
    page->setVersionControlId(tmp.value(QLatin1String(VCSCOMMAND_VCSID)).toString());
    page->setRunMessage(tmp.value(QLatin1String(VCSCOMMAND_RUN_MESSAGE)).toString());

    QStringList args;
    const QVariant argsVar = tmp.value(QLatin1String(VCSCOMMAND_EXTRA_ARGS));
    if (!argsVar.isNull()) {
        if (argsVar.type() == QVariant::String) {
            args << argsVar.toString();
        } else if (argsVar.type() == QVariant::List) {
            args = Utils::transform(argsVar.toList(), &QVariant::toString);
        } else {
            return nullptr;
        }
    }

    page->setCheckoutData(tmp.value(QLatin1String(VCSCOMMAND_REPO)).toString(),
                          tmp.value(QLatin1String(VCSCOMMAND_DIR)).toString(),
                          tmp.value(QLatin1String(VCSCOMMAND_CHECKOUTNAME)).toString(),
                          args);

    const QVariantList values = tmp.value(QLatin1String(VCSCOMMAND_JOBS)).toList();
    for (const QVariant &value : values) {
        const QVariantMap job = value.toMap();
        const bool skipEmpty = job.value(QLatin1String(JOB_SKIP_EMPTY), true).toBool();
        const FilePath workDir = FilePath::fromVariant(job.value(QLatin1String(JOB_WORK_DIRECTORY)));

        const QString cmdString = job.value(QLatin1String(JOB_COMMAND)).toString();
        QTC_ASSERT(!cmdString.isEmpty(), continue);

        QStringList command;
        command << cmdString;

        const QVariant &jobArgVar = job.value(QLatin1String(JOB_ARGUMENTS));
        QStringList jobArgs;
        if (!jobArgVar.isNull()) {
            if (jobArgVar.type() == QVariant::List)
                jobArgs = Utils::transform(jobArgVar.toList(), &QVariant::toString);
            else
                jobArgs << jobArgVar.toString();
        }

        bool ok;
        int timeoutFactor = job.value(QLatin1String(JOB_TIME_OUT), 1).toInt(&ok);
        if (!ok)
            timeoutFactor = 1;

        command << jobArgs;

        const QVariant condition = job.value(QLatin1String(JOB_ENABLED), true);

        page->appendJob(skipEmpty, workDir, command, condition, timeoutFactor);
    }

    return page;
}

bool VcsCommandPageFactory::validateData(Id typeId, const QVariant &data, QString *errorMessage)
{
    QTC_ASSERT(canCreate(typeId), return false);

    QString em;
    if (data.type() != QVariant::Map)
        em = tr("\"data\" is no JSON object in \"VcsCommand\" page.");

    if (em.isEmpty()) {
        const QVariantMap tmp = data.toMap();

        QString str = tmp.value(QLatin1String(VCSCOMMAND_VCSID)).toString();
        if (str.isEmpty()) {
            em = tr("\"%1\" not set in \"data\" section of \"VcsCommand\" page.")
                    .arg(QLatin1String(VCSCOMMAND_VCSID));
        }
        str = tmp.value(QLatin1String(VCSCOMMAND_REPO)).toString();
        if (str.isEmpty()) {
            em = tr("\"%1\" not set in \"data\" section of \"VcsCommand\" page.")
                    .arg(QLatin1String(VCSCOMMAND_REPO));
        }
        str = tmp.value(QLatin1String(VCSCOMMAND_DIR)).toString();
        if (str.isEmpty()) {
            em = tr("\"%1\" not set in \"data\" section of \"VcsCommand\" page.")
                    .arg(QLatin1String(VCSCOMMAND_DIR));;
        }
        str = tmp.value(QLatin1String(VCSCOMMAND_CHECKOUTNAME)).toString();
        if (str.isEmpty()) {
            em = tr("\"%1\" not set in \"data\" section of \"VcsCommand\" page.")
                    .arg(QLatin1String(VCSCOMMAND_CHECKOUTNAME));
        }

        str = tmp.value(QLatin1String(VCSCOMMAND_RUN_MESSAGE)).toString();
        if (str.isEmpty()) {
            em = tr("\"%1\" not set in \"data\" section of \"VcsCommand\" page.")
                    .arg(QLatin1String(VCSCOMMAND_RUN_MESSAGE));
        }

        const QVariant extra = tmp.value(QLatin1String(VCSCOMMAND_EXTRA_ARGS));
        if (!extra.isNull() && extra.type() != QVariant::String && extra.type() != QVariant::List) {
            em = tr("\"%1\" in \"data\" section of \"VcsCommand\" page has unexpected type (unset, String or List).")
                    .arg(QLatin1String(VCSCOMMAND_EXTRA_ARGS));
        }

        const QVariant jobs = tmp.value(QLatin1String(VCSCOMMAND_JOBS));
        if (!jobs.isNull() && extra.type() != QVariant::List) {
            em = tr("\"%1\" in \"data\" section of \"VcsCommand\" page has unexpected type (unset or List).")
                    .arg(QLatin1String(VCSCOMMAND_JOBS));
        }

        const QVariantList jobList = jobs.toList();
        for (const QVariant &j : jobList) {
            if (j.isNull()) {
                em = tr("Job in \"VcsCommand\" page is empty.");
                break;
            }
            if (j.type() != QVariant::Map) {
                em = tr("Job in \"VcsCommand\" page is not an object.");
                break;
            }
            const QVariantMap &details = j.toMap();
            if (details.value(QLatin1String(JOB_COMMAND)).isNull()) {
                em = tr("Job in \"VcsCommand\" page has no \"%1\" set.").arg(QLatin1String(JOB_COMMAND));
                break;
            }
        }
    }

    if (errorMessage)
        *errorMessage = em;

    return em.isEmpty();
}

// ----------------------------------------------------------------------
// VcsCommandPage:
// ----------------------------------------------------------------------

/*!
    \class VcsBase::VcsCommandPage

    \brief The VcsCommandPage implements a page showing the
    progress of a \c VcsCommand.

    Turns complete when the command succeeds.
*/

VcsCommandPage::VcsCommandPage()
    : m_startedStatus(tr("Command started..."))
{
    resize(264, 200);
    auto verticalLayout = new QVBoxLayout(this);
    m_logPlainTextEdit = new QPlainTextEdit;
    m_formatter = new OutputFormatter;
    m_logPlainTextEdit->setReadOnly(true);
    m_formatter->setPlainTextEdit(m_logPlainTextEdit);

    verticalLayout->addWidget(m_logPlainTextEdit);

    m_statusLabel = new QLabel;
    verticalLayout->addWidget(m_statusLabel);
    setTitle(tr("Checkout"));
}

VcsCommandPage::~VcsCommandPage()
{
    QTC_ASSERT(m_state != Running, QApplication::restoreOverrideCursor());
    delete m_formatter;
}


void VcsCommandPage::initializePage()
{
    // Delay real initialization till after QWizard is done with its setup.
    // Otherwise QWizard will reset our disabled back button again.
    QTimer::singleShot(0, this, &VcsCommandPage::delayedInitialize);
}

bool VcsCommandPage::isComplete() const
{
    return m_state == Succeeded;
}

bool VcsCommandPage::handleReject()
{
    if (m_state != Running)
        return false;

    if (m_command)
        m_command->cancel();
    return true;
}

void VcsCommandPage::delayedInitialize()
{
    auto wiz = qobject_cast<JsonWizard *>(wizard());
    QTC_ASSERT(wiz, return);

    const QString vcsId = wiz->expander()->expand(m_vcsId);
    VcsBasePluginPrivate *vc = static_cast<VcsBasePluginPrivate *>(
                VcsManager::versionControl(Id::fromString(vcsId)));
    if (!vc) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "\"%1\" (%2) not found.")
                      .arg(QLatin1String(VCSCOMMAND_VCSID), vcsId);
        return;
    }
    if (!vc->isConfigured()) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "Version control \"%1\" is not configured.")
                      .arg(vcsId);
        return;
    }
    if (!vc->supportsOperation(IVersionControl::InitialCheckoutOperation)) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "Version control \"%1\" does not support initial checkouts.")
                      .arg(vcsId);
        return;
    }

    const QString repo = wiz->expander()->expand(m_repository);
    if (repo.isEmpty()) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "\"%1\" is empty when trying to run checkout.")
                      .arg(QLatin1String(VCSCOMMAND_REPO));
        return;
    }

    const QString base = wiz->expander()->expand(m_directory);
    if (!QDir(base).exists()) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "\"%1\" (%2) does not exist.")
                      .arg(QLatin1String(VCSCOMMAND_DIR), base);
        return;
    }

    const QString name = wiz->expander()->expand(m_name);
    if (name.isEmpty()) {
        qWarning() << QCoreApplication::translate("VcsBase::VcsCommandPage",
                                                  "\"%1\" is empty when trying to run checkout.")
                      .arg(QLatin1String(VCSCOMMAND_CHECKOUTNAME));
        return;
    }

    const QString runMessage = wiz->expander()->expand(m_runMessage);
    if (!runMessage.isEmpty())
        m_startedStatus = runMessage;

    QStringList extraArgs;
    for (const QString &in : qAsConst(m_arguments)) {
        const QString tmp = wiz->expander()->expand(in);
        if (tmp.isEmpty())
           continue;
        if (tmp == QStringLiteral("\"\""))
            extraArgs << QString();
        extraArgs << tmp;
    }

    VcsCommand *command = vc->createInitialCheckoutCommand(repo, FilePath::fromString(base),
                                                           name, extraArgs);

    for (const JobData &job : qAsConst(m_additionalJobs)) {
        QTC_ASSERT(!job.job.isEmpty(), continue);

        if (!JsonWizard::boolFromVariant(job.condition, wiz->expander()))
            continue;

        QString commandString = wiz->expander()->expand(job.job.at(0));
        if (commandString.isEmpty())
            continue;

        QStringList args;
        for (int i = 1; i < job.job.count(); ++i) {
            const QString tmp = wiz->expander()->expand(job.job.at(i));
            if (tmp.isEmpty() && job.skipEmptyArguments)
                continue;
            args << tmp;
        }

        const FilePath dir = wiz->expander()->expand(job.workDirectory);
        const int defaultTimeoutS = 10;
        const int timeoutS = defaultTimeoutS * job.timeOutFactor;
        command->addJob({FilePath::fromUserInput(commandString), args}, timeoutS, dir);
    }

    start(command);
}

void VcsCommandPage::start(VcsCommand *command)
{
    if (!command) {
        m_logPlainTextEdit->setPlainText(tr("No job running, please abort."));
        return;
    }

    QTC_ASSERT(m_state != Running, return);
    m_command = command;
    command->setProgressiveOutput(true);
    connect(command, &VcsCommand::stdOutText, this, [this](const QString &text) {
        m_formatter->appendMessage(text, StdOutFormat);
    });
    connect(command, &VcsCommand::stdErrText, this, [this](const QString &text) {
        m_formatter->appendMessage(text, StdErrFormat);
    });
    connect(command, &VcsCommand::finished, this, &VcsCommandPage::finished);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_logPlainTextEdit->clear();
    m_overwriteOutput = false;
    m_statusLabel->setText(m_startedStatus);
    m_statusLabel->setPalette(QPalette());
    m_state = Running;
    command->execute();

    wizard()->button(QWizard::BackButton)->setEnabled(false);
}

void VcsCommandPage::finished(bool success)
{
    QTC_ASSERT(m_state == Running, return);

    QString message;
    QPalette palette;

    if (success) {
        m_state = Succeeded;
        message = tr("Succeeded.");
        palette.setColor(QPalette::WindowText, creatorTheme()->color(Theme::TextColorNormal).name());
    } else {
        m_state = Failed;
        message = tr("Failed.");
        palette.setColor(QPalette::WindowText, creatorTheme()->color(Theme::TextColorError).name());
    }

    m_statusLabel->setText(message);
    m_statusLabel->setPalette(palette);

    QApplication::restoreOverrideCursor();
    wizard()->button(QWizard::BackButton)->setEnabled(true);

    if (success)
        emit completeChanged();
}

void VcsCommandPage::setCheckoutData(const QString &repo, const QString &baseDir, const QString &name,
                                  const QStringList &args)
{
    m_repository = repo;
    m_directory = baseDir;
    m_name = name;
    m_arguments = args;
}

void VcsCommandPage::appendJob(bool skipEmpty, const FilePath &workDir, const QStringList &command,
                               const QVariant &condition, int timeoutFactor)
{
    m_additionalJobs.append(JobData{skipEmpty, workDir, command, condition, timeoutFactor});
}

void VcsCommandPage::setVersionControlId(const QString &id)
{
    m_vcsId = id;
}

void VcsCommandPage::setRunMessage(const QString &msg)
{
    m_runMessage = msg;
}

} // namespace Internal
} // namespace VcsBase

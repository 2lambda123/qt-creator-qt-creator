// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>
#include <QSet>
#include <QVersionNumber>
#include <QtGlobal>

#if defined(MCUSUPPORT_LIBRARY)
#define MCUSUPPORTSHARED_EXPORT Q_DECL_EXPORT
#else
#define MCUSUPPORTSHARED_EXPORT Q_DECL_IMPORT
#endif

namespace McuSupport::Internal {

class McuTarget;
class McuAbstractPackage;
class McuToolchainPackage;

using McuPackagePtr = std::shared_ptr<McuAbstractPackage>;
using McuToolchainPackagePtr = std::shared_ptr<McuToolchainPackage>;
using McuTargetPtr = std::shared_ptr<McuTarget>;

static const QVersionNumber minimalVersion{2, 0, 0};
static const QVersionNumber newVersion{2, 3};
using Targets = QList<McuTargetPtr>;
using Packages = QSet<McuPackagePtr>;

struct McuSupportMessage
{
    QString packageName;
    QString platform;
    QString message;
    enum Status { Warning, Error } status = Status::Error;
};
using MessagesList = QList<McuSupportMessage>;

} // namespace McuSupport::Internal

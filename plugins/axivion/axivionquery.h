// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#pragma once

#include <utils/qtcprocess.h>

#include <QObject>

namespace Axivion::Internal {

class AxivionQuery
{
public:
    enum QueryType {NoQuery, DashboardInfo, ProjectInfo, IssuesForFileList, RuleInfo};
    explicit AxivionQuery(QueryType type, const QStringList &parameters = {});

    QString toString() const;

private:
    QueryType m_type = NoQuery;
    QStringList m_parameters;
};

class AxivionQueryRunner : public QObject
{
    Q_OBJECT
public:
    explicit AxivionQueryRunner(const AxivionQuery &query, QObject *parent = nullptr);
    void start();

signals:
    void finished();
    void resultRetrieved(const QByteArray &json);

private:
    Utils::QtcProcess m_process;
};

} // Axivion::Internal

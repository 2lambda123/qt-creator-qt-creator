/****************************************************************************
**
** Copyright (C) 2013 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com <http://qt.digia.com/>
**
** This file is part of the Qt Enterprise Qt Quick Profiler Add-on.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com <http://qt.digia.com/>
**
****************************************************************************/

#ifndef PIXMAPCACHEMODEL_H
#define PIXMAPCACHEMODEL_H

#include "qmlprofiler/qmlprofilertimelinemodelproxy.h"
#include "qmlprofiler/abstracttimelinemodel.h"
#include "qmlprofiler/qmlprofilerdatamodel.h"

#include <QStringList>
#include <QColor>

namespace QmlProfilerExtension {
namespace Internal {

class PixmapCacheModel : public QmlProfiler::AbstractTimelineModel
{
    Q_OBJECT
public:

    struct PixmapCacheEvent {
        int pixmapEventType;
        int urlIndex;
        int sizeIndex;
        int rowNumberCollapsed;
        qint64 cacheSize;
    };

    enum PixmapEventType {
        PixmapSizeKnown,
        PixmapReferenceCountChanged,
        PixmapCacheCountChanged,
        PixmapLoadingStarted,
        PixmapLoadingFinished,
        PixmapLoadingError,

        MaximumPixmapEventType
    };

    PixmapCacheModel(QObject *parent = 0);

    int rowCount() const;

    int getEventRow(int index) const;
    Q_INVOKABLE int getEventId(int index) const;
    Q_INVOKABLE QColor getColor(int index) const;
    Q_INVOKABLE float getHeight(int index) const;

    const QVariantList getLabels() const;

    Q_INVOKABLE const QVariantList getEventDetails(int index) const;

    void loadData();
    void clear();

private:
    static const int PixmapCacheCountHue = 240;

    class PixmapCacheModelPrivate;
    Q_DECLARE_PRIVATE(PixmapCacheModel)
};

} // namespace Internal
} // namespace QmlProfilerExtension

#endif // PIXMAPCACHEMODEL_H

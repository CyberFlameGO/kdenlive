/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef THUMBNAILPROVIDER_H
#define THUMBNAILPROVIDER_H

#include <KImageCache>
#include <QCache>
#include <QQuickImageProvider>
#include <memory>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider();
    ~ThumbnailProvider() override;
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QImage makeThumbnail(const std::shared_ptr<Mlt::Producer> &producer, int frameNumber, const QSize &requestedSize);
    QString cacheKey(Mlt::Properties &properties, const QString &service, const QString &resource, const QString &hash, int frameNumber);
};

#endif // THUMBNAILPROVIDER_H

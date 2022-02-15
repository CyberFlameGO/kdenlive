/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QDir>
#include <QUrl>
#include <QImage>
#include <QMutex>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

/** @class ThumbnailCache
    @brief This class class is an interface to the caches that store thumbnails.
    In Kdenlive, we use two such caches, a persistent that is stored on disk to allow thumbnails to be reused when reopening.
    The other one is a volatile LRU cache that lives in memory.
    Note that for the volatile cache uses a custom implementation.
    QCache is not suitable since it operates on pointers and since the object is removed from the cache when accessed.
    KImageCache is not suitable since it lacks a way to remove objects from the cache.
 * Note that this class is a Singleton
 */
class ThumbnailCache
{

public:
    // Returns the instance of the Singleton
    static std::unique_ptr<ThumbnailCache> &get();

    /** @brief Check whether a given thumbnail is in the cache
       @param binId is the id of the queried clip
       @param pos is the position where we query
       @param volatileOnly if true, we only check the volatile cache (no disk access)
     */
    bool hasThumbnail(const QString &binId, int pos, bool volatileOnly = false) const;

    /** @brief Get a given thumbnail from the cache
       @param binId is the id of the queried clip
       @param pos is the position where we query
       @param volatileOnly if true, we only check the volatile cache (no disk access)
    */
    QImage getThumbnail(const QString &binId, int pos, bool volatileOnly = false) const;
    QImage getAudioThumbnail(const QString &binId, bool volatileOnly = false) const;
    const QList <QUrl> getAudioThumbPath(const QString &binId) const;

    /** @brief Get a given thumbnail from the cache
       @param binId is the id of the queried clip
       @param pos is the position where we query
       @param persistent if true, we store the image in the persistent cache, which generates a disk access
    */
    void storeThumbnail(const QString &binId, int pos, const QImage &img, bool persistent = false);

    /** @brief Removes all the thumbnails for a given clip */
    void invalidateThumbsForClip(const QString &binId);

    /** @brief Save all cached thumbs to disk */
    void saveCachedThumbs(const QStringList &keys);

    /** @brief Reset cache (discarding all thumbs stored in memory) */
    void clearCache();

protected:
    // Constructor is protected because class is a Singleton
    ThumbnailCache();

    // Return the key associated to a thumbnail
    static QString getKey(const QString &binId, int pos, bool *ok);
    static QStringList getAudioKey(const QString &binId, bool *ok);

    // Return the dir where the persistent cache lives
    static QDir getDir(bool audio, bool *ok);

    static std::unique_ptr<ThumbnailCache> instance;
    static std::once_flag m_onceFlag; // flag to create the repository only once;

    class Cache_t;
    std::unique_ptr<Cache_t> m_volatileCache;
    mutable QMutex m_mutex;

    // the following maps keeps track of the positions that we store for each clip in volatile caches.
    // Note that we don't track deletions due to items dropped from the cache. So the maps can contain more items that are currently stored.
    std::unordered_map<QString, std::vector<int>> m_storedVolatile;
    mutable std::unordered_map<QString, std::vector<int>> m_storedOnDisk;
};

/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "scenesplittask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <klocalizedstring.h>

SceneSplitTask::SceneSplitTask(const ObjectId &owner, double threshold, QObject* object)
    : AbstractTask(owner, AbstractTask::ANALYSECLIPJOB, object)
    , m_jobDuration(0)
    , m_threshold(threshold)
    , m_jobProcess(nullptr)
{
}

void SceneSplitTask::start(QObject* object, bool force)
{
    bool ok;
    int threshold = QInputDialog::getInt(qApp->activeWindow(), i18n("Scene Split"), i18n("Enter detection threshold"), KdenliveSettings::scenesplitthreshold(), 0, 100, 1, &ok);
    if (!ok) {
        return;
    }
    KdenliveSettings::setScenesplitthreshold(threshold);
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    for (auto & id : binIds) {
        SceneSplitTask* task = nullptr;
        ObjectId owner;
        if (id.contains(QLatin1Char('/'))) {
            QStringList binData = id.split(QLatin1Char('/'));
            if (binData.size() < 3) {
                // Invalid subclip data
                qDebug()<<"=== INVALID SUBCLIP DATA: "<<id;
                continue;
            }
            owner = ObjectId(ObjectType::BinClip, binData.first().toInt());
            auto binClip = pCore->projectItemModel()->getClipByBinID(binData.first());
            task = new SceneSplitTask(owner, threshold / 100., binClip.get());

        } else {
            owner = ObjectId(ObjectType::BinClip, id.toInt());
            auto binClip = pCore->projectItemModel()->getClipByBinID(id);
            task = new SceneSplitTask(owner, threshold / 100., binClip.get());
        }
        // See if there is already a task for this MLT service and resource.
        if (task && pCore->taskManager.hasPendingJob(owner, AbstractTask::ANALYSECLIPJOB)) {
            delete task;
            task = 0;
        }
        if (task) {
            // Otherwise, start a new audio levels generation thread.
            task->m_isForce = force;
            pCore->taskManager.startTask(owner.second, task);
        }
    }
}

void SceneSplitTask::run()
{
    if (m_isCanceled) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    m_running = true;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
    const QString source = binClip->url();
    ClipType::ProducerType type = binClip->clipType();
    bool result;
    if (type != ClipType::AV && type != ClipType::Video) {
        // This job can only process video files
        m_errorMessage.prepend(i18n("Cannot analyse this clip type"));
        pCore->taskManager.taskDone(m_owner.second, this);
        qDebug()<<"=== ABORT 1";
        return;
    }
    if (KdenliveSettings::ffmpegpath().isEmpty()) {
        // FFmpeg not detected, cannot process the Job
        m_errorMessage.prepend(i18n("Failed to create proxy. FFmpeg not found, please set path in Kdenlive's settings Environment"));
        pCore->taskManager.taskDone(m_owner.second, this);
        qDebug()<<"=== ABORT 2";
        return;
    }
    m_jobDuration = int(binClip->duration().seconds());
    //QStringList parameters = {QStringLiteral("-loglevel"),QStringLiteral("info"),QStringLiteral("-i"),source,QStringLiteral("-filter:v"),QString("scdet"),QStringLiteral("-f"),QStringLiteral("null"),QStringLiteral("-")};
    QStringList parameters = {QStringLiteral("-y"),QStringLiteral("-loglevel"),QStringLiteral("info"),QStringLiteral("-i"),source,QStringLiteral("-filter:v"),QString("select='gt(scene,0.1)',showinfo"),QStringLiteral("-vsync"),QStringLiteral("vfr"),QStringLiteral("-r"),QStringLiteral("50")};
#ifdef Q_OS_WIN
        parameters << QStringLiteral("-");
#else
        //parameters << QStringLiteral("-");
#endif
        parameters << QStringLiteral("/tmp/res.mp4");

    m_jobProcess.reset(new QProcess);
    //m_jobProcess->setStandardErrorFile("/tmp/test_settings.txt");
    m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
    qDebug()<<"=== READY TO START JOB:"<<parameters;
    QObject::connect(this, &SceneSplitTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardOutput, this, &SceneSplitTask::processLogInfo);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &SceneSplitTask::processLogErr);
    m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters);
    //m_jobProcess->closeReadChannel(QProcess::StandardError);
    m_jobProcess->waitForStarted();
    //QString data;
    /*while(m_jobProcess->waitForReadyRead()) {
        //data.append(m_jobProcess->readAll());
        qDebug()<<"???? READ: \n"<<m_jobProcess->readAll();
    }*/
    m_jobProcess->waitForFinished(-1);
    result = m_jobProcess->exitStatus() == QProcess::NormalExit;

    // remove temporary playlist if it exists
    m_progress = 100;
    pCore->taskManager.taskDone(m_owner.second, this);
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    if (result && !m_isCanceled) {
        qDebug()<<"========================\n\nGOR RESULTS: "<<m_results<<"\n\n=========";
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
    //if (m_markersType >= 0) {
        // Build json data for markers
        QJsonArray list;
        int ix = 1;
        int lastCut = 0;
        for (auto &marker : m_results) {
            int pos = GenTime(marker).frames(pCore->getCurrentFps());
            /*
            if (m_minInterval > 0 && ix > 1 && pos - lastCut < m_minInterval) {
                continue;
            }*/
            lastCut = pos;
            QJsonObject currentMarker;
            currentMarker.insert(QLatin1String("pos"), QJsonValue(pos));
            currentMarker.insert(QLatin1String("comment"), QJsonValue(i18n("Scene %1", ix)));
            currentMarker.insert(QLatin1String("type"), QJsonValue(0)); //m_markersType));
            list.push_back(currentMarker);
            ix++;
        }
        QJsonDocument json(list);
        QMetaObject::invokeMethod(m_object, "importJsonMarkers", Q_ARG(const QString&,QString(json.toJson())));
        //binClip->getMarkerModel()->importFromJson(QString(json.toJson()), true, undo, redo);
    //}
        /*if (QFileInfo(destUrl).size() == 0) {
            QFile::remove(destUrl);
            // File was not created
            m_errorMessage.append(i18n("Failed to create file."));
        } else {
            QString id = QString::number(m_owner.second);
            auto binClip = pCore->projectItemModel()->getClipByBinID(id);
            if (m_replaceProducer && binClip) {
                QMap <QString, QString> sourceProps;
                QMap <QString, QString> newProps;
                sourceProps.insert(QStringLiteral("resource"), binClip->url());
                sourceProps.insert(QStringLiteral("kdenlive:clipname"), binClip->clipName());
                newProps.insert(QStringLiteral("resource"), destUrl);
                newProps.insert(QStringLiteral("kdenlive:clipname"), QFileInfo(destUrl).fileName());
                pCore->bin()->slotEditClipCommand(id, sourceProps, newProps);
            } else {
                QString folder = QStringLiteral("-1");
                if (binClip) {
                    auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
                    if (containingFolder) {
                        folder = containingFolder->clipId();
                    }
                }
                QMetaObject::invokeMethod(pCore->window(), "addProjectClip", Qt::QueuedConnection, Q_ARG(const QString&,destUrl), Q_ARG(const QString&,folder));
                //id = ClipCreator::createClipFromFile(destUrl, folderId, pCore->projectItemModel());
            }
        }*/
    } else {
        // Proxy process crashed
        m_errorMessage.append(QString::fromUtf8(m_jobProcess->readAll()));
    }
}

void SceneSplitTask::processLogErr()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    qDebug()<<"ERROR: ----\n"<<buffer;
}

void SceneSplitTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardOutput());
    m_logDetails.append(buffer);
    int progress = 0;
    // Parse FFmpeg output
    qDebug()<<"-------------\n"<<buffer;
    if (buffer.contains(QLatin1String("[Parsed_showinfo"))) {
        QString timeMarker("pts_time:");
        bool ok;
        QStringList output = buffer.split("[Parsed_showinfo");
        output.removeFirst();
        for (const QString &o : output) {
            if (o.contains(timeMarker)) {
                double res = o.section(timeMarker, 1).section(QLatin1Char(' '), 0, 0).toDouble(&ok);
                if (ok) {
                    m_results << res;
                }
            }
        }
    }
    if (m_jobDuration == 0) {
        qDebug()<<"=== NO DURATION!!!";
        if (buffer.contains(QLatin1String("Duration:"))) {
            QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            if (!data.isEmpty()) {
                qDebug()<<"==== GOT DURATION:"<<data;
                QStringList numbers = data.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    return;
                }
                m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
            }
        }
    } else if (buffer.contains(QLatin1String("time="))) {
        QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        qDebug()<<"=== GOT PROGRESS TIME: "<<time;
        if (!time.isEmpty()) {
            QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                progress = time.toInt();
                if (progress == 0) {
                    return;
                }
            } else {
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
            }
        }
        m_progress = 100 * progress / m_jobDuration;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
        //emit jobProgress(int(100.0 * progress / m_jobDuration));
    }
}

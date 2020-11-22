/***************************************************************************
 *   Copyright (C) 2020 by Sashmita Raghav                                 *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "subtitlemodel.hpp"
#include "bin/bin.h"
#include "core.h"
#include "project/projectmanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/snapmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "undohelper.hpp"

#include <mlt++/MltProperties.h>
#include <mlt++/Mlt.h>

#include <KLocalizedString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SubtitleModel::SubtitleModel(Mlt::Tractor *tractor, std::shared_ptr<TimelineItemModel> timeline, QObject *parent)
    : QAbstractListModel(parent)
    , m_timeline(timeline)
    , m_lock(QReadWriteLock::Recursive)
    , m_subtitleFilter(new Mlt::Filter(pCore->getCurrentProfile()->profile(), "avfilter.subtitles"))
    , m_tractor(tractor)
{
    qDebug()<< "subtitle constructor";
    qDebug()<<"Filter!";
    if (tractor != nullptr) {
        qDebug()<<"Tractor!";
        m_subtitleFilter->set("internal_added", 237);
        m_tractor->attach(*m_subtitleFilter.get());
    }
    setup();
    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    int fontSize = frameSize.height() / 15;
    int fontMargin = frameSize.height() - (2 *fontSize);
    scriptInfoSection = QString("[Script Info]\n; This is a Sub Station Alpha v4 script.\n;\nScriptType: v4.00\nCollisions: Normal\nPlayResX: %1\nPlayResY: %2\nTimer: 100.0000\n").arg(frameSize.width()).arg(frameSize.height());
    styleSection = QString("[V4 Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\nStyle: Default,Consolas,%1,16777215,65535,255,0,-1,0,1,2,2,6,40,40,%2,0,1\n").arg(fontSize).arg(fontMargin);
    eventSection = QStringLiteral("[Events]\n");
    styleName = QStringLiteral("Default");
    connect(this, &SubtitleModel::modelChanged, [this]() {
        jsontoSubtitle(toJson()); 
    });
    
}

void SubtitleModel::setup()
{
    // We connect the signals of the abstractitemmodel to a more generic one.
    connect(this, &SubtitleModel::columnsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsRemoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::columnsInserted, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsMoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsRemoved, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::rowsInserted, this, &SubtitleModel::modelChanged);
    connect(this, &SubtitleModel::modelReset, this, &SubtitleModel::modelChanged);
}

void SubtitleModel::importSubtitle(const QString filePath, int offset, bool externalImport)
{
    QString start,end,comment;
    QString timeLine;
    GenTime startPos, endPos;
    int turn = 0,r = 0;
    /*
     * turn = 0 -> Parse next subtitle line [srt] (or) Parse next section [ssa]
     * turn = 1 -> Add string to timeLine
     * turn > 1 -> Add string to completeLine
     */
    if (filePath.isEmpty())
        return;
    GenTime subtitleOffset(offset, pCore->getCurrentFps());
    if (filePath.endsWith(".srt")) {
        QFile srtFile(filePath);
        if (!srtFile.exists() || !srtFile.open(QIODevice::ReadOnly)) {
            qDebug() << " File not found " << filePath;
            return;
        }
        qDebug()<< "srt File";
        //parsing srt file
        QTextStream stream(&srtFile);
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty()) {
                if (!turn) {
                    // index=atoi(line.toStdString().c_str());
                    turn++;
                    continue;
                }
                if (line.contains("-->")) {
                    timeLine += line;
                    QStringList srtTime = timeLine.split(' ');
                    if (srtTime.count() < 3) {
                        // invalid time
                        continue;
                    }
                    start = srtTime[0];
                    startPos= stringtoTime(start);
                    end = srtTime[2];
                    endPos = stringtoTime(end);
                } else {
                    r++;
                    if (!comment.isEmpty())
                        comment += " ";
                    if (r == 1)
                        comment += line;
                    else
                        comment = comment + "\r" +line;
                }
                turn++;
            } else {
                addSubtitle(TimelineModel::getNextId(), startPos + subtitleOffset, endPos + subtitleOffset, comment);
                //reinitialize
                comment = timeLine = "";
                turn = 0; r = 0;
            }            
        }  
        srtFile.close();
    } else if (filePath.endsWith(".ass")) {
        qDebug()<< "ass File";
        QString startTime,endTime,text;
        QString EventFormat, section;
        turn = 0;
        int maxSplit =0;
        QFile assFile(filePath);
        if (!assFile.exists() || !assFile.open(QIODevice::ReadOnly)) {
            qDebug() << " Failed attempt on opening " << filePath;
            return;
        }
        QTextStream stream(&assFile);
        QString line;
        qDebug() << " correct ass file  " << filePath;
        scriptInfoSection.clear();
        styleSection.clear();
        eventSection.clear();
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty()) {
                if (!turn) {
                    //qDebug() << " turn = 0  " << line;
                    //check if it is script info, event,or v4+ style
                    QString linespace = line;
                    if (linespace.replace(" ","").contains("ScriptInfo")) {
                        //qDebug()<< "Script Info";
                        section = "Script Info";
                        scriptInfoSection += line+"\n";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Styles")) {
                        //qDebug()<< "V4 Styles";
                        section = "V4 Styles";
                        styleSection += line + "\n";
                        turn++;
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else if (line.contains("Events")) {
                        turn++;
                        section = "Events";
                        eventSection += line +"\n";
                        //qDebug()<< "turn" << turn;
                        continue;
                    } else {
                        //unknown section
                        
                    }
                }
                if (section.contains("Script Info")) {
                    scriptInfoSection += line + "\n";
                }
                if (section.contains("V4 Styles")) {
                    QStringList styleFormat;
                    styleSection +=line + "\n";
                    //Style: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding
                    styleFormat = (line.split(": ")[1].replace(" ","")).split(',');
                    styleName = styleFormat[0];

                }
                //qDebug() << "\n turn != 0  " << turn<< line;
                if (section.contains("Events")) {
                    //if it is event
                    QStringList format;
                    if (line.contains("Format:")) {
                    	eventSection += line +"\n";
                        EventFormat += line;
                        format = (EventFormat.split(": ")[1].replace(" ","")).split(',');
                        //qDebug() << format << format.count();
                        maxSplit = format.count();
                        //TIME
                        startTime = format[1];
                        endTime = format[2];
                        // Text
                        text = format[9];
                        //qDebug()<< startTime << endTime << text;
                    } else {
                        QString EventDialogue;
                        QStringList dialogue;
                        start = "";end = "";comment = "";
                        EventDialogue += line;
                        dialogue = EventDialogue.split(": ")[1].split(',');
                        QString remainingStr = "," + EventDialogue.split(": ")[1].section(',', maxSplit);
                        //qDebug()<< dialogue;
                        //TIME
                        start = dialogue[1];
                        startPos= stringtoTime(start);
                        end = dialogue[2];
                        endPos= stringtoTime(end);
                        // Text
                        comment = dialogue[9]+ remainingStr;
                        //qDebug()<<"Start: "<< start << "End: "<<end << comment;
                        addSubtitle(TimelineModel::getNextId(), startPos + subtitleOffset, endPos + subtitleOffset, comment);
                    }
                }
                turn++;
            } else {
                turn = 0;
                text = startTime = endTime = "";
            }
        }
        assFile.close();
    }
    if (externalImport) {
        jsontoSubtitle(toJson());
    }
}

void SubtitleModel::parseSubtitle(const QString subPath)
{   
	qDebug()<<"Parsing started";
    if (!subPath.isEmpty()) {
        m_subtitleFilter->set("av.filename", subPath.toUtf8().constData());
    }
    QString filePath = m_subtitleFilter->get("av.filename");
    m_subFilePath = filePath;
    importSubtitle(filePath, 0, false);
    //jsontoSubtitle(toJson());
}

const QString SubtitleModel::getUrl()
{
    return m_subtitleFilter->get("av.filename");
}

GenTime SubtitleModel::stringtoTime(QString &str)
{
    QStringList total,secs;
    double hours = 0, mins = 0, seconds = 0, ms = 0;
    double total_sec = 0;
    GenTime pos;
    total = str.split(':');
    hours = atoi(total[0].toStdString().c_str());
    mins = atoi(total[1].toStdString().c_str());
    if (total[2].contains('.'))
        secs = total[2].split('.'); //ssa file
    else
        secs = total[2].split(','); //srt file
    seconds = atoi(secs[0].toStdString().c_str());
    ms = atoi(secs[1].toStdString().c_str());
    total_sec = hours *3600 + mins *60 + seconds + ms * 0.001 ;
    pos= GenTime(total_sec);
    return pos;
}

bool SubtitleModel::addSubtitle(int id, GenTime start, GenTime end, const QString str, bool temporary)
{
	if (start.frames(pCore->getCurrentFps()) < 0 || end.frames(pCore->getCurrentFps()) < 0) {
        qDebug()<<"Time error: is negative";
        return false;
    }
    if (start.frames(pCore->getCurrentFps()) > end.frames(pCore->getCurrentFps())) {
        qDebug()<<"Time error: start should be less than end";
        return false;
    }
    // Don't allow 2 subtitles at same start pos
    if (m_subtitleList.count(start) > 0) {
        qDebug()<<"already present in model"<<"string :"<<m_subtitleList[start].first<<" start time "<<start.frames(pCore->getCurrentFps())<<"end time : "<< m_subtitleList[start].second.frames(pCore->getCurrentFps());
        return false;
    }
    int row = m_timeline->m_allSubtitles.size();
    beginInsertRows(QModelIndex(), row, row);
    m_subtitleList[start] = {str, end};
    m_timeline->registerSubtitle(id, start, temporary);
    endInsertRows();
    addSnapPoint(start);
    addSnapPoint(end);
    qDebug()<<"Added to model";
    return true;
}

QHash<int, QByteArray> SubtitleModel::roleNames() const 
{
    QHash<int, QByteArray> roles;
    roles[SubtitleRole] = "subtitle";
    roles[StartPosRole] = "startposition";
    roles[EndPosRole] = "endposition";
    roles[StartFrameRole] = "startframe";
    roles[EndFrameRole] = "endframe";
    roles[IdRole] = "id";
    roles[SelectedRole] = "selected";
    return roles;
}

QVariant SubtitleModel::data(const QModelIndex& index, int role) const
{   
    if (index.row() < 0 || index.row() >= static_cast<int>(m_subtitleList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto subInfo = m_timeline->getSubtitleIdFromIndex(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
        case SubtitleRole:
            return m_subtitleList.at(subInfo.second).first;
        case IdRole:
            return subInfo.first;
        case StartPosRole:
            return subInfo.second.seconds();
        case EndPosRole:
            return m_subtitleList.at(subInfo.second).second.seconds();
        case StartFrameRole:
            return subInfo.second.frames(pCore->getCurrentFps());
        case EndFrameRole:
            return m_subtitleList.at(subInfo.second).second.frames(pCore->getCurrentFps());
        case SelectedRole:
            return m_selected.contains(subInfo.first);
    }
    return QVariant();
}

int SubtitleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_subtitleList.size());
}

QList<SubtitledTime> SubtitleModel::getAllSubtitles() const
{
    QList<SubtitledTime> subtitle;
    for (const auto &subtitles : m_subtitleList) {
        SubtitledTime s(subtitles.first, subtitles.second.first, subtitles.second.second);
        subtitle << s;
    }
    return subtitle;
}

SubtitledTime SubtitleModel::getSubtitle(GenTime startFrame) const
{
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first == startFrame) {
            return SubtitledTime(subtitles.first, subtitles.second.first, subtitles.second.second);
        }
    }
    return SubtitledTime(GenTime(), QString(), GenTime());
}

QString SubtitleModel::getText(int id) const
{
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end()) {
        return QString();
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    return m_subtitleList.at(start).first;
}

bool SubtitleModel::setText(int id, const QString text)
{
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end()) {
        return false;
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    GenTime end = m_subtitleList.at(start).second;
    QString oldText = m_subtitleList.at(start).first;
    m_subtitleList[start].first = text;
    Fun local_redo = [this, start, end, text]() {
        editSubtitle(start, text);
        pCore->refreshProjectRange({start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())});
        return true;
    };
    Fun local_undo = [this, start, end, oldText]() {
        editSubtitle(start, oldText);
        pCore->refreshProjectRange({start.frames(pCore->getCurrentFps()), end.frames(pCore->getCurrentFps())});
        return true;
    };
    local_redo();
    pCore->pushUndo(local_undo, local_redo, i18n("Edit subtitle"));
    return true;
}

std::unordered_set<int> SubtitleModel::getItemsInRange(int startFrame, int endFrame) const
{
    GenTime startTime(startFrame, pCore->getCurrentFps());
    GenTime endTime(endFrame, pCore->getCurrentFps());
    std::unordered_set<int> matching;
    for (const auto &subtitles : m_subtitleList) {
        if (endFrame > -1 && subtitles.first > endTime) {
            // Outside range
            continue;
        }
        if (subtitles.first >= startTime || subtitles.second.second >= startTime) {
            matching.emplace(getIdForStartPos(subtitles.first));
        }
    }
    return matching;
}

void SubtitleModel::cutSubtitle(int position)
{
    Fun redo = []() { return true; };
    Fun undo = []() { return true; };
    if (cutSubtitle(position, undo, redo)) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
    }
}


bool SubtitleModel::cutSubtitle(int position, Fun &undo, Fun &redo)
{
    GenTime pos(position, pCore->getCurrentFps());
    GenTime start = GenTime(-1);
    for (const auto &subtitles : m_subtitleList) {
        if (subtitles.first <= pos && subtitles.second.second > pos) {
            start = subtitles.first;
            break;
        }
    }
    if (start >= GenTime()) {
        GenTime end = m_subtitleList.at(start).second;
        QString text = m_subtitleList.at(start).first;
        
        int subId = getIdForStartPos(start);
        int duration = position - start.frames(pCore->getCurrentFps());
        bool res = requestResize(subId, duration, true, undo, redo, false);
        if (res) {
            int id = TimelineModel::getNextId();
            Fun local_redo = [this, id, pos, end, text]() {
                return addSubtitle(id, pos, end, text);
            };
            Fun local_undo = [this, id]() {
                removeSubtitle(id);
                return true;
            };
            if (local_redo()) {
                UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
                return true;
            }
        }
    }
    undo();
    return false;
}

void SubtitleModel::registerSnap(const std::weak_ptr<SnapInterface> &snapModel)
{
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_regSnaps.push_back(snapModel);
        // we now add the already existing subtitles to the snap
        for (const auto &subtitle : m_subtitleList) {
            ptr->addPoint(subtitle.first.frames(pCore->getCurrentFps()));
        }
    } else {
        qDebug() << "Error: added snapmodel for subtitle is null";
        Q_ASSERT(false);
    }
}

void SubtitleModel::addSnapPoint(GenTime startpos)
{
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_regSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->addPoint(startpos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_regSnaps, validSnapModels);
}

void SubtitleModel::removeSnapPoint(GenTime startpos)
{
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_regSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->removePoint(startpos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_regSnaps, validSnapModels);
}

void SubtitleModel::editEndPos(GenTime startPos, GenTime newEndPos, bool refreshModel)
{
    qDebug()<<"Changing the sub end timings in model";
    if (m_subtitleList.count(startPos) <= 0) {
        //is not present in model only
        return;
    }
    m_subtitleList[startPos].second = newEndPos;
    // Trigger update of the qml view
    int id = getIdForStartPos(startPos);
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), {EndFrameRole});
    if (refreshModel) {
        emit modelChanged();
    }
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
}

bool SubtitleModel::requestResize(int id, int size, bool right)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestResize(id, size, right, undo, redo, true);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Resize subtitle"));
        return true;
    } else {
        undo();
        return false;
    }
}

bool SubtitleModel::requestResize(int id, int size, bool right, Fun &undo, Fun &redo, bool logUndo)
{
    Q_ASSERT(m_timeline->m_allSubtitles.find( id ) != m_timeline->m_allSubtitles.end());
    GenTime startPos = m_timeline->m_allSubtitles.at(id);
    GenTime endPos = m_subtitleList.at(startPos).second;
    Fun operation = []() { return true; };
    Fun reverse = []() { return true; };
    if (right) {
        GenTime newEndPos = startPos + GenTime(size, pCore->getCurrentFps());
        operation = [this, id, startPos, endPos, newEndPos, logUndo]() {
            m_subtitleList[startPos].second = newEndPos;
            removeSnapPoint(endPos);
            addSnapPoint(newEndPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                emit modelChanged();
                if (endPos > newEndPos) {
                    pCore->refreshProjectRange({newEndPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())});
                } else {
                    pCore->refreshProjectRange({endPos.frames(pCore->getCurrentFps()), newEndPos.frames(pCore->getCurrentFps())});
                }
            }
            return true;
        };
        reverse = [this, id, startPos, endPos, newEndPos, logUndo]() {
            m_subtitleList[startPos].second = endPos;
            removeSnapPoint(newEndPos);
            addSnapPoint(endPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {EndFrameRole});
            if (logUndo) {
                emit modelChanged();
                if (endPos > newEndPos) {
                    pCore->refreshProjectRange({newEndPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())});
                } else {
                    pCore->refreshProjectRange({endPos.frames(pCore->getCurrentFps()), newEndPos.frames(pCore->getCurrentFps())});
                }
            }
            return true;
        };
    } else {
        GenTime newStartPos = endPos - GenTime(size, pCore->getCurrentFps());
        if (m_subtitleList.count(newStartPos) > 0) {
            // There already is another subtitle at this position, abort
            return false;
        }
        const QString text = m_subtitleList.at(startPos).first;
        operation = [this, id, startPos, newStartPos, endPos, text, logUndo]() {
            m_timeline->m_allSubtitles[id] = newStartPos;
            m_subtitleList.erase(startPos);
            m_subtitleList[newStartPos] = {text, endPos};
            // Trigger update of the qml view
            removeSnapPoint(startPos);
            addSnapPoint(newStartPos);
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                emit modelChanged();
                if (startPos > newStartPos) {
                    pCore->refreshProjectRange({newStartPos.frames(pCore->getCurrentFps()), startPos.frames(pCore->getCurrentFps())});
                } else {
                    pCore->refreshProjectRange({startPos.frames(pCore->getCurrentFps()), newStartPos.frames(pCore->getCurrentFps())});
                }
            }
            return true;
        };
        reverse = [this, id, startPos, newStartPos, endPos, text, logUndo]() {
            m_timeline->m_allSubtitles[id] = startPos;
            m_subtitleList.erase(newStartPos);
            m_subtitleList[startPos] = {text, endPos};
            removeSnapPoint(newStartPos);
            addSnapPoint(startPos);
            // Trigger update of the qml view
            int row = m_timeline->getSubtitleIndex(id);
            emit dataChanged(index(row), index(row), {StartFrameRole});
            if (logUndo) {
                emit modelChanged();
                if (startPos > newStartPos) {
                    pCore->refreshProjectRange({newStartPos.frames(pCore->getCurrentFps()), startPos.frames(pCore->getCurrentFps())});
                } else {
                    pCore->refreshProjectRange({startPos.frames(pCore->getCurrentFps()), newStartPos.frames(pCore->getCurrentFps())});
                }
            }
            return true;
        };
    }
    operation();
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    return true;
}

void SubtitleModel::editSubtitle(GenTime startPos, QString newSubtitleText)
{
    if(startPos.frames(pCore->getCurrentFps()) < 0) {
        qDebug()<<"Time error: is negative";
        return;
    }
    qDebug()<<"Editing existing subtitle in model";
    m_subtitleList[startPos].first = newSubtitleText ;
    int id = getIdForStartPos(startPos);
    qDebug()<<startPos.frames(pCore->getCurrentFps())<<m_subtitleList[startPos].first<<m_subtitleList[startPos].second.frames(pCore->getCurrentFps());
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), QVector<int>() << SubtitleRole);
    emit modelChanged();
    return;
}

bool SubtitleModel::removeSubtitle(int id, bool temporary)
{
    qDebug()<<"Deleting subtitle in model";
    if (m_timeline->m_allSubtitles.find( id ) == m_timeline->m_allSubtitles.end()) {
        qDebug()<<"No Subtitle at pos in model";
        return false;
    }
    GenTime start = m_timeline->m_allSubtitles.at(id);
    if (m_subtitleList.count(start) <= 0) {
        qDebug()<<"No Subtitle at pos in model";
        return false;
    }
    GenTime end = m_subtitleList.at(start).second;
    int row = m_timeline->getSubtitleIndex(id);
    m_timeline->deregisterSubtitle(id, temporary);
    beginRemoveRows(QModelIndex(), row, row);
    m_subtitleList.erase(start);
    endRemoveRows();
    removeSnapPoint(start);
    removeSnapPoint(end);
    return true;
}

void SubtitleModel::removeAllSubtitles()
{
    auto ids = m_timeline->m_allSubtitles;
    for (const auto &p : ids) {
        removeSubtitle(p.first);
    }
}

void SubtitleModel::requestSubtitleMove(int clipId, GenTime position)
{
    
    GenTime oldPos = getStartPosForId(clipId);
    Fun local_redo = [this, clipId, position]() {
        return moveSubtitle(clipId, position, true, true);
    };
    Fun local_undo = [this, clipId, oldPos]() {
        return moveSubtitle(clipId, oldPos, true, true);
    };
    bool res = local_redo();
    if (res) {
        pCore->pushUndo(local_undo, local_redo, i18n("Move subtitle"));
    }
}

bool SubtitleModel::moveSubtitle(int subId, GenTime newPos, bool updateModel, bool updateView)
{
    qDebug()<<"Moving Subtitle";
    if (m_timeline->m_allSubtitles.count(subId) == 0) {
        return false;
    }
    GenTime oldPos = m_timeline->m_allSubtitles.at(subId);
    if (m_subtitleList.count(oldPos) <= 0 || m_subtitleList.count(newPos) > 0) {
        //is not present in model, or already another one at new position
        qDebug()<<"==== MOVE FAILED";
        return false;
    }
    QString subtitleText = m_subtitleList[oldPos].first ;
    removeSnapPoint(oldPos);
    removeSnapPoint(m_subtitleList[oldPos].second);
    GenTime duration = m_subtitleList[oldPos].second - oldPos;
    GenTime endPos = newPos + duration;
    int id = getIdForStartPos(oldPos);
    m_timeline->m_allSubtitles[id] = newPos;
    m_subtitleList.erase(oldPos);
    m_subtitleList[newPos] = {subtitleText, endPos};
    addSnapPoint(newPos);
    addSnapPoint(endPos);
    if (updateView) {
        updateSub(id, {StartFrameRole, EndFrameRole});
        if (oldPos < newPos) {
            pCore->refreshProjectRange({oldPos.frames(pCore->getCurrentFps()), endPos.frames(pCore->getCurrentFps())});
        } else {
            pCore->refreshProjectRange({newPos.frames(pCore->getCurrentFps()), (oldPos + duration).frames(pCore->getCurrentFps())});
        }
    }
    if (updateModel) {
        // Trigger update of the subtitle file
        emit modelChanged();
    }
    return true;
}

int SubtitleModel::getIdForStartPos(GenTime startTime) const
{
    auto findResult = std::find_if(std::begin(m_timeline->m_allSubtitles), std::end(m_timeline->m_allSubtitles), [&](const std::pair<int, GenTime> &pair) {
        return pair.second == startTime;
    });
    if (findResult != std::end(m_timeline->m_allSubtitles)) {
        return findResult->first;
    }
    return -1;
}

GenTime SubtitleModel::getStartPosForId(int id) const
{
    if (m_timeline->m_allSubtitles.count(id) == 0) {
        return GenTime();
    };
    return m_timeline->m_allSubtitles.at(id);
}

int SubtitleModel::getPreviousSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find(start)));
    if (row > 0) {
        row--;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first;
        return getIdForStartPos(res);
    }
    return -1;
}

int SubtitleModel::getNextSub(int id) const
{
    GenTime start = getStartPosForId(id);
    int row = static_cast<int>(std::distance(m_subtitleList.begin(), m_subtitleList.find(start)));
    if (row < static_cast<int>(m_subtitleList.size()) - 1) {
        row++;
        auto it = m_subtitleList.begin();
        std::advance(it, row);
        const GenTime res = it->first;
        return getIdForStartPos(res);
    }
    return -1;
}

QString SubtitleModel::toJson()
{
    //qDebug()<< "to JSON";
    QJsonArray list;
    for (const auto &subtitle : m_subtitleList) {
        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("startPos"), QJsonValue(subtitle.first.seconds()));
        currentSubtitle.insert(QLatin1String("dialogue"), QJsonValue(subtitle.second.first));
        currentSubtitle.insert(QLatin1String("endPos"), QJsonValue(subtitle.second.second.seconds()));
        list.push_back(currentSubtitle);
        //qDebug()<<subtitle.first.seconds();
    }
    QJsonDocument jsonDoc(list);
    //qDebug()<<QString(jsonDoc.toJson());
    return QString(jsonDoc.toJson());
}

void SubtitleModel::jsontoSubtitle(const QString &data, QString updatedFileName)
{
	QString outFile = updatedFileName.isEmpty() ? m_subtitleFilter->get("av.filename") : updatedFileName;
    if (outFile.isEmpty()) {
        outFile = pCore->currentDoc()->subTitlePath(); // use srt format as default unless file is imported (m_subFilePath)
    }
    bool assFormat = outFile.endsWith(".ass");
    if (!assFormat) {
        qDebug()<< "srt file import"; // if imported file isn't .ass, it is .srt format
    }
    QFile outF(outFile);

    //qDebug()<< "Import from JSON";
    QWriteLocker locker(&m_lock);
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return;
    }
    int line=0;
    auto list = json.array();
    if (outF.open(QIODevice::WriteOnly)) {
        QTextStream out(&outF);
        if (assFormat) {
        	out<<scriptInfoSection<<endl;
        	out<<styleSection<<endl;
        	out<<eventSection;
        }
        for (const auto &entry : list) {
            if (!entry.isObject()) {
                qDebug() << "Warning : Skipping invalid subtitle data";
                continue;
            }
            auto entryObj = entry.toObject();
            if (!entryObj.contains(QLatin1String("startPos"))) {
                qDebug() << "Warning : Skipping invalid subtitle data (does not contain position)";
                continue;
            }
            double startPos = entryObj[QLatin1String("startPos")].toDouble();
            //convert seconds to FORMAT= hh:mm:ss.SS (in .ass) and hh:mm:ss,SSS (in .srt)
            int millisec = startPos * 1000;
            int seconds = millisec / 1000;
            millisec %=1000;
            int minutes = seconds / 60;
            seconds %= 60;
            int hours = minutes /60;
            minutes %= 60;
            int milli_2 = millisec / 10;
            QString startTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));
            QString startTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            QString dialogue = entryObj[QLatin1String("dialogue")].toString();
            double endPos = entryObj[QLatin1String("endPos")].toDouble();
            millisec = endPos * 1000;
            seconds = millisec / 1000;
            millisec %=1000;
            minutes = seconds / 60;
            seconds %= 60;
            hours = minutes /60;
            minutes %= 60;

            milli_2 = millisec / 10; // to limit ms to 2 digits (for .ass)
            QString endTimeString = QString("%1:%2:%3.%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(milli_2,2,10,QChar('0'));

            QString endTimeStringSRT = QString("%1:%2:%3,%4")
              .arg(hours, 1, 10, QChar('0'))
              .arg(minutes, 2, 10, QChar('0'))
              .arg(seconds, 2, 10, QChar('0'))
              .arg(millisec,3,10,QChar('0'));
            line++;
            if (assFormat) {
            	//Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
            	out <<"Dialogue: 0,"<<startTimeString<<","<<endTimeString<<","<<styleName<<",,0000,0000,0000,,"<<dialogue<<endl;
            } else {
                out<<line<<"\n"<<startTimeStringSRT<<" --> "<<endTimeStringSRT<<"\n"<<dialogue<<"\n"<<endl;
            }
            
            //qDebug() << "ADDING SUBTITLE to FILE AT START POS: " << startPos <<" END POS: "<<endPos;//<< ", FPS: " << pCore->getCurrentFps();
        }
    }
    qDebug()<<"Setting subtitle filter: "<<outFile;
    if (line > 0) {
        m_subtitleFilter->set("av.filename", outFile.toUtf8().constData());
        m_tractor->attach(*m_subtitleFilter.get());
    } else {
        m_tractor->detach(*m_subtitleFilter.get());
    }
}

void SubtitleModel::updateSub(int id, QVector <int> roles)
{
    int row = m_timeline->getSubtitleIndex(id);
    emit dataChanged(index(row), index(row), roles);
}

int SubtitleModel::getRowForId(int id) const
{
    return m_timeline->getSubtitleIndex(id);
}

int SubtitleModel::getSubtitlePlaytime(int id) const
{
    GenTime startPos = m_timeline->m_allSubtitles.at(id);
    return (m_subtitleList.at(startPos).second - startPos).frames(pCore->getCurrentFps());
}

void SubtitleModel::setSelected(int id, bool select)
{
    if (select) {
        m_selected << id;
    } else {
        m_selected.removeAll(id);
    }
    updateSub(id, {SelectedRole});
}

bool SubtitleModel::isSelected(int id) const
{
    return m_selected.contains(id);
}
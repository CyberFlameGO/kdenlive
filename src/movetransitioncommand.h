/***************************************************************************
                          movetransitioncommand.h  -  description
                             -------------------
    begin                : Mar 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MOVETRANSITIONCOMMAND_H
#define MOVETRANSITIONCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>

#include <KDebug>

class CustomTrackView;

class MoveTransitionCommand : public QUndoCommand {
public:
    MoveTransitionCommand(CustomTrackView *view, const QPointF startPos, const QPointF endPos, bool doIt);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    QPointF m_startPos;
    QPointF m_endPos;
    bool m_doIt;
	 int m_startTrack;
	 int m_endTrack;
};

#endif


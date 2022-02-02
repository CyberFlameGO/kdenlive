/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "simplekeyframewidget.h"
#include "simpletimelinewidget.h"
#include "widgets/timecodedisplay.h"

#include <QGridLayout>
#include <QToolButton>

#include <klocalizedstring.h>

SimpleKeyframeWidget::SimpleKeyframeWidget(const Timecode &t, int duration, QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *l = new QGridLayout(this);

    m_timeline = new SimpleTimelineWidget(this);
    m_timeline->setDuration(duration);

    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);
    m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_buttonPrevious = new QToolButton(this);
    m_buttonPrevious->setAutoRaise(true);
    m_buttonPrevious->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-backward")));
    m_buttonPrevious->setToolTip(i18n("Go to previous keyframe"));

    m_buttonNext = new QToolButton(this);
    m_buttonNext->setAutoRaise(true);
    m_buttonNext->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-forward")));
    m_buttonNext->setToolTip(i18n("Go to next keyframe"));

    m_time = new TimecodeDisplay(t, this);
    m_time->setRange(0, duration);

    l->addWidget(m_timeline, 0, 0, 1, -1);
    l->addWidget(m_buttonPrevious, 1, 0);
    l->addWidget(m_buttonAddDelete, 1, 1);
    l->addWidget(m_buttonNext, 1, 2);
    l->addWidget(m_time, 1, 3, Qt::AlignRight);

    connect(m_time, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotSetPosition()));
    connect(m_timeline, SIGNAL(positionChanged(int)), this, SLOT(slotSetPosition(int)));
    connect(m_timeline, &SimpleTimelineWidget::atKeyframe, this, &SimpleKeyframeWidget::slotAtKeyframe);
    connect(m_timeline, &SimpleTimelineWidget::keyframeAdded, this, &SimpleKeyframeWidget::keyframeAdded);
    connect(m_timeline, &SimpleTimelineWidget::keyframeRemoved, this, &SimpleKeyframeWidget::keyframeRemoved);
    connect(m_timeline, &SimpleTimelineWidget::keyframeMoved, this, &SimpleKeyframeWidget::keyframeMoved);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_timeline, &SimpleTimelineWidget::slotAddRemove);
    connect(m_buttonPrevious, &QAbstractButton::pressed, m_timeline, &SimpleTimelineWidget::slotGoToPrev);
    connect(m_buttonNext, &QAbstractButton::pressed, m_timeline, &SimpleTimelineWidget::slotGoToNext);

    // no keyframes yet
    setEnabled(false);
}

SimpleKeyframeWidget::~SimpleKeyframeWidget()
{
    delete m_timeline;
    delete m_buttonAddDelete;
    delete m_buttonPrevious;
    delete m_buttonNext;
    delete m_time;
}

void SimpleKeyframeWidget::slotSetPosition(int pos, bool update)
{
    if (pos < 0) {
        pos = m_time->getValue();
        m_timeline->slotSetPosition(pos);
    } else {
        m_time->setValue(pos);
        m_timeline->slotSetPosition(pos);
    }

    if (update) {
        emit positionChanged(pos);
    }
}

int SimpleKeyframeWidget::getPosition() const
{
    return m_time->getValue();
}

void SimpleKeyframeWidget::setKeyframes(const QList<int> &keyframes)
{
    m_timeline->setKeyframes(keyframes);
    setEnabled(true);
}

void SimpleKeyframeWidget::addKeyframe(int pos)
{
    blockSignals(true);
    m_timeline->slotAddKeyframe(pos);
    blockSignals(false);
    setEnabled(true);
}

void SimpleKeyframeWidget::updateTimecodeFormat()
{
    m_time->slotUpdateTimeCodeFormat();
}

void SimpleKeyframeWidget::slotAtKeyframe(bool atKeyframe)
{
    if (atKeyframe) {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
        m_buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    } else {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
        m_buttonAddDelete->setToolTip(i18n("Add keyframe"));
    }
}

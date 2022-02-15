/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


#include "speeddialog.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "widgets/timecodedisplay.h"
#include "ui_clipspeed_ui.h"

#include <KMessageWidget>
#include <QDebug>
#include <QPushButton>
#include <QtMath>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, int duration, double minSpeed, double maxSpeed, bool reversed, bool pitch_compensate)
    : QDialog(parent)
    , ui(new Ui::ClipSpeed_UI)
    , m_durationDisplay(nullptr)
    , m_duration(duration)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->speedSpin->setDecimals(2);
    ui->speedSpin->setMinimum(minSpeed);
    ui->speedSpin->setMaximum(maxSpeed);
    ui->speedSlider->setMinimum(0);
    ui->speedSlider->setMaximum(100);
    ui->speedSlider->setTickInterval(10);
    ui->label_dest->setVisible(false);
    ui->kurlrequester->setVisible(false);
    ui->toolButton->setVisible(false);
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    ui->speedSpin->setValue(speed);
    ui->speedSlider->setValue(int(qLn(speed) * 12));
    ui->pitchCompensate->setChecked(pitch_compensate);
    if (!EffectsRepository::get()->exists(QStringLiteral("rbpitch"))) {
        ui->pitchCompensate->setEnabled(false);
        ui->pitchCompensate->setToolTip(i18n("MLT must be compiled with rubberband library to enable pitch correction"));
    }

    // Info widget
    auto *infoMessage = new KMessageWidget(this);
    ui->infoLayout->addWidget(infoMessage);
    infoMessage->hide();
    ui->speedSpin->setFocus();
    ui->speedSpin->selectAll();
    if (m_duration > 0) {
        ui->durationLayout->addWidget(new QLabel(i18n("Duration"), this));
        m_durationDisplay = new TimecodeDisplay(pCore->timecode(), this);
        m_durationDisplay->setValue(m_duration);
        ui->durationLayout->addWidget(m_durationDisplay);
        connect(m_durationDisplay, &TimecodeDisplay::timeCodeEditingFinished, this, [this, infoMessage, speed, minSpeed](int value) {
            if (value < 1) {
                value = 1;
                m_durationDisplay->setValue(value);
            } else if (value > m_duration * speed / minSpeed) {
                value = int(m_duration * speed / minSpeed);
                m_durationDisplay->setValue(value);
            }
            double updatedSpeed = speed * m_duration / value;
            QSignalBlocker bk(ui->speedSlider);
            ui->speedSlider->setValue(int(qLn(updatedSpeed) * 12));
            QSignalBlocker bk2(ui->speedSpin);
            ui->speedSpin->setValue(updatedSpeed);
            checkSpeed(infoMessage, updatedSpeed);
            
        });
    }

    connect(ui->speedSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&, speed] (double value) {
        QSignalBlocker bk(ui->speedSlider);
        ui->speedSlider->setValue(int(qLn(value) * 12));
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            int dur = qRound(m_duration * std::fabs(speed / value));
            qDebug()<<"==== CALCULATED SPEED DIALOG DURATION: "<<dur;
            m_durationDisplay->setValue(dur);
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
    connect(ui->speedSlider, &QSlider::valueChanged, this, [this, infoMessage, speed] (int value) {
        double res = qExp(value / 12.);
        QSignalBlocker bk(ui->speedSpin);
        checkSpeed(infoMessage, res);
        ui->speedSpin->setValue(res);
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            m_durationDisplay->setValue(qRound(m_duration * std::fabs(speed / res)));
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(ui->speedSpin->value()));
    });
}


void SpeedDialog::checkSpeed(KMessageWidget *infoMessage, double res)
{
    if (res < ui->speedSpin->minimum() || res > ui->speedSpin->maximum()) {
        infoMessage->setText(res < ui->speedSpin->minimum() ? i18n("Minimum speed is %1", ui->speedSpin->minimum()) : i18n("Maximum speed is %1", ui->speedSpin->maximum()));
        infoMessage->setCloseButtonVisible(true);
        infoMessage->setMessageType(KMessageWidget::Warning);
        infoMessage->animatedShow();
    }
}

SpeedDialog::~SpeedDialog()
{
    delete ui;
}

double SpeedDialog::getValue() const
{
    double val = ui->speedSpin->value();
    if (ui->checkBox->checkState() == Qt::Checked) {
        val *= -1;
    }
    return val;
}

bool SpeedDialog::getPitchCompensate() const
{
    return ui->pitchCompensate->isChecked();
}

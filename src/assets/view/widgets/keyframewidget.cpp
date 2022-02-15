/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframewidget.hpp"
#include "assets/keyframes/model/corners/cornershelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/model/keyframemonitorhelper.hpp"
#include "assets/keyframes/model/rect/recthelper.hpp"
#include "assets/keyframes/model/rotoscoping/rotohelper.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/keyframeimport.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "lumaliftgainparam.hpp"
#include "monitor/monitor.h"
#include "utils/timecode.h"
#include "widgets/timecodedisplay.h"
#include "widgets/doublewidget.h"
#include "widgets/geometrywidget.h"

#include <KActionCategory>
#include <KSelectAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QMenu>
#include <QPointer>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <klocalizedstring.h>
#include <utility>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_monitorHelper(nullptr)
    , m_neededScene(MonitorSceneType::MonitorSceneDefault)
    , m_sourceFrameSize(frameSize.isValid() && !frameSize.isNull() ? frameSize : pCore->getCurrentFrameSize())
    , m_baseHeight(0)
    , m_addedHeight(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lay = new QVBoxLayout(this);
    m_lay->setSpacing(0);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();
    m_keyframeview = new KeyframeView(m_keyframes, duration, this);


    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(this, &KeyframeWidget::addRemove, m_keyframeview, &KeyframeView::slotAddRemove);

    m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));


    QToolButton *buttonPrevious = new QToolButton(this);
    buttonPrevious->setAutoRaise(true);
    buttonPrevious->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-previous")));
    buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    connect(buttonPrevious, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(this, &KeyframeWidget::goToPrevious, m_keyframeview, &KeyframeView::slotGoToPrev);

    QToolButton *buttonNext = new QToolButton(this);
    buttonNext->setAutoRaise(true);
    buttonNext->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-next")));
    buttonNext->setToolTip(i18n("Go to next keyframe"));
    connect(buttonNext, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToNext);
    connect(this, &KeyframeWidget::goToNext, m_keyframeview, &KeyframeView::slotGoToNext);
    
    // Move keyframe to cursor
    m_buttonCenter = new QToolButton(this);
    m_buttonCenter->setAutoRaise(true);
    m_buttonCenter->setIcon(QIcon::fromTheme(QStringLiteral("align-horizontal-center")));
    m_buttonCenter->setToolTip(i18n("Move selected keyframe to cursor"));
    
    // Duplicate selected keyframe at cursor pos
    m_buttonCopy = new QToolButton(this);
    m_buttonCopy->setAutoRaise(true);
    m_buttonCopy->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-duplicate")));
    m_buttonCopy->setToolTip(i18n("Duplicate selected keyframe"));
    
    // Apply current value to selected keyframes
    m_buttonApply = new QToolButton(this);
    m_buttonApply->setAutoRaise(true);
    m_buttonApply->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    m_buttonApply->setToolTip(i18n("Apply current position value to selected keyframes"));
    m_buttonApply->setFocusPolicy(Qt::StrongFocus);
    
    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("keyframes")), i18n("Keyframe interpolation"), this);
    QAction *linear = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData(int(mlt_keyframe_linear));
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *discrete = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData(int(mlt_keyframe_discrete));
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *curve = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData(int(mlt_keyframe_smooth));
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &KeyframeWidget::slotEditKeyframeType);
    m_selectType->setToolBarMode(KSelectAction::ComboBoxMode);
    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(size, size));

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    connect(pCore.get(), &Core::disconnectEffectStack, this, &KeyframeWidget::disconnectEffectStack);

    m_time = new TimecodeDisplay(pCore->timecode(), this);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(m_model->data(index, AssetParameterModel::ParentInRole).toInt());

    m_toolbar->addWidget(buttonPrevious);
    m_toolbar->addWidget(m_buttonAddDelete);
    m_toolbar->addWidget(buttonNext);
    m_toolbar->addWidget(m_buttonCenter);
    m_toolbar->addWidget(m_buttonCopy);
    m_toolbar->addWidget(m_buttonApply);
    m_toolbar->addAction(m_selectType);

    QAction *seekKeyframe = new QAction(i18n("Seek to Keyframe on Select"), this);
    seekKeyframe->setCheckable(true);
    seekKeyframe->setChecked(KdenliveSettings::keyframeseek());
    connect(seekKeyframe, &QAction::triggered, [&](bool selected) {
        KdenliveSettings::setKeyframeseek(selected);
    });
    // copy/paste keyframes from clipboard
    QAction *copy = new QAction(i18n("Copy Keyframes to Clipboard"), this);
    connect(copy, &QAction::triggered, this, &KeyframeWidget::slotCopyKeyframes);
    QAction *copyValue = new QAction(i18n("Copy Value at Cursor Position to Clipboard"), this);
    connect(copyValue, &QAction::triggered, this, &KeyframeWidget::slotCopyValueAtCursorPos);
    QAction *paste = new QAction(i18n("Import Keyframes from Clipboard…"), this);
    connect(paste, &QAction::triggered, this, &KeyframeWidget::slotImportKeyframes);
    if (m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::ColorWheel) {
        // TODO color wheel doesn't support keyframe import/export yet
        copy->setVisible(false);
        copyValue->setVisible(false);
        paste->setVisible(false);
    }
    // Remove keyframes
    QAction *removeNext = new QAction(i18n("Remove all Keyframes After Cursor"), this);
    connect(removeNext, &QAction::triggered, this, &KeyframeWidget::slotRemoveNextKeyframes);

    // Default kf interpolation
    KSelectAction *kfType = new KSelectAction(i18n("Default Keyframe Type"), this);
    QAction *discrete2 = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete2->setData(int(mlt_keyframe_discrete));
    discrete2->setCheckable(true);
    kfType->addAction(discrete2);
    QAction *linear2 = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear2->setData(int(mlt_keyframe_linear));
    linear2->setCheckable(true);
    kfType->addAction(linear2);
    QAction *curve2 = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve2->setData(int(mlt_keyframe_smooth));
    curve2->setCheckable(true);
    kfType->addAction(curve2);
    switch (KdenliveSettings::defaultkeyframeinterp()) {
    case mlt_keyframe_discrete:
        kfType->setCurrentAction(discrete2);
        break;
    case mlt_keyframe_smooth:
        kfType->setCurrentAction(curve2);
        break;
    default:
        kfType->setCurrentAction(linear2);
        break;
    }
    connect(kfType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered),
            this, [&](QAction *ac) { KdenliveSettings::setDefaultkeyframeinterp(ac->data().toInt()); });
    auto *container = new QMenu(this);
    container->addAction(seekKeyframe);
    container->addAction(copy);
    container->addAction(copyValue);
    container->addAction(paste);
    container->addSeparator();
    container->addAction(kfType);
    container->addAction(removeNext);

    // rotoscoping only supports linear keyframes
    if (m_model->getAssetId() == QLatin1String("rotoscoping")) {
        m_selectType->setVisible(false);
        m_selectType->setCurrentAction(linear);
        kfType->setVisible(false);
        kfType->setCurrentAction(linear2);
    }

    // Menu toolbutton
    auto *menuButton = new QToolButton(this);
    menuButton->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
    menuButton->setToolTip(i18n("Options"));
    menuButton->setMenu(container);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    m_toolbar->addWidget(menuButton);
    m_toolbar->addWidget(m_time);

    m_lay->addWidget(m_keyframeview);
    m_lay->addWidget(m_toolbar);

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, this, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, this, [&](int pos) {
        if (pos < 0) {
            m_time->setValue(0);
            m_keyframeview->slotSetPosition(0, true);
        } else {
            int in = m_model->data(m_index, AssetParameterModel::InRole).toInt();
            int p = qMax(0, pos - in);
            m_time->setValue(p);
            m_keyframeview->slotSetPosition(p, true);
        }
        m_buttonAddDelete->setEnabled(pos > 0);
        slotRefreshParams();
        emit seekToPos(pos);
    });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_keyframeview, &KeyframeView::activateEffect, this, &KeyframeWidget::activateEffect);

    connect(m_buttonCenter, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotCenterKeyframe);
    connect(m_buttonCopy, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotDuplicateKeyframe);
    connect(m_buttonApply, &QAbstractButton::pressed, this, [this]() {
        QMultiMap<QPersistentModelIndex, QString> paramList;
        QList<QPersistentModelIndex> rectParams;
        for (const auto &w : m_parameters) {
            auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
            if (type == ParamType::AnimatedRect) {
                if (m_model->data(w.first, AssetParameterModel::OpacityRole).toBool()) {
                    paramList.insert(w.first, i18n("Opacity"));
                }
                paramList.insert(w.first, i18n("Height"));
                paramList.insert(w.first, i18n("Width"));
                paramList.insert(w.first, i18n("Y position"));
                paramList.insert(w.first, i18n("X position"));
                rectParams << w.first;
            } else {
                paramList.insert(w.first, m_model->data(w.first, Qt::DisplayRole).toString());
            }
        }
        if (paramList.count() == 0) {
            qDebug()<<"=== No parameter to copy, aborting";
            return;
        }
        if (paramList.count() == 1) {
            m_keyframeview->copyCurrentValue(m_keyframes->getIndexAtRow(0), QString());
            return;
        }
        // More than one param
        QDialog d(this);
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto *l = new QVBoxLayout;
        d.setLayout(l);
        l->addWidget(new QLabel(i18n("Select parameters to copy"), &d));
        QMapIterator<QPersistentModelIndex, QString> i(paramList);
        while (i.hasNext()) {
            i.next();
            auto *cb = new QCheckBox(i.value(), this);
            cb->setProperty("index", i.key());
            l->addWidget(cb);
        }
        l->addWidget(buttonBox);
        d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        if (d.exec() != QDialog::Accepted) {
            return;
        }
        paramList.clear();
        QList<QCheckBox *> cbs = d.findChildren<QCheckBox *>();
        QMap<QPersistentModelIndex, QStringList> params;
        for (auto c : qAsConst(cbs)) {
            //qDebug()<<"=== FOUND CBS: "<<KLocalizedString::removeAcceleratorMarker(c->text());
            if (c->isChecked()) {
                QPersistentModelIndex ix = c->property("index").toModelIndex();
                if (rectParams.contains(ix)) {
                    // Check param name
                    QString cbName = KLocalizedString::removeAcceleratorMarker(c->text());
                    if (cbName == i18n("Opacity")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinO");
                        } else {
                            params.insert(ix, {QStringLiteral("spinO")});
                        }
                    } else if (cbName == i18n("Height")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinH");
                        } else {
                            params.insert(ix, {QStringLiteral("spinH")});
                        }
                    } else if (cbName == i18n("Width")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinW");
                        } else {
                            params.insert(ix, {QStringLiteral("spinW")});
                        }
                    } else if (cbName == i18n("X position")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinX");
                        } else {
                            params.insert(ix, {QStringLiteral("spinX")});
                        }
                    } else if (cbName == i18n("Y position")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinY");
                        } else {
                            params.insert(ix, {QStringLiteral("spinY")});
                        }
                    }
                    if (!params.contains(ix)) {
                        params.insert(ix, {});
                    }
                }
            }
        }
        QMapIterator<QPersistentModelIndex, QStringList> p(params);
        while (p.hasNext()) {
            p.next();
            m_keyframeview->copyCurrentValue(p.key(), p.value().join(QLatin1Char(' ')));
        }
        return;
    });
    //m_baseHeight = m_keyframeview->height() + m_selectType->defaultWidget()->sizeHint().height();
    QMargins mrg = m_lay->contentsMargins();
    m_baseHeight = m_keyframeview->height() + m_toolbar->sizeHint().height();
    m_addedHeight = mrg.top() + mrg.bottom();
    setFixedHeight(m_baseHeight + m_addedHeight);
    addParameter(index);
}

KeyframeWidget::~KeyframeWidget()
{
    delete m_keyframeview;
    delete m_buttonAddDelete;
    delete m_time;
}

void KeyframeWidget::disconnectEffectStack()
{
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    disconnect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek);
}

void KeyframeWidget::monitorSeek(int pos)
{
    int in = 0;
    int out = 0;
    bool canHaveZone = m_model->getOwnerId().first == ObjectType::Master || m_model->getOwnerId().first == ObjectType::TimelineTrack;
    if (canHaveZone) {
        bool ok = false;
        in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
        out = m_model->data(m_index, AssetParameterModel::OutRole).toInt(&ok);
        Q_ASSERT(ok);
    }
    if (in == 0 && out == 0) {
        in = pCore->getItemPosition(m_model->getOwnerId());
        out = in + pCore->getItemDuration(m_model->getOwnerId());
    }
    bool isInRange = pos >= in && pos < out;
    connectMonitor(isInRange && m_model->isActive());
    m_buttonAddDelete->setEnabled(isInRange && pos > in);
    int framePos = qBound(in, pos, out) - in;
    if (isInRange && framePos != m_time->getValue()) {
        slotSetPosition(framePos, false);
    }
}

void KeyframeWidget::slotEditKeyframeType(QAction *action)
{
    int type = action->data().toInt();
    m_keyframeview->slotEditType(type, m_index);
    emit activateEffect();
}

void KeyframeWidget::slotRefreshParams()
{
    int pos = getPosition();
    KeyframeType keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
    int i = 0;
    while (auto ac = m_selectType->action(i)) {
        if (ac->data().toInt() == int(keyType)) {
            m_selectType->setCurrentItem(i);
            break;
        }
        i++;
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::KeyframeParam) {
            (static_cast<DoubleWidget *>(w.second))->setValue(m_keyframes->getInterpolatedValue(pos, w.first).toDouble());
        } else if (type == ParamType::AnimatedRect) {
            const QString val = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            const QStringList vals = val.split(QLatin1Char(' '));
            QRect rect;
            double opacity = -1;
            if (vals.count() >= 4) {
                rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
                if (vals.count() > 4) {
                    opacity = vals.at(4).toDouble();
                }
            }
            (static_cast<GeometryWidget *>(w.second))->setValue(rect, opacity);
        } else if (type == ParamType::ColorWheel) {
            (static_cast<LumaLiftGainParam *>(w.second)->slotRefresh(pos));
        }
    }
    if (m_monitorHelper && m_model->isActive()) {
        m_monitorHelper->refreshParams(pos);
        return;
    }
}
void KeyframeWidget::slotSetPosition(int pos, bool update)
{
    if (pos < 0) {
        pos = m_time->getValue();
    } else {
        m_time->setValue(pos);
    }
    m_keyframeview->slotSetPosition(pos, true);
    m_buttonAddDelete->setEnabled(pos > 0);
    slotRefreshParams();

    if (update) {
        //int in = m_model->data(m_index, AssetParameterModel::InRole).toInt();
        emit seekToPos(pos);
    }
}

int KeyframeWidget::getPosition() const
{
    return m_time->getValue() + pCore->getItemIn(m_model->getOwnerId());
}

void KeyframeWidget::updateTimecodeFormat()
{
    m_time->slotUpdateTimeCodeFormat();
}

void KeyframeWidget::slotAtKeyframe(bool atKeyframe, bool singleKeyframe)
{
    if (atKeyframe) {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-remove")));
        m_buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    } else {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
        m_buttonAddDelete->setToolTip(i18n("Add keyframe"));
    }
    m_buttonCenter->setEnabled(!atKeyframe);
    m_buttonCopy->setEnabled(!atKeyframe);
    emit updateEffectKeyframe(atKeyframe || singleKeyframe);
    m_selectType->setEnabled(atKeyframe || singleKeyframe);
    for (const auto &w : m_parameters) {
        w.second->setEnabled(atKeyframe || singleKeyframe);
    }
}

void KeyframeWidget::slotRefresh()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    //->getKeyframeModel()->getKeyModel(m_index)->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration, in);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(in);
    slotRefreshParams();
}

void KeyframeWidget::resetKeyframes()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
    // reset keyframes
    m_keyframes->refresh();
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration, in);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(in);
    slotRefreshParams();
}

void KeyframeWidget::addParameter(const QPersistentModelIndex &index)
{
    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();

    auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
    // Construct object
    QWidget *paramWidget = nullptr;
    if (type == ParamType::AnimatedRect) {
        m_neededScene = MonitorSceneType::MonitorSceneGeometry;
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        m_monitorHelper = new KeyframeMonitorHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        QRect rect;
        double opacity = 0;
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() > 3) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
            if (vals.count() > 4) {
                opacity = vals.at(4).toDouble();
            }
        }
        // qtblend uses an opacity value in the (0-1) range, while older geometry effects use (0-100)
        GeometryWidget *geomWidget = new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, opacity, m_sourceFrameSize, false,
                                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this);
        connect(geomWidget, &GeometryWidget::valueChanged,
                this, [this, index](const QString &v) {
                    emit activateEffect();
                    m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
        connect(geomWidget, &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
                    if (m_model->isActive()) {
                        pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
                    }
        });
        paramWidget = geomWidget;
    } else if (type == ParamType::ColorWheel) {
        auto colorWheelWidget = new LumaLiftGainParam(m_model, index, this);
        connect(colorWheelWidget, &LumaLiftGainParam::valuesChanged,
                this, [this, index](const QList <QModelIndex> &indexes, const QStringList& list, bool) {
            emit activateEffect();
            auto *parentCommand = new QUndoCommand();
            parentCommand->setText(i18n("Edit %1 keyframe", EffectsRepository::get()->getName(m_model->getAssetId())));
            for (int i = 0; i < indexes.count(); i++) {
                if (m_keyframes->getInterpolatedValue(getPosition(), indexes.at(i)) != list.at(i)) {
                    m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(list.at(i)), indexes.at(i), parentCommand);
                }
            }
            if (parentCommand->childCount() > 0) {
                pCore->pushUndo(parentCommand);
            }
        });
        connect(colorWheelWidget, &LumaLiftGainParam::updateHeight, this, [&](int h){
            setFixedHeight(m_baseHeight + m_addedHeight + h);
            emit updateHeight();
        });
        paramWidget = colorWheelWidget;
    } else if (type == ParamType::Roto_spline) {
        m_monitorHelper = new RotoHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        m_neededScene = MonitorSceneType::MonitorSceneRoto;
    } else {
        if (m_model->getAssetId() == QLatin1String("frei0r.c0rners")) {
            if (m_neededScene == MonitorSceneDefault && !m_monitorHelper) {
                m_neededScene = MonitorSceneType::MonitorSceneCorners;
                m_monitorHelper = new CornersHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
                connect(this, &KeyframeWidget::addIndex, m_monitorHelper, &CornersHelper::addIndex);
            } else {
                if (type == ParamType::KeyframeParam) {
                    int paramName = m_model->data(index, AssetParameterModel::NameRole).toInt();
                    if (paramName < 8) {
                        emit addIndex(index);
                    }
                }
            }
        }
        if(m_model->getAssetId().contains(QLatin1String("frei0r.alphaspot"))) {
            if (m_neededScene == MonitorSceneDefault && !m_monitorHelper) {
                m_neededScene = MonitorSceneType::MonitorSceneGeometry;
                m_monitorHelper = new RectHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
                connect(this, &KeyframeWidget::addIndex, m_monitorHelper, &RectHelper::addIndex);
            } else {
                if (type == ParamType::KeyframeParam) {
                    QString paramName = m_model->data(index, AssetParameterModel::NameRole).toString();
                    if (paramName.contains(QLatin1String("Position X")) || paramName.contains(QLatin1String("Position Y")) ||
                            paramName.contains(QLatin1String("Size X")) || paramName.contains(QLatin1String("Size Y"))) {
                        emit addIndex(index);
                    }
                }
            }
        }
        double value = m_keyframes->getInterpolatedValue(getPosition(), index).toDouble();
        double min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
        double max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
        double defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toDouble();
        int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();
        double factor = m_model->data(index, AssetParameterModel::FactorRole).toDouble();
        factor = qFuzzyIsNull(factor) ? 1 : factor;
        auto doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals, m_model->data(index, AssetParameterModel::OddRole).toBool(), this);
        connect(doubleWidget, &DoubleWidget::valueChanged,
                this, [this, index](double v) {
            emit activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index);
        });
        doubleWidget->setDragObjectName(QString::number(index.row()));
        paramWidget = doubleWidget;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        m_lay->addWidget(paramWidget);
        m_addedHeight += paramWidget->minimumHeight();
        setFixedHeight(m_baseHeight + m_addedHeight);
    }
}

void KeyframeWidget::slotInitMonitor(bool active)
{
    connectMonitor(active);
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
        connect(monitor, &Monitor::updateScene, m_keyframeview, &KeyframeView::slotModelChanged, Qt::UniqueConnection);
    }
}

void KeyframeWidget::connectMonitor(bool active)
{
    if (m_monitorHelper) {
        if (m_model->isActive()) {
            connect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor, Qt::UniqueConnection);
            if (m_monitorHelper->connectMonitor(active)) {
                slotRefreshParams();
            }
        } else {
            m_monitorHelper->connectMonitor(false);
            disconnect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor);
        }
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (active) {
        connect(monitor, &Monitor::seekToNextKeyframe, m_keyframeview, &KeyframeView::slotGoToNext, Qt::UniqueConnection);
        connect(monitor, &Monitor::seekToPreviousKeyframe, m_keyframeview, &KeyframeView::slotGoToPrev, Qt::UniqueConnection);
        connect(monitor, &Monitor::addRemoveKeyframe, m_keyframeview, &KeyframeView::slotAddRemove, Qt::UniqueConnection);
        connect(this, &KeyframeWidget::updateEffectKeyframe, monitor, &Monitor::setEffectKeyframe, Qt::DirectConnection);
        connect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekToNextKeyframe, m_keyframeview, &KeyframeView::slotGoToNext);
        disconnect(monitor, &Monitor::seekToPreviousKeyframe, m_keyframeview, &KeyframeView::slotGoToPrev);
        disconnect(monitor, &Monitor::addRemoveKeyframe, m_keyframeview, &KeyframeView::slotAddRemove);
        disconnect(this, &KeyframeWidget::updateEffectKeyframe, monitor, &Monitor::setEffectKeyframe);
        disconnect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe);
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect) {
            (static_cast<GeometryWidget *>(w.second))->connectMonitor(active);
            break;
        }
    }
}

void KeyframeWidget::slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res)
{
    emit activateEffect();
    if (m_keyframes->isEmpty()) {
        GenTime pos(pCore->getItemIn(m_model->getOwnerId()) + m_time->getValue(), pCore->getCurrentFps());
        if (m_time->getValue() > 0) {
            GenTime pos0(pCore->getItemIn(m_model->getOwnerId()), pCore->getCurrentFps());
            m_keyframes->addKeyframe(pos0, KeyframeType::Linear);
            m_keyframes->updateKeyframe(pos0, res, index);
        }
        m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        m_keyframes->updateKeyframe(pos, res, index);
    } else if (m_keyframes->hasKeyframe(getPosition()) || m_keyframes->singleKeyframe()) {
        GenTime pos(getPosition(), pCore->getCurrentFps());
        if (m_keyframes->singleKeyframe() && KdenliveSettings::autoKeyframe() && m_neededScene == MonitorSceneType::MonitorSceneRoto) {
            m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        }
        m_keyframes->updateKeyframe(pos, res, index);
    } else {
        qDebug()<<"==== NO KFR AT: "<<getPosition();
    }
}

MonitorSceneType KeyframeWidget::requiredScene() const
{
    qDebug() << "// // // RESULTING REQUIRED SCENE: " << m_neededScene;
    return m_neededScene;
}

bool KeyframeWidget::keyframesVisible() const
{
    return m_keyframeview->isVisible();
}

void KeyframeWidget::showKeyframes(bool enable)
{
    if (enable && m_toolbar->isVisible()) {
        return;
    }
    m_toolbar->setVisible(enable);
    m_keyframeview->setVisible(enable);
    setFixedHeight(m_addedHeight + (enable ? m_baseHeight : 0));
}

void KeyframeWidget::slotCopyKeyframes()
{
    QJsonDocument effectDoc = m_model->toJson(false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
}

void KeyframeWidget::slotCopyValueAtCursorPos()
{
    QJsonDocument effectDoc = m_model->valueAsJson(getPosition(), false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
}

void KeyframeWidget::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    QList<QPersistentModelIndex> indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    if (m_neededScene == MonitorSceneRoto) {
        indexes << m_monitorHelper->getIndexes();
    }
    QPointer<KeyframeImport> import = new KeyframeImport(values, m_model, indexes, m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(), m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(), this);
    import->show();
    connect(import, &KeyframeImport::updateQmlView, this, &KeyframeWidget::slotRefreshParams);
}

void KeyframeWidget::slotRemoveNextKeyframes()
{
    int pos = m_time->getValue() + m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    m_keyframes->removeNextKeyframes(GenTime(pos, pCore->getCurrentFps()));
}


void KeyframeWidget::slotSeekToKeyframe(int ix)
{
    int pos = m_keyframes->getPosAtIndex(ix).frames(pCore->getCurrentFps());
    slotSetPosition(pos, true);
}

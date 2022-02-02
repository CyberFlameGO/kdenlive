/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2011 Marco Gittler <marco@gitma.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipstabilize.h"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/assetparameterview.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "widgets/doublewidget.h"
#include "widgets/positionwidget.h"

#include "kdenlivesettings.h"
#include <KIO/RenameDialog>
#include <KMessageBox>
#include <QFontDatabase>
#include <memory>
#include <mlt++/Mlt.h>

ClipStabilize::ClipStabilize(const std::vector<QString> &binIds, QString filterName, QWidget *parent)
    : QDialog(parent)
    , m_filtername(std::move(filterName))
    , m_binIds(binIds)
    , m_vbox(nullptr)
    , m_assetModel(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18ncp("@action", "Add clip to project", "Add clips to project", m_binIds.size()));
    auto_add->setChecked(KdenliveSettings::add_new_clip());
    auto_folder->setChecked(KdenliveSettings::add_new_clip_to_folder());

    // QString stylesheet = EffectStackView2::getStyleSheet();
    // setStyleSheet(stylesheet);

    Q_ASSERT(binIds.size() > 0);
    auto firstBinClip = pCore->projectItemModel()->getClipByBinID(m_binIds.front().section(QLatin1Char('/'), 0, 0));
    auto firstUrl = firstBinClip->url();
    if (m_binIds.size() == 1) {
        QString newFile = firstUrl;
        newFile.append(QStringLiteral(".mlt"));
        dest_url->setMode(KFile::File);
        dest_url->setUrl(QUrl(newFile));
    } else {
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory | KFile::ExistingOnly);
        dest_url->setUrl(QUrl(firstUrl).adjusted(QUrl::RemoveFilename));
    }
    m_vbox = new QVBoxLayout(optionsbox);
    if (m_filtername == QLatin1String("vidstab")) {
        m_view = std::make_unique<AssetParameterView>(this);
        qDebug()<<"// Fetching effect: "<<m_filtername;
        std::unique_ptr<Mlt::Filter> asset = EffectsRepository::get()->getEffect(m_filtername);
        auto prop = std::make_unique<Mlt::Properties>(asset->get_properties());
        QDomElement xml = EffectsRepository::get()->getXml(m_filtername);
        m_assetModel.reset(new AssetParameterModel(std::move(prop), xml, m_filtername, {ObjectType::NoItem, -1}));
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
        const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(m_assetModel->getAssetId()));
        const QVector<QPair<QString, QVariant>> params = m_assetModel->loadPreset(presetFile, i18n("Last setting"));
        if (!params.isEmpty()) {
            m_assetModel->setParameters(params);
        }
        m_view->setModel(m_assetModel, QSize(1920, 1080));
        m_vbox->addWidget(m_view.get());
        // Presets
        preset_button->setIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
        preset_button->setMenu(m_view->presetMenu());
        preset_button->setToolTip(i18n("Presets"));
    }

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &ClipStabilize::slotValidate);
    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    /*if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }*/
    KdenliveSettings::setAdd_new_clip(auto_add->isChecked());
    KdenliveSettings::setAdd_new_clip_to_folder(auto_folder->isChecked());
}

std::unordered_map<QString, QVariant> ClipStabilize::filterParams() const
{
    QVector<QPair<QString, QVariant>> result = m_assetModel->getAllParameters();
    std::unordered_map<QString, QVariant> params;

    for (const auto &it : qAsConst(result)) {
        params[it.first] = it.second;
    }
    return params;
}

QString ClipStabilize::filterName() const
{
    return m_filtername;
}

QString ClipStabilize::destination() const
{
    QString path = dest_url->url().toLocalFile();
    if (m_binIds.size() > 1 && !path.endsWith(QDir::separator())) {
        path.append(QDir::separator());
    }
    return path;
}

QString ClipStabilize::desc() const
{
    return i18nc("Description", "Stabilize clip");
}

bool ClipStabilize::autoAddClip() const
{
    return auto_add->isChecked();
}

bool ClipStabilize::addClipInFolder() const
{
    return auto_folder->isChecked();
}

void ClipStabilize::slotValidate()
{
    if (m_binIds.size() == 1) {
        if (QFile::exists(dest_url->url().toLocalFile())) {
            KIO::RenameDialog renameDialog(this, i18n("File already exists"), dest_url->url(), dest_url->url(), KIO::RenameDialog_Option::RenameDialog_Overwrite );
            if (renameDialog.exec() != QDialog::Rejected) {
                QUrl final = renameDialog.newDestUrl();
                if (final.isValid()) {
                    dest_url->setUrl(final);
                }
            } else {
                return;
            }
        }
    } else {
        QDir folder(dest_url->url().toLocalFile());
        QStringList existingFiles;
        for (const QString &binId : m_binIds) {
            auto binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            auto url = binClip->url();
            if (folder.exists(url + QStringLiteral(".mlt"))) {
                existingFiles.append(folder.absoluteFilePath(url + QStringLiteral(".mlt")));
            }
        }
        if (!existingFiles.isEmpty()) {
            if (KMessageBox::warningContinueCancelList(this, i18n("The stabilize job will overwrite the following files:"), existingFiles) ==
                KMessageBox::Cancel) {
                return;
            }
        }
    }
    m_view->slotSavePreset(i18n("Last setting"));
    accept();
}


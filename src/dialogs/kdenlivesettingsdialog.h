/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KDENLIVESETTINGSDIALOG_H
#define KDENLIVESETTINGSDIALOG_H

#include <KConfigDialog>
#include <KProcess>
#include <QMap>
#include <QListWidget>

#include "ui_configcapture_ui.h"
#include "ui_configenv_ui.h"
#include "ui_configjogshuttle_ui.h"
#include "ui_configmisc_ui.h"
#include "ui_configproject_ui.h"
#include "ui_configproxy_ui.h"
#include "ui_configsdl_ui.h"
#include "ui_configtimeline_ui.h"
#include "ui_configtranscode_ui.h"
#include "ui_configcolors_ui.h"
#include "ui_configspeech_ui.h"

#include "pythoninterfaces/speechtotext.h"
#include "encodingprofilesdialog.h"

class ProfileWidget;
class KJob;

class SpeechList : public QListWidget
{
    Q_OBJECT

public:
    SpeechList(QWidget *parent = nullptr);

protected:
    QStringList mimeTypes() const override;
    void dropEvent(QDropEvent *event) override;

signals:
    void getDictionary(const QUrl url);
};

class KdenliveSettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent = nullptr);
    ~KdenliveSettingsDialog() override;
    void showPage(int page, int option);
    void checkProfile();

protected slots:
    void updateSettings() override;
    void updateWidgets() override;
    bool hasChanged() override;
    void accept() override;

private slots:
    void slotCheckShuttle(int state = 0);
    void slotUpdateShuttleDevice(int ix = 0);
    void slotEditImageApplication();
    void slotEditAudioApplication();
    void slotReadAudioDevices();
    void slotUpdateGrabRegionStatus();
    void slotSetFullscreenMonitor();
    void slotCheckAlsaDriver();
    void slotCheckAudioBackend();
    void slotAddTranscode();
    void slotDeleteTranscode();
    /** @brief Update current transcoding profile. */
    void slotUpdateTranscodingProfile();
    /** @brief Enable / disable the update profile button. */
    void slotEnableTranscodeUpdate();
    /** @brief Update display of current transcoding profile parameters. */
    void slotSetTranscodeProfile();
    void slotShuttleModified();
    void slotDialogModified();
    void slotEnableCaptureFolder();
    void slotEnableLibraryFolder();
    void slotEnableVideoFolder();
    void slotUpdatev4lDevice();
    void slotUpdatev4lCaptureProfile();
    void slotEditVideo4LinuxProfile();
    void slotReloadBlackMagic();
    void slotReloadShuttleDevices();
    void loadExternalProxyProfiles();
    void slotUpdateAudioCaptureChannels(int index);
    void slotUpdateAudioCaptureSampleRate(int index);
    void slotParseVoskDictionaries();
    void getDictionary(const QUrl &sourceUrl = QUrl());
    void removeDictionary();
    void downloadModelFinished(KJob* job);
    void processArchive(const QString &path);
    void doShowSpeechMessage(const QString &message, int messageType);
    
private:
    KPageWidgetItem *m_page1;
    KPageWidgetItem *m_page2;
    KPageWidgetItem *m_page3;
    KPageWidgetItem *m_page4;
    KPageWidgetItem *m_page5;
    KPageWidgetItem *m_page6;
    KPageWidgetItem *m_page7;
    KPageWidgetItem *m_page8;
    KPageWidgetItem *m_page10;
    KPageWidgetItem *m_page11;
    Ui::ConfigEnv_UI m_configEnv;
    Ui::ConfigMisc_UI m_configMisc;
    Ui::ConfigColors_UI m_configColors;
    Ui::ConfigTimeline_UI m_configTimeline;
    Ui::ConfigCapture_UI m_configCapture;
    Ui::ConfigJogShuttle_UI m_configShuttle;
    Ui::ConfigSdl_UI m_configSdl;
    Ui::ConfigTranscode_UI m_configTranscode;
    Ui::ConfigProject_UI m_configProject;
    Ui::ConfigProxy_UI m_configProxy;
    Ui::ConfigSpeech_UI m_configSpeech;
    SpeechList *m_speechListWidget;
    ProfileWidget *m_pw;
    KProcess m_readProcess;
    QAction *m_voskAction;
    bool m_modified;
    bool m_shuttleModified;
    bool m_voskUpdated;
    SpeechToText *m_stt;
    QMap<QString, QString> m_mappable_actions;
    QVector<QComboBox *> m_shuttle_buttons;
    EncodingProfilesChooser *m_tlPreviewProfiles;
    EncodingProfilesChooser *m_proxyProfiles;
    EncodingProfilesChooser *m_decklinkProfiles;
    EncodingProfilesChooser *m_v4lProfiles;
    EncodingProfilesChooser *m_grabProfiles;
    void initDevices();
    void loadTranscodeProfiles();
    void saveTranscodeProfiles();
    void loadCurrentV4lProfileInfo();
    void saveCurrentV4lProfile();
    void setupJogshuttleBtns(const QString &device);
    /** @brief Fill a combobox with the found blackmagic devices */
    static bool getBlackMagicDeviceList(QComboBox *devicelist, bool force = false);
    static bool getBlackMagicOutputDeviceList(QComboBox *devicelist, bool force = false);
    /** @brief Init QtMultimedia audio record settings */
    bool initAudioRecDevice();
    void initMiscPage();
    void initProjectPage();
    void initProxyPage();
    void initTimelinePage();
    void initEnviromentPage();
    void initColorsPage();
    /** @brief Init Speech to text settings */
    void initSpeechPage();
    void initCapturePage();
    void initJogShuttlePage();
    void initSdlPage(bool gpuAllowed);
    void initTranscodePage();

signals:
    void customChanged();
    void doResetConsumer(bool fullReset);
    void updateCaptureFolder();
    void updateLibraryFolder();
    /** @brief Screengrab method changed between fullsceen and region, update rec monitor */
    void updateFullScreenGrab();
    /** @brief A settings changed that requires a Kdenlive restart, trigger it */
    void restartKdenlive(bool resetConfig = false);
    void checkTabPosition();
    /** @brief Switch between merged / separate channels for audio thumbs */
    void audioThumbFormatChanged();
    /** @brief An important timeline property changed, prepare for a reset */
    void resetView();
    /** @brief Monitor background color changed, update monitors */
    void updateMonitorBg();
    /** @brief Trigger parsing of the speech models folder */
    void parseDictionaries();
};

#endif

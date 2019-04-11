// Copyright (C) 2019 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "android/skin/qt/extended-pages/record-macro-page.h"

#include "android/base/StringFormat.h"
#include "android/base/files/PathUtils.h"
#include "android/base/system/System.h"
#include "android/emulation/control/automation_agent.h"
#include "android/skin/qt/extended-pages/common.h"
#include "android/skin/qt/qt-settings.h"
#include "android/skin/qt/stylesheet.h"
#include "android/skin/qt/video-player/QtVideoPlayerNotifier.h"

#include <QMessageBox>
#include <iostream>
#include <sstream>

using namespace android::base;

const QAndroidAutomationAgent* RecordMacroPage::sAutomationAgent = nullptr;

RecordMacroPage::RecordMacroPage(QWidget* parent)
    : QWidget(parent), mUi(new Ui::RecordMacroPage()) {
    mUi->setupUi(this);
    loadUi();
}

// static
void RecordMacroPage::setAutomationAgent(const QAndroidAutomationAgent* agent) {
    sAutomationAgent = agent;
}

void RecordMacroPage::loadUi() {
    // Clear all items. Might need to optimize this and keep track of existing.
    mUi->macroList->clear();

    // Descriptions as QStrings have to be initialized here to use tr().
    mDescriptions = {
            {"Reset_position", tr("Resets sensors to default.")},
            {"Track_horizontal_plane", tr("Circles around the rug.")},
            {"Track_vertical_plane", tr("Looks at the wall next to the tv.")},
            {"Walk_to_image_room", tr("Moves to the dining room.")}};

    const std::string macrosPath = getMacrosDirectory();
    std::vector<std::string> macroFileNames =
            System::get()->scanDirEntries(macrosPath);

    // For every macro, create a macroSavedItem with its name.
    for (auto& macroName : macroFileNames) {
        // Set the real macro name as the object's data.
        QListWidgetItem* listItem = new QListWidgetItem(mUi->macroList);
        QVariant macroNameData(QString::fromStdString(macroName));
        listItem->setData(Qt::UserRole, macroNameData);

        RecordMacroSavedItem* macroSavedItem = new RecordMacroSavedItem();
        macroSavedItem->setDisplayInfo(mDescriptions[macroName]);
        std::replace(macroName.begin(), macroName.end(), '_', ' ');
        macroName.append(" (Preset macro)");
        macroSavedItem->setName(macroName.c_str());

        listItem->setSizeHint(QSize(macroSavedItem->sizeHint().width(), 50));

        mUi->macroList->addItem(listItem);
        mUi->macroList->setItemWidget(listItem, macroSavedItem);
    }

    setMacroUiState(MacroUiState::Waiting);
}

void RecordMacroPage::on_playStopButton_clicked() {
    // Stop and reset automation.
    sAutomationAgent->stopPlayback();

    QListWidgetItem* listItem = mUi->macroList->selectedItems().first();
    if (mState == MacroUiState::Playing) {
        stopButtonClicked(listItem);
    } else {
        playButtonClicked(listItem);
    }
}

void RecordMacroPage::on_macroList_itemPressed(QListWidgetItem* listItem) {
    const std::string macroName = getMacroNameFromItem(listItem);

    if (mMacroPlaying && mCurrentMacroName == macroName) {
        setMacroUiState(MacroUiState::Playing);
        showPreviewFrame(macroName);
    } else {
        setMacroUiState(MacroUiState::Selected);
        showPreview(macroName);
    }
}

// For dragging and clicking outside the items in the item list.
void RecordMacroPage::on_macroList_itemSelectionChanged() {
    if (mVideoPlayer && mVideoPlayer->isRunning()) {
        mVideoPlayer->stop();
    }

    setMacroUiState(MacroUiState::Waiting);
}

std::string RecordMacroPage::getMacrosDirectory() {
    return PathUtils::join(System::get()->getLauncherDirectory(), "resources",
                           "macros");
}

std::string RecordMacroPage::getMacroPreviewsDirectory() {
    return PathUtils::join(System::get()->getLauncherDirectory(), "resources",
                           "macroPreviews");
}

void RecordMacroPage::setMacroUiState(MacroUiState state) {
    mState = state;

    switch (mState) {
        case MacroUiState::Waiting: {
            mUi->previewLabel->setText(tr("Select a macro to preview"));
            mUi->previewLabel->show();
            mUi->previewOverlay->show();
            mUi->replayIcon->hide();
            mUi->playStopButton->setIcon(getIconForCurrentTheme("play_arrow"));
            mUi->playStopButton->setProperty("themeIconName", "play_arrow");
            mUi->playStopButton->setText(tr("PLAY "));
            mUi->playStopButton->setEnabled(false);
            break;
        }
        case MacroUiState::Selected: {
            mUi->previewLabel->hide();
            mUi->previewOverlay->hide();
            mUi->replayIcon->hide();
            mUi->playStopButton->setIcon(getIconForCurrentTheme("play_arrow"));
            mUi->playStopButton->setProperty("themeIconName", "play_arrow");
            mUi->playStopButton->setText(tr("PLAY "));
            mUi->playStopButton->setEnabled(true);
            break;
        }
        case MacroUiState::PreviewFinished: {
            mUi->previewLabel->setText(tr("Click anywhere to replay preview"));
            mUi->previewLabel->show();
            mUi->previewOverlay->show();
            mUi->replayIcon->setPixmap(getIconForCurrentTheme("refresh").pixmap(QSize(36, 36)));
            mUi->replayIcon->show();
            mUi->playStopButton->setIcon(getIconForCurrentTheme("play_arrow"));
            mUi->playStopButton->setProperty("themeIconName", "play_arrow");
            mUi->playStopButton->setText(tr("PLAY "));
            mUi->playStopButton->setEnabled(true);
            break;
        }
        case MacroUiState::Playing: {
            mUi->previewLabel->setText(tr("Macro playing on the Emulator"));
            mUi->previewLabel->show();
            mUi->previewOverlay->show();
            mUi->replayIcon->hide();
            mUi->playStopButton->setIcon(getIconForCurrentTheme("stop_red"));
            mUi->playStopButton->setProperty("themeIconName", "stop_red");
            mUi->playStopButton->setText(tr("STOP "));
            mUi->playStopButton->setEnabled(true);
            break;
        }
    }
}

void RecordMacroPage::playButtonClicked(QListWidgetItem* listItem) {
    RecordMacroSavedItem* macroSavedItem = getItemWidget(listItem);
    macroSavedItem->setDisplayInfo(tr("Now playing..."));
    mVideoPlayer->stop();

    const std::string macroName = getMacroNameFromItem(listItem);
    const std::string macroAbsolutePath =
            PathUtils::join(getMacrosDirectory(), macroName);

    auto result = sAutomationAgent->startPlayback(macroAbsolutePath);
    if (result.err()) {
        std::ostringstream errString;
        errString << result.unwrapErr();

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("An error ocurred."));
        msgBox.setInformativeText(errString.str().c_str());
        msgBox.setDefaultButton(QMessageBox::Save);

        int ret = msgBox.exec();
        return;
    }

    disableMacroItemsExcept(listItem);

    mCurrentMacroName = macroName;
    mMacroPlaying = true;
    setMacroUiState(MacroUiState::Playing);
    showPreviewFrame(macroName);
}

void RecordMacroPage::stopButtonClicked(QListWidgetItem* listItem) {
    const std::string macroName = getMacroNameFromItem(listItem);
    RecordMacroSavedItem* macroSavedItem = getItemWidget(listItem);
    macroSavedItem->setDisplayInfo(mDescriptions[macroName]);

    enableMacroItems();

    mMacroPlaying = false;
    setMacroUiState(MacroUiState::PreviewFinished);
    showPreviewFrame(macroName);
}

void RecordMacroPage::showPreview(const std::string& previewName) {
    const std::string previewPath =
            PathUtils::join(getMacroPreviewsDirectory(), previewName + ".mp4");

    auto videoPlayerNotifier =
            std::unique_ptr<android::videoplayer::QtVideoPlayerNotifier>(
                    new android::videoplayer::QtVideoPlayerNotifier());
    connect(videoPlayerNotifier.get(), SIGNAL(updateWidget()), this,
            SLOT(updatePreviewVideoView()));
    connect(videoPlayerNotifier.get(), SIGNAL(videoStopped()), this,
            SLOT(previewVideoPlayingFinished()));
    mVideoPlayer = android::videoplayer::VideoPlayer::create(
            previewPath, mUi->videoWidget, std::move(videoPlayerNotifier));

    mVideoPlayer->scheduleRefresh(20);
    mVideoPlayer->start();
}

RecordMacroSavedItem* RecordMacroPage::getItemWidget(
        QListWidgetItem* listItem) {
    return qobject_cast<RecordMacroSavedItem*>(
            mUi->macroList->itemWidget(listItem));
}

void RecordMacroPage::updatePreviewVideoView() {
    mUi->videoWidget->repaint();
}

void RecordMacroPage::previewVideoPlayingFinished() {
    setMacroUiState(MacroUiState::PreviewFinished);

    QListWidgetItem* listItem = mUi->macroList->selectedItems().first();
    const std::string macroName = getMacroNameFromItem(listItem);
    showPreviewFrame(macroName);
}

void RecordMacroPage::mousePressEvent(QMouseEvent* event) {
    if (mState == MacroUiState::PreviewFinished) {
        QListWidgetItem* listItem = mUi->macroList->selectedItems().first();
        const std::string macroName = getMacroNameFromItem(listItem);
        showPreview(macroName);
        setMacroUiState(MacroUiState::Selected);
    }
}

void RecordMacroPage::disableMacroItemsExcept(QListWidgetItem* listItem) {
    // Make selection show that macro is playing.
    SettingsTheme theme = getSelectedTheme();
    mUi->macroList->setStyleSheet(
            "QListWidget::item:focus, QListView::item:selected { "
            "background-color: " +
            Ui::stylesheetValues(theme)[Ui::MACRO_BKG_COLOR_VAR] + "}");

    for (int i = 0; i < mUi->macroList->count(); ++i) {
        QListWidgetItem* item = mUi->macroList->item(i);
        if (item != listItem) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            RecordMacroSavedItem* macroItem = getItemWidget(item);
            macroItem->setEnabled(false);
        }
    }
}

void RecordMacroPage::enableMacroItems() {
    // Return selection to normal.
    mUi->macroList->setStyleSheet("");

    for (int i = 0; i < mUi->macroList->count(); ++i) {
        QListWidgetItem* item = mUi->macroList->item(i);
        item->setFlags(item->flags() | Qt::ItemIsEnabled);
        RecordMacroSavedItem* macroItem = getItemWidget(item);
        macroItem->setEnabled(true);
    }
}

void RecordMacroPage::showPreviewFrame(const std::string& previewName) {
    const std::string previewPath =
            PathUtils::join(getMacroPreviewsDirectory(), previewName + ".mp4");

    mVideoInfo.reset(
            new android::videoplayer::VideoInfo(mUi->videoWidget, previewPath));
    connect(mVideoInfo.get(), SIGNAL(updateWidget()), this,
            SLOT(updatePreviewVideoView()));

    mVideoInfo->show();
}

std::string RecordMacroPage::getMacroNameFromItem(QListWidgetItem* listItem) {
    QVariant data = listItem->data(Qt::UserRole);
    QString macroName = data.toString();
    return macroName.toUtf8().constData();
}
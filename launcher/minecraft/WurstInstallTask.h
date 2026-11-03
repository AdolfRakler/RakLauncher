// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QPair>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <functional>

#include "QObjectPtr.h"
#include "meta/Version.h"
#include "tasks/Task.h"

class WurstInstallTask final : public Task {
    Q_OBJECT

   public:
    WurstInstallTask(QString mcVersion, QString instanceName);
    ~WurstInstallTask() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private:
    void resolveLoader();
    void createInstance();
    void downloadMods();

    // Mod loader specific helpers.
    void resolveFabricApi();
    void onFabricApiResolved(const QByteArray& response);

    // Performance mods: Sodium (or a Sodium fork) on Fabric, OptiFine everywhere else.
    void resolvePerformanceMod();
    void queryModrinth(const QString& slug, std::function<void(const QJsonArray&)> onResult, std::function<void()> onFailure);
    bool addModrinthMod(const QJsonArray& versions, const QString& displayName);
    void resolveSodiumFork();
    void resolveOptiFabric();
    void onOptiFabricResolved(const QByteArray& response);
    void resolveOptiFine();
    void onOptiFineDownloadsFetched(const QByteArray& response);
    void onOptiFineAdPageFetched(const QByteArray& response);
    void commitPerformanceMods();
    void skipPerformanceMod(const QString& reason);

    // Runs a GET request and hands the response body to the success handler on the main thread.
    void fetchText(const QUrl& url, const QString& jobName, std::function<void(const QByteArray&)> onSuccess,
                   std::function<void()> onFailure = {});

    void startDownloads();

    QString m_mcVersion;
    QString m_instanceName;
    QString m_loaderUid;
    QString m_newInstanceId;
    QString m_modsDir;

    QString m_optiFabricFile;
    QString m_optiFabricName;
    QUrl m_optiFabricUrl;
    QString m_optiFineFile;
    QUrl m_optiFineUrl;
    bool m_optiFineNeedsOptiFabric = false;
    QStringList m_extraMods;
    int m_forkIndex = 0;

    Meta::Version::Ptr m_loaderVersion;
    Task::Ptr m_currentTask;
    unique_qobject_ptr<Task> m_stagingTask;

    QVector<QPair<QUrl, QString>> m_files;
};

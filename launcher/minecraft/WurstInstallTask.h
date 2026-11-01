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
#include <QPair>
#include <QUrl>
#include <QVector>

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
    void resolveFabricApi();
    void onFabricApiResolved();
    void startDownloads();

    QString m_mcVersion;
    QString m_instanceName;
    QString m_loaderUid;
    QString m_newInstanceId;
    QString m_modsDir;

    Meta::Version::Ptr m_loaderVersion;
    Task::Ptr m_currentTask;
    unique_qobject_ptr<Task> m_stagingTask;

    QByteArray* m_apiResponse = nullptr;
    QVector<QPair<QUrl, QString>> m_files;
};

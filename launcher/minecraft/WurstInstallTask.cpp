// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "WurstInstallTask.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <utility>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "modplatform/wurst/WurstVersions.h"
#include "net/ApiDownload.h"
#include "net/Download.h"
#include "net/NetJob.h"

WurstInstallTask::WurstInstallTask(QString mcVersion, QString instanceName)
    : m_mcVersion(std::move(mcVersion)), m_instanceName(std::move(instanceName))
{}

bool WurstInstallTask::abort()
{
    if (m_stagingTask) {
        m_stagingTask->abort();
    }
    if (m_currentTask) {
        return m_currentTask->abort();
    }
    return true;
}

void WurstInstallTask::executeTask()
{
    setStatus(tr("Preparing Wurst installation..."));
    m_loaderUid = Wurst::loaderUid(m_mcVersion);
    if (m_loaderUid.isEmpty()) {
        emitFailed(tr("Wurst is not available for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    auto list = APPLICATION->metadataIndex()->get(m_loaderUid);
    m_currentTask = list->getLoadTask();
    connect(m_currentTask.get(), &Task::succeeded, this, [this] { resolveLoader(); });
    connect(m_currentTask.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    m_currentTask->start();
}

void WurstInstallTask::resolveLoader()
{
    m_currentTask.reset();

    auto list = APPLICATION->metadataIndex()->get(m_loaderUid);
    m_loaderVersion = list->getLatestForParent("net.minecraft", m_mcVersion);
    if (!m_loaderVersion) {
        emitFailed(tr("Could not find a compatible %1 version for Minecraft %2. "
                      "Make sure the launcher has an Internet connection and try again.")
                       .arg(Wurst::loaderName(m_mcVersion), m_mcVersion));
        return;
    }

    createInstance();
}

void WurstInstallTask::createInstance()
{
    setStatus(tr("Creating Wurst instance..."));
    auto mcVersion = APPLICATION->metadataIndex()->get("net.minecraft", m_mcVersion);
    if (!mcVersion) {
        emitFailed(tr("Could not find Minecraft %1 in the version metadata. "
                      "Make sure the launcher has an Internet connection and try again.")
                       .arg(m_mcVersion));
        return;
    }

    auto* creationTask = new VanillaCreationTask(mcVersion, m_loaderUid, m_loaderVersion);
    creationTask->setName(m_instanceName);
    creationTask->setIcon("default");
    creationTask->setGroup("Wurst");

    m_stagingTask.reset(APPLICATION->instances()->wrapInstanceTask(creationTask));

    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, [this](const QString& id) {
        if (m_newInstanceId.isEmpty()) {
            m_newInstanceId = id;
        }
    });
    connect(m_stagingTask.data(), &Task::succeeded, this, [this] { downloadMods(); });
    connect(m_stagingTask.data(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    connect(m_stagingTask.data(), &Task::aborted, this, [this] { emitAborted(); });
    m_stagingTask->start();
}

void WurstInstallTask::downloadMods()
{
    m_stagingTask.reset();

    if (m_newInstanceId.isEmpty()) {
        emitFailed(tr("The Wurst instance was created, but its identifier could not be determined."));
        return;
    }

    auto* instance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->getInstanceById(m_newInstanceId));
    if (instance) {
        m_modsDir = instance->modsRoot();
    } else {
        // The instance list may not have picked up the freshly committed instance yet, so fall back to
        // the on-disk layout that VanillaCreationTask and MinecraftInstance use.
        const auto instanceRoot = FS::PathCombine(APPLICATION->settings()->get("InstanceDir").toString(), m_newInstanceId);
        const auto dotMinecraft = FS::PathCombine(instanceRoot, ".minecraft");
        const auto minecraft = FS::PathCombine(instanceRoot, "minecraft");
        const auto gameRoot = QFileInfo(dotMinecraft).exists() && !QFileInfo(minecraft).exists() ? dotMinecraft : minecraft;
        m_modsDir = FS::PathCombine(gameRoot, "mods");
    }

    if (!FS::ensureFolderPathExists(m_modsDir)) {
        emitFailed(tr("Could not create the mods folder for the Wurst instance."));
        return;
    }

    const auto* build = Wurst::buildFor(m_mcVersion);
    if (!build) {
        emitFailed(tr("Wurst is not available for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    m_files.append({ QUrl(build->url), FS::PathCombine(m_modsDir, QUrl(build->url).fileName()) });

    if (Wurst::usesFabric(m_mcVersion)) {
        resolveFabricApi();
    } else {
        startDownloads();
    }
}

void WurstInstallTask::resolveFabricApi()
{
    setStatus(tr("Resolving Fabric API..."));

    QUrl url("https://api.modrinth.com/v2/project/fabric-api/version");
    QUrlQuery query;
    query.addQueryItem("game_versions", QString("[\"%1\"]").arg(m_mcVersion));
    query.addQueryItem("loaders", QString("[\"fabric\"]"));
    url.setQuery(query);

    auto [action, response] = Net::ApiDownload::makeByteArray(url);
    m_apiResponse = response;

    auto job = makeShared<NetJob>(tr("Resolve Fabric API"), APPLICATION->network());
    job->addNetAction(action);
    connect(job.get(), &Task::succeeded, this, [this] { onFabricApiResolved(); });
    connect(job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    m_currentTask = job;
    job->start();
}

void WurstInstallTask::onFabricApiResolved()
{
    const QByteArray response = m_apiResponse ? *m_apiResponse : QByteArray();
    m_apiResponse = nullptr;
    m_currentTask.reset();

    if (response.isEmpty()) {
        emitFailed(tr("Could not resolve Fabric API for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    const auto versions = QJsonDocument::fromJson(response).array();
    if (versions.isEmpty()) {
        emitFailed(tr("Could not find Fabric API for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    const auto files = versions.first().toObject()["files"].toArray();
    if (files.isEmpty()) {
        emitFailed(tr("Could not find a Fabric API download for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    const auto file = files.first().toObject();
    const auto downloadUrl = QUrl(file["url"].toString());
    const auto fileName = file["filename"].toString();
    if (!downloadUrl.isValid() || fileName.isEmpty()) {
        emitFailed(tr("Could not find a Fabric API download for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    m_files.append({ downloadUrl, FS::PathCombine(m_modsDir, fileName) });
    startDownloads();
}

void WurstInstallTask::startDownloads()
{
    setStatus(tr("Downloading Wurst client..."));

    auto job = makeShared<NetJob>(tr("Download Wurst for %1").arg(m_mcVersion), APPLICATION->network());
    for (const auto& [url, path] : m_files) {
        job->addNetAction(Net::Download::makeFile(url, path));
    }

    connect(job.get(), &Task::succeeded, this, [this] { emitSucceeded(); });
    connect(job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    m_currentTask = job;
    job->start();
}

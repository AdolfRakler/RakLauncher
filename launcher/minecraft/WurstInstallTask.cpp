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
#include <QRegularExpression>
#include <QUrlQuery>
#include <utility>

#include "Application.h"
#include "BuildConfig.h"
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

namespace {

// OptiFabric is the Fabric compatibility layer required to load OptiFine on Fabric.
const auto OPTIFABRIC_PROJECT_ID = QStringLiteral("322385");

// Sodium forks and alternatives, in order of preference.
const QVector<QPair<QString, QString>>& sodiumForks()
{
    static const QVector<QPair<QString, QString>> forks = {
        { QStringLiteral("embeddium"), QStringLiteral("Embeddium") },
        { QStringLiteral("magnesium"), QStringLiteral("Magnesium") },
        { QStringLiteral("rubidium"), QStringLiteral("Rubidium") },
    };
    return forks;
}

QString curseForgeFileUrl(const QJsonObject& file)
{
    const auto downloadUrl = file["downloadUrl"].toString();
    if (!downloadUrl.isEmpty()) {
        return downloadUrl;
    }

    // CurseForge hides the download URL of some files until the project owner opts in,
    // but the CDN path is predictable and still works.
    const auto id = file["id"].toVariant().toLongLong();
    const auto fileName = file["fileName"].toString();
    if (id <= 0 || fileName.isEmpty()) {
        return {};
    }
    return QStringLiteral("https://%1/files/%2/%3/%4")
        .arg(BuildConfig.FLAME_DOWNLOAD_HOST)
        .arg(id / 1000)
        .arg(id % 1000, 3, 10, QChar('0'))
        .arg(fileName);
}

}  // namespace

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
    connect(m_currentTask.get(), &Task::aborted, this, [this] { emitAborted(); });
    m_currentTask->start();
}

void WurstInstallTask::resolveLoader()
{
    m_currentTask.reset();

    auto list = APPLICATION->metadataIndex()->get(m_loaderUid);

    if (Wurst::usesFabric(m_mcVersion)) {
        // Fabric Loader is not tied to a specific Minecraft version (it depends on intermediary instead),
        // so the version list has no parent requirements. Use the recommended build, like the new
        // instance dialog does.
        for (const auto& version : list->versions()) {
            if (version->isRecommended()) {
                m_loaderVersion = version;
                break;
            }
        }
        if (!m_loaderVersion && list->count() > 0) {
            m_loaderVersion = list->concreteAt(0);
        }
    } else {
        m_loaderVersion = list->getLatestForParent("net.minecraft", m_mcVersion);
    }

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
        resolvePerformanceMod();
    }
}

void WurstInstallTask::resolveFabricApi()
{
    setStatus(tr("Resolving Fabric API..."));

    QUrl url(QString("%1/project/fabric-api/version").arg(BuildConfig.MODRINTH_PROD_URL));
    QUrlQuery query;
    query.addQueryItem("game_versions", QString("[\"%1\"]").arg(m_mcVersion));
    query.addQueryItem("loaders", QString("[\"fabric\"]"));
    url.setQuery(query);

    fetchText(
        url, tr("Resolve Fabric API"), [this](const QByteArray& response) { onFabricApiResolved(response); },
        [this] { emitFailed(tr("Could not resolve Fabric API for Minecraft %1.").arg(m_mcVersion)); });
}

void WurstInstallTask::onFabricApiResolved(const QByteArray& response)
{
    const auto versions = QJsonDocument::fromJson(response).array();
    if (versions.isEmpty()) {
        emitFailed(tr("Could not find Fabric API for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    if (!addModrinthMod(versions, QStringLiteral("Fabric API"))) {
        emitFailed(tr("Could not find a Fabric API download for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    resolvePerformanceMod();
}

void WurstInstallTask::resolvePerformanceMod()
{
    if (!Wurst::usesFabric(m_mcVersion)) {
        // Wurst only uses Forge for 1.8 - 1.12.2, where OptiFine is the standard performance mod.
        resolveOptiFine();
        return;
    }

    setStatus(tr("Looking for Sodium..."));
    queryModrinth(
        QStringLiteral("sodium"),
        [this](const QJsonArray& versions) {
            if (addModrinthMod(versions, QStringLiteral("Sodium"))) {
                startDownloads();
            } else {
                resolveSodiumFork();
            }
        },
        [this] { resolveSodiumFork(); });
}

void WurstInstallTask::resolveSodiumFork()
{
    const auto& forks = sodiumForks();
    if (m_forkIndex >= forks.size()) {
        // No Sodium build exists for this Minecraft version, fall back to OptiFine
        // (paired with OptiFabric on Fabric).
        resolveOptiFine();
        return;
    }

    const auto slug = forks[m_forkIndex].first;
    const auto name = forks[m_forkIndex].second;
    m_forkIndex++;

    setStatus(tr("Looking for %1...").arg(name));
    queryModrinth(
        slug,
        [this, name](const QJsonArray& versions) {
            if (addModrinthMod(versions, name)) {
                startDownloads();
            } else {
                resolveSodiumFork();
            }
        },
        [this] { resolveSodiumFork(); });
}

void WurstInstallTask::resolveOptiFine()
{
    m_optiFineNeedsOptiFabric = Wurst::usesFabric(m_mcVersion);

    setStatus(tr("Looking for OptiFine..."));
    fetchText(
        QUrl(QStringLiteral("https://optifine.net/downloads")), tr("Find OptiFine"),
        [this](const QByteArray& response) { onOptiFineDownloadsFetched(response); },
        [this] { skipPerformanceMod(tr("Could not check the OptiFine download page.")); });
}

void WurstInstallTask::onOptiFineDownloadsFetched(const QByteArray& response)
{
    static const QRegularExpression linkRegex(QStringLiteral("adloadx\\?f=(OptiFine_[^\"'&<>]+\\.jar)"));

    const auto html = QString::fromUtf8(response);
    const auto marker = QStringLiteral("_%1_").arg(m_mcVersion);

    QString fileName;
    auto matches = linkRegex.globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto candidate = match.captured(1);
        if (candidate.contains(marker)) {
            fileName = candidate;
            break;
        }
    }

    if (fileName.isEmpty()) {
        skipPerformanceMod(tr("OptiFine is not available for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    m_optiFineFile = fileName;

    QUrl url(QStringLiteral("https://optifine.net/adloadx"));
    QUrlQuery query;
    query.addQueryItem("f", fileName);
    url.setQuery(query);

    fetchText(
        url, tr("Prepare OptiFine download"), [this](const QByteArray& page) { onOptiFineAdPageFetched(page); },
        [this] { skipPerformanceMod(tr("Could not prepare the OptiFine download.")); });
}

void WurstInstallTask::onOptiFineAdPageFetched(const QByteArray& response)
{
    static const QRegularExpression linkRegex(QStringLiteral("(downloadx\\?f=[^\"'<>]+)"));

    const auto match = linkRegex.match(QString::fromUtf8(response));
    if (!match.hasMatch()) {
        skipPerformanceMod(tr("Could not find the OptiFine download link."));
        return;
    }

    auto link = match.captured(1);
    link.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    m_optiFineUrl = QUrl(QStringLiteral("https://optifine.net/") + link);
    if (!m_optiFineUrl.isValid()) {
        skipPerformanceMod(tr("Could not find the OptiFine download link."));
        return;
    }

    if (m_optiFineNeedsOptiFabric) {
        resolveOptiFabric();
    } else {
        commitPerformanceMods();
    }
}

void WurstInstallTask::resolveOptiFabric()
{
    setStatus(tr("Looking for OptiFabric..."));

    QUrl url(QString("%1/mods/%2/files").arg(BuildConfig.FLAME_BASE_URL, OPTIFABRIC_PROJECT_ID));
    QUrlQuery query;
    query.addQueryItem("gameVersion", m_mcVersion);
    query.addQueryItem("pageSize", QStringLiteral("50"));
    url.setQuery(query);

    fetchText(
        url, tr("Find OptiFabric"), [this](const QByteArray& response) { onOptiFabricResolved(response); },
        [this] { skipPerformanceMod(tr("Could not resolve OptiFabric for Minecraft %1.").arg(m_mcVersion)); });
}

void WurstInstallTask::onOptiFabricResolved(const QByteArray& response)
{
    const auto files = QJsonDocument::fromJson(response).object()["data"].toArray();

    QJsonObject newest;
    QString newestDate;
    for (const auto& value : files) {
        const auto file = value.toObject();

        bool supportsFabric = false;
        for (const auto& loader : file["gameVersions"].toArray()) {
            if (loader.toString().compare(QStringLiteral("Fabric"), Qt::CaseInsensitive) == 0) {
                supportsFabric = true;
                break;
            }
        }
        if (!supportsFabric) {
            continue;
        }

        const auto date = file["fileDate"].toString();
        if (newest.isEmpty() || date > newestDate) {
            newest = file;
            newestDate = date;
        }
    }

    if (newest.isEmpty()) {
        skipPerformanceMod(
            tr("Sodium is not available for Minecraft %1 and OptiFabric has no build for it.").arg(m_mcVersion));
        return;
    }

    m_optiFabricUrl = QUrl(curseForgeFileUrl(newest));
    m_optiFabricFile = newest["fileName"].toString();
    m_optiFabricName = newest["displayName"].toString();
    if (!m_optiFabricUrl.isValid() || m_optiFabricFile.isEmpty()) {
        skipPerformanceMod(tr("Could not find an OptiFabric download for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    commitPerformanceMods();
}

void WurstInstallTask::commitPerformanceMods()
{
    if (m_optiFineFile.isEmpty() || !m_optiFineUrl.isValid()) {
        skipPerformanceMod(tr("Could not find an OptiFine download for Minecraft %1.").arg(m_mcVersion));
        return;
    }

    if (!m_optiFabricFile.isEmpty()) {
        m_files.append({ m_optiFabricUrl, FS::PathCombine(m_modsDir, m_optiFabricFile) });
        m_extraMods << m_optiFabricName.trimmed();
    }

    m_files.append({ m_optiFineUrl, FS::PathCombine(m_modsDir, m_optiFineFile) });
    m_extraMods << QStringLiteral("OptiFine (%1)").arg(m_optiFineFile);
    startDownloads();
}

void WurstInstallTask::skipPerformanceMod(const QString& reason)
{
    qWarning() << "Wurst install:" << reason;
    logWarning(reason);
    startDownloads();
}

void WurstInstallTask::queryModrinth(const QString& slug, std::function<void(const QJsonArray&)> onResult,
                                     std::function<void()> onFailure)
{
    QUrl url(QString("%1/project/%2/version").arg(BuildConfig.MODRINTH_PROD_URL, slug));
    QUrlQuery query;
    query.addQueryItem("game_versions", QString("[\"%1\"]").arg(m_mcVersion));
    query.addQueryItem("loaders", QString("[\"fabric\"]"));
    url.setQuery(query);

    fetchText(
        url, tr("Find %1").arg(slug),
        [onResult = std::move(onResult)](const QByteArray& response) {
            onResult(QJsonDocument::fromJson(response).array());
        },
        std::move(onFailure));
}

bool WurstInstallTask::addModrinthMod(const QJsonArray& versions, const QString& displayName)
{
    if (versions.isEmpty()) {
        return false;
    }

    // Prefer a stable release over a beta or alpha when one exists.
    QJsonObject chosen;
    for (const auto& value : versions) {
        const auto version = value.toObject();
        if (version["version_type"].toString() == QLatin1String("release")) {
            chosen = version;
            break;
        }
    }
    if (chosen.isEmpty()) {
        chosen = versions.first().toObject();
    }

    const auto files = chosen["files"].toArray();
    QJsonObject chosenFile;
    for (const auto& value : files) {
        const auto file = value.toObject();
        if (file["primary"].toBool()) {
            chosenFile = file;
            break;
        }
    }
    if (chosenFile.isEmpty() && !files.isEmpty()) {
        chosenFile = files.first().toObject();
    }

    const auto url = QUrl(chosenFile["url"].toString());
    const auto fileName = chosenFile["filename"].toString();
    if (!url.isValid() || fileName.isEmpty()) {
        return false;
    }

    m_files.append({ url, FS::PathCombine(m_modsDir, fileName) });
    m_extraMods << QStringLiteral("%1 %2").arg(displayName, chosen["version_number"].toString()).trimmed();
    return true;
}

void WurstInstallTask::fetchText(const QUrl& url, const QString& jobName, std::function<void(const QByteArray&)> onSuccess,
                                 std::function<void()> onFailure)
{
    auto [action, response] = Net::ApiDownload::makeByteArray(url);

    auto job = makeShared<NetJob>(jobName, APPLICATION->network());
    job->addNetAction(action);

    connect(job.get(), &Task::succeeded, this, [this, response, onSuccess = std::move(onSuccess)] {
        const QByteArray data = *response;
        m_currentTask.reset();
        onSuccess(data);
    });
    connect(job.get(), &Task::failed, this, [this, onFailure = std::move(onFailure)](const QString& reason) {
        m_currentTask.reset();
        if (onFailure) {
            onFailure();
        } else {
            emitFailed(reason);
        }
    });
    connect(job.get(), &Task::aborted, this, [this] {
        m_currentTask.reset();
        emitAborted();
    });

    m_currentTask = job;
    job->start();
}

void WurstInstallTask::startDownloads()
{
    QStringList items{ tr("Wurst client") };
    items += m_extraMods;
    setStatus(tr("Downloading %1...").arg(items.join(QStringLiteral(", "))));

    auto job = makeShared<NetJob>(tr("Download Wurst for %1").arg(m_mcVersion), APPLICATION->network());
    for (const auto& [url, path] : m_files) {
        job->addNetAction(Net::Download::makeFile(url, path));
    }

    connect(job.get(), &Task::succeeded, this, [this, items] {
        setStatus(tr("Installed %1.").arg(items.join(QStringLiteral(", "))));
        emitSucceeded();
    });
    connect(job.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    connect(job.get(), &Task::aborted, this, [this] { emitAborted(); });
    m_currentTask = job;
    job->start();
}

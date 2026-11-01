// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "WurstVersions.h"

#include <QHash>
#include <algorithm>

#include "Version.h"

namespace Wurst {

namespace {

// Latest stable Wurst release for every Minecraft version supported by Wurst 6 and Wurst 7.
// Wurst 6 (Forge) covers 1.8 - 1.12.2, Wurst 7 (Fabric) covers 1.14+.
const QList<Build>& allBuilds()
{
    static const QList<Build> builds = {
        { "26.3", "7.56.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.56.1/Wurst-Client-v7.56.1-MC26.3.jar" },
        { "26.2", "7.56.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.56.1/Wurst-Client-v7.56.1-MC26.2.jar" },
        { "26.1.2", "7.56.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.56.1/Wurst-Client-v7.56.1-MC26.1.2.jar" },
        { "26.1.1", "7.53.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.53.1/Wurst-Client-v7.53.1-MC26.1.1.jar" },
        { "26.1", "7.53.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.53.1/Wurst-Client-v7.53.1-MC26.1.jar" },
        { "1.21.11", "7.56.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.56.1/Wurst-Client-v7.56.1-MC1.21.11.jar" },
        { "1.21.10", "7.53", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.53/Wurst-Client-v7.53-MC1.21.10.jar" },
        { "1.21.9", "7.51", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.51/Wurst-Client-v7.51-MC1.21.9.jar" },
        { "1.21.8", "7.53", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.53/Wurst-Client-v7.53-MC1.21.8.jar" },
        { "1.21.7", "7.49", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.49/Wurst-Client-v7.49-MC1.21.7.jar" },
        { "1.21.6", "7.49", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.49/Wurst-Client-v7.49-MC1.21.6.jar" },
        { "1.21.5", "7.49", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.49/Wurst-Client-v7.49-MC1.21.5.jar" },
        { "1.21.4", "7.51.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.51.2/Wurst-Client-v7.51.2-MC1.21.4.jar" },
        { "1.21.3", "7.46.6", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.46.6/Wurst-Client-v7.46.6-MC1.21.3.jar" },
        { "1.21.2", "7.46.6", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.46.6/Wurst-Client-v7.46.6-MC1.21.2.jar" },
        { "1.21.1", "7.51.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.51.2/Wurst-Client-v7.51.2-MC1.21.1.jar" },
        { "1.21", "7.46.6", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.46.6/Wurst-Client-v7.46.6-MC1.21.jar" },
        { "1.20.6", "7.45", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.45/Wurst-Client-v7.45-MC1.20.6.jar" },
        { "1.20.5", "7.42", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.42/Wurst-Client-v7.42-MC1.20.5.jar" },
        { "1.20.4", "7.46.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.46.1/Wurst-Client-v7.46.1-MC1.20.4.jar" },
        { "1.20.3", "7.41.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.41.2/Wurst-Client-v7.41.2-MC1.20.3.jar" },
        { "1.20.2", "7.41.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.41.2/Wurst-Client-v7.41.2-MC1.20.2.jar" },
        { "1.20.1", "7.50.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.50.3/Wurst-Client-v7.50.3-MC1.20.1.jar" },
        { "1.20", "7.41.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.41.2/Wurst-Client-v7.41.2-MC1.20.jar" },
        { "1.19.4", "7.35.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.35.2/Wurst-Client-v7.35.2-MC1.19.4.jar" },
        { "1.19.3", "7.35.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.35.1/Wurst-Client-v7.35.1-MC1.19.3.jar" },
        { "1.19.2", "7.35.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.35.2/Wurst-Client-v7.35.2-MC1.19.2.jar" },
        { "1.19.1", "7.27.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.27.3/Wurst-Client-v7.27.3-MC1.19.1.jar" },
        { "1.19", "7.32", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.32/Wurst-Client-v7.32-MC1.19.jar" },
        { "1.18.2", "7.32", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.32/Wurst-Client-v7.32-MC1.18.2.jar" },
        { "1.18.1", "7.25", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.25/Wurst-Client-v7.25-MC1.18.1.jar" },
        { "1.18", "7.19", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.19/Wurst-Client-v7.19-MC1.18.jar" },
        { "1.17.1", "7.27.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.27.3/Wurst-Client-v7.27.3-MC1.17.1.jar" },
        { "1.17", "7.15.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.15.3/Wurst-Client-v7.15.3-MC1.17.jar" },
        { "1.16.5", "7.35.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.35.1/Wurst-Client-v7.35.1-MC1.16.5.jar" },
        { "1.16.4", "7.15.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.15.2/Wurst-Client-v7.15.2-MC1.16.4.jar" },
        { "1.16.3", "7.15.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.15.2/Wurst-Client-v7.15.2-MC1.16.3.jar" },
        { "1.16.2", "7.9", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.9/Wurst-Client-v7.9-MC1.16.2.jar" },
        { "1.16.1", "7.9", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.9/Wurst-Client-v7.9-MC1.16.1.jar" },
        { "1.16", "7.1.2", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.1.2/Wurst-Client-v7.1.2-MC1.16.jar" },
        { "1.15.2", "7.11", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.11/Wurst-Client-v7.11-MC1.15.2.jar" },
        { "1.15.1", "7.0", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.0/Wurst-Client-v7.0-MC1.15.1.jar" },
        { "1.15", "7.0", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.0/Wurst-Client-v7.0-MC1.15.jar" },
        { "1.14.4", "7.6.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.6.1/Wurst-Client-v7.6.1-MC1.14.4.jar" },
        { "1.14.3", "7.0", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.0/Wurst-Client-v7.0-MC1.14.3.jar" },
        { "1.14.2", "7.0", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v7.0/Wurst-Client-v7.0-MC1.14.2.jar" },
        { "1.12", "6.35.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v6.35.3/Wurst-Client-v6.35.3-MC1.12.jar" },
        { "1.11", "6.28.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v6.28.1/Wurst-Client-v6.28.1-MC1.11.jar" },
        { "1.10", "6.28.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v6.28.1/Wurst-Client-v6.28.1-MC1.10.jar" },
        { "1.9", "6.28.1", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v6.28.1/Wurst-Client-v6.28.1-MC1.9.jar" },
        { "1.8", "6.35.3", "https://github.com/Wurst-Imperium/Wurst-MCX2/releases/download/v6.35.3/Wurst-Client-v6.35.3-MC1.8.jar" },
    };
    return builds;
}

// Wurst jars are built for a specific Minecraft version but usually work for the whole
// patch range. Map every supported patch version to the jar that covers it.
const QHash<QString, QString>& aliases()
{
    static const QHash<QString, QString> map = {
        { "1.8.9", "1.8" },
        { "1.9.2", "1.9" },
        { "1.10.2", "1.10" },
        { "1.11.2", "1.11" },
        { "1.12.2", "1.12" },
        { "1.14.2", "1.14.4" },
        { "1.14.3", "1.14.4" },
        { "1.15", "1.15.2" },
        { "1.15.1", "1.15.2" },
        { "1.16", "1.16.1" },
        { "1.16.2", "1.16.5" },
        { "1.16.3", "1.16.5" },
        { "1.16.4", "1.16.5" },
        { "1.17", "1.17.1" },
        { "1.18", "1.18.1" },
        { "1.20", "1.20.1" },
    };
    return map;
}

}  // namespace

QStringList supportedMcVersions()
{
    QStringList versions;
    for (const auto& build : allBuilds()) {
        versions << build.mcVersion;
    }
    for (auto it = aliases().constBegin(); it != aliases().constEnd(); ++it) {
        versions << it.key();
    }

    versions.removeDuplicates();
    std::sort(versions.begin(), versions.end(), [](const QString& a, const QString& b) { return Version(a) > Version(b); });
    return versions;
}

const Build* buildFor(const QString& mcVersion)
{
    auto exact = std::find_if(allBuilds().constBegin(), allBuilds().constEnd(),
                              [&mcVersion](const Build& build) { return build.mcVersion == mcVersion; });
    if (exact != allBuilds().constEnd()) {
        return &*exact;
    }

    const auto alias = aliases().constFind(mcVersion);
    if (alias == aliases().constEnd()) {
        return nullptr;
    }

    auto aliased = std::find_if(allBuilds().constBegin(), allBuilds().constEnd(),
                                [&alias](const Build& build) { return build.mcVersion == alias.value(); });
    return aliased != allBuilds().constEnd() ? &*aliased : nullptr;
}

bool usesFabric(const QString& mcVersion)
{
    return Version(mcVersion) >= Version("1.14");
}

QString loaderUid(const QString& mcVersion)
{
    if (!buildFor(mcVersion)) {
        return {};
    }
    return usesFabric(mcVersion) ? QStringLiteral("net.fabricmc.fabric-loader") : QStringLiteral("net.minecraftforge");
}

QString loaderName(const QString& mcVersion)
{
    if (!buildFor(mcVersion)) {
        return {};
    }
    return usesFabric(mcVersion) ? QStringLiteral("Fabric") : QStringLiteral("Forge");
}

}  // namespace Wurst

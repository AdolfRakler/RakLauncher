// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace Wurst {

struct Build {
    QString mcVersion;
    QString wurstVersion;
    QString url;
};

/** All Minecraft versions we can install Wurst for, newest first. */
QStringList supportedMcVersions();

/** The best Wurst build for the given Minecraft version, or nullptr if unsupported. */
const Build* buildFor(const QString& mcVersion);

/** Wurst 7 (and therefore Fabric) is used for Minecraft 1.14 and newer. */
bool usesFabric(const QString& mcVersion);

/** Metadata uid of the mod loader required by Wurst for this Minecraft version. */
QString loaderUid(const QString& mcVersion);

/** Human readable name of the mod loader required by Wurst for this Minecraft version. */
QString loaderName(const QString& mcVersion);

}  // namespace Wurst

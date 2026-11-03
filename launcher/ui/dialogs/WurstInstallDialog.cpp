// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "WurstInstallDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Application.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "modplatform/wurst/WurstVersions.h"

WurstInstallDialog::WurstInstallDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Install Wurst client"));
    setModal(true);
    resize(560, 0);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(tr("<h3>Install Wurst client</h3>"), this);
    layout->addWidget(title);

    auto* description = new QLabel(
        tr("A separate instance will be created with the required mod loader "
           "(Fabric for Minecraft 1.14 and newer, Forge for 1.8 - 1.12.2). "
           "The latest Wurst client and, when needed, Fabric API will be downloaded into it. "
           "A performance mod is installed as well: Sodium (or a Sodium fork) on Fabric, "
           "or OptiFine when Sodium is not available for the selected Minecraft version."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* warning = new QLabel(tr("<b>Warning:</b> Wurst is a hacked client. Using it on multiplayer servers may get you banned."), this);
    warning->setWordWrap(true);
    layout->addWidget(warning);

    auto* form = new QFormLayout();
    m_versionCombo = new QComboBox(this);
    form->addRow(tr("Minecraft version:"), m_versionCombo);
    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("Instance name:"), m_nameEdit);
    layout->addLayout(form);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Install"));
    layout->addWidget(m_buttons);

    connect(m_versionCombo, &QComboBox::currentTextChanged, this, [this] { updateDefaultName(); });
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this] { updateOkButton(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto mcList = APPLICATION->metadataIndex()->get("net.minecraft");
    const bool filterByMetadata = mcList && mcList->isLoaded();

    for (const auto& mcVersion : Wurst::supportedMcVersions()) {
        if (filterByMetadata && !mcList->hasVersion(mcVersion)) {
            continue;
        }
        m_versionCombo->addItem(mcVersion);
    }

    // If the metadata index has not been loaded yet, or somehow lists nothing, show everything we know about.
    if (m_versionCombo->count() == 0) {
        for (const auto& mcVersion : Wurst::supportedMcVersions()) {
            m_versionCombo->addItem(mcVersion);
        }
    }

    updateDefaultName();
    updateOkButton();
}

QString WurstInstallDialog::selectedMinecraftVersion() const
{
    return m_versionCombo->currentText();
}

QString WurstInstallDialog::instanceName() const
{
    return m_nameEdit->text().trimmed();
}

void WurstInstallDialog::updateDefaultName()
{
    const auto mcVersion = selectedMinecraftVersion();
    if (mcVersion.isEmpty()) {
        return;
    }

    const auto currentName = m_nameEdit->text();
    if (currentName.isEmpty() || currentName.startsWith(QStringLiteral("Wurst "))) {
        m_nameEdit->setText(QStringLiteral("Wurst %1").arg(mcVersion));
    }
}

void WurstInstallDialog::updateOkButton()
{
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!instanceName().isEmpty());
}

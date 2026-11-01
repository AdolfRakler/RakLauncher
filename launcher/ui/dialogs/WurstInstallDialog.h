// SPDX-License-Identifier: GPL-3.0-only
/*
 *  RakLauncher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;

class WurstInstallDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit WurstInstallDialog(QWidget* parent = nullptr);

    QString selectedMinecraftVersion() const;
    QString instanceName() const;

   private:
    void updateDefaultName();
    void updateOkButton();

    QComboBox* m_versionCombo = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};

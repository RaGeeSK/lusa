#pragma once

#include <QList>
#include <QString>

#include "vaultentry.h"

class VaultService
{
public:
    VaultService();

    QString vaultFilePath() const;
    bool vaultExists() const;

    bool createVault(const QString &masterPassword, QString *errorMessage = nullptr);
    bool unlockVault(const QString &masterPassword, QList<VaultEntry> *entries, QString *errorMessage = nullptr) const;
    bool saveVault(const QList<VaultEntry> &entries, const QString &masterPassword, QString *errorMessage = nullptr) const;

private:
    bool writeEncryptedDocument(const QList<VaultEntry> &entries, const QString &masterPassword, QString *errorMessage) const;

    QString m_vaultFilePath;
};

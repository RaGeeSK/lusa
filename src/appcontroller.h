#pragma once

#include <QObject>
#include <QVariantMap>

#include "vaultentry.h"
#include "vaultlistmodel.h"
#include "vaultservice.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(VaultListModel *vaultModel READ vaultModel CONSTANT)
    Q_PROPERTY(bool vaultExists READ vaultExists NOTIFY vaultStateChanged)
    Q_PROPERTY(bool vaultUnlocked READ vaultUnlocked NOTIFY vaultStateChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectedEntryChanged)
    Q_PROPERTY(QString selectedEntryId READ selectedEntryId NOTIFY selectedEntryChanged)
    Q_PROPERTY(QVariantMap selectedEntry READ selectedEntry NOTIFY selectedEntryChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    VaultListModel *vaultModel();
    bool vaultExists() const;
    bool vaultUnlocked() const;
    bool hasSelection() const;
    QString selectedEntryId() const;
    QVariantMap selectedEntry() const;
    QString storagePath() const;
    QString statusMessage() const;
    QString errorMessage() const;

    Q_INVOKABLE bool createVault(const QString &masterPassword);
    Q_INVOKABLE bool unlockVault(const QString &masterPassword);
    Q_INVOKABLE void lockVault();
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void beginCreateEntry();
    Q_INVOKABLE void selectEntryByRow(int row);
    Q_INVOKABLE bool saveEntry(const QVariantMap &entryData);
    Q_INVOKABLE bool deleteCurrentEntry();
    Q_INVOKABLE bool copyText(const QString &text);
    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void setFavoritesOnly(bool enabled);
    Q_INVOKABLE QString generatePassword(int length, bool upper, bool lower, bool digits, bool symbols) const;
    Q_INVOKABLE int estimateStrengthScore(const QString &password) const;
    Q_INVOKABLE QString strengthLabel(const QString &password) const;

signals:
    void vaultStateChanged();
    void selectedEntryChanged();
    void statusMessageChanged();
    void errorMessageChanged();

private:
    int indexOfEntry(const QString &id) const;
    void refreshModel();
    void setStatusMessage(const QString &message);
    void setErrorMessage(const QString &message);
    static QList<VaultEntry> sortedEntries(QList<VaultEntry> entries);

    VaultListModel m_vaultModel;
    VaultService m_vaultService;
    QList<VaultEntry> m_entries;
    VaultEntry m_draftEntry;
    bool m_hasDraft = false;
    bool m_vaultUnlocked = false;
    QString m_selectedId;
    QString m_masterPassword;
    QString m_statusMessage;
    QString m_errorMessage;
};

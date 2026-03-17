#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "vaultentry.h"

class VaultListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        WebsiteRole,
        FavoriteRole,
        UpdatedAtRole,
        UpdatedAtLabelRole,
        TagsRole
    };

    explicit VaultListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int visibleCount() const;
    int totalCount() const;

    void setEntries(const QList<VaultEntry> &entries);
    void setSearchQuery(const QString &query);
    void setFavoritesOnly(bool enabled);

    Q_INVOKABLE QString entryIdAt(int row) const;
    Q_INVOKABLE QVariantMap entryAt(int row) const;

signals:
    void countChanged();

private:
    void rebuildVisibleEntries();
    bool matchesFilters(const VaultEntry &entry) const;

    QList<VaultEntry> m_entries;
    QList<VaultEntry> m_visibleEntries;
    QString m_searchQuery;
    bool m_favoritesOnly = false;
};

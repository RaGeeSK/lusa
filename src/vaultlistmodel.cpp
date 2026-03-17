#include "vaultlistmodel.h"

VaultListModel::VaultListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int VaultListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_visibleEntries.size();
}

QVariant VaultListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleEntries.size()) {
        return {};
    }

    const VaultEntry &entry = m_visibleEntries.at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case TitleRole:
        return entry.title.isEmpty() ? QStringLiteral("Untitled entry") : entry.title;
    case SubtitleRole:
        return entry.subtitle();
    case WebsiteRole:
        return entry.website;
    case FavoriteRole:
        return entry.favorite;
    case UpdatedAtRole:
        return entry.updatedAt;
    case UpdatedAtLabelRole:
        return entry.updatedAt.isValid() ? entry.updatedAt.toLocalTime().toString("dd MMM yyyy, HH:mm") : QString();
    case TagsRole:
        return entry.tagsSummary();
    default:
        return {};
    }
}

QHash<int, QByteArray> VaultListModel::roleNames() const
{
    return {
        { IdRole, "entryId" },
        { TitleRole, "title" },
        { SubtitleRole, "subtitle" },
        { WebsiteRole, "website" },
        { FavoriteRole, "favorite" },
        { UpdatedAtRole, "updatedAt" },
        { UpdatedAtLabelRole, "updatedAtLabel" },
        { TagsRole, "tags" },
    };
}

int VaultListModel::visibleCount() const
{
    return m_visibleEntries.size();
}

int VaultListModel::totalCount() const
{
    return m_entries.size();
}

void VaultListModel::setEntries(const QList<VaultEntry> &entries)
{
    const int previousTotalCount = m_entries.size();
    m_entries = entries;
    rebuildVisibleEntries();
    if (previousTotalCount != m_entries.size()) {
        emit countChanged();
    }
}

void VaultListModel::setSearchQuery(const QString &query)
{
    const QString normalized = query.trimmed();
    if (normalized == m_searchQuery) {
        return;
    }
    m_searchQuery = normalized;
    rebuildVisibleEntries();
}

void VaultListModel::setFavoritesOnly(bool enabled)
{
    if (enabled == m_favoritesOnly) {
        return;
    }
    m_favoritesOnly = enabled;
    rebuildVisibleEntries();
}

QString VaultListModel::entryIdAt(int row) const
{
    if (row < 0 || row >= m_visibleEntries.size()) {
        return {};
    }
    return m_visibleEntries.at(row).id;
}

QVariantMap VaultListModel::entryAt(int row) const
{
    if (row < 0 || row >= m_visibleEntries.size()) {
        return {};
    }
    return m_visibleEntries.at(row).toVariantMap();
}

void VaultListModel::rebuildVisibleEntries()
{
    const int previousVisibleCount = m_visibleEntries.size();

    beginResetModel();
    m_visibleEntries.clear();
    for (const VaultEntry &entry : m_entries) {
        if (matchesFilters(entry)) {
            m_visibleEntries.append(entry);
        }
    }
    endResetModel();

    if (previousVisibleCount != m_visibleEntries.size()) {
        emit countChanged();
    }
}

bool VaultListModel::matchesFilters(const VaultEntry &entry) const
{
    if (m_favoritesOnly && !entry.favorite) {
        return false;
    }

    if (m_searchQuery.isEmpty()) {
        return true;
    }

    const QString haystack = QStringList{
        entry.title,
        entry.username,
        entry.website,
        entry.notes,
        entry.tags.join(' ')
    }.join(' ');

    return haystack.contains(m_searchQuery, Qt::CaseInsensitive);
}

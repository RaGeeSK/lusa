#include "vaultentry.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace
{
QString fallbackTimestamp(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toString(Qt::ISODate) : QString();
}
}

QJsonObject VaultEntry::toJson() const
{
    QJsonArray tagsArray;
    for (const QString &tag : tags) {
        tagsArray.append(tag);
    }

    QJsonObject object;
    object["id"] = id;
    object["title"] = title;
    object["username"] = username;
    object["password"] = password;
    object["website"] = website;
    object["notes"] = notes;
    object["tags"] = tagsArray;
    object["favorite"] = favorite;
    object["createdAt"] = fallbackTimestamp(createdAt);
    object["updatedAt"] = fallbackTimestamp(updatedAt);
    return object;
}

QVariantMap VaultEntry::toVariantMap() const
{
    QVariantMap map;
    map.insert("id", id);
    map.insert("title", title);
    map.insert("username", username);
    map.insert("password", password);
    map.insert("website", website);
    map.insert("notes", notes);
    map.insert("tags", tags);
    map.insert("tagsText", tags.join(", "));
    map.insert("favorite", favorite);
    map.insert("createdAt", createdAt);
    map.insert("updatedAt", updatedAt);
    map.insert("createdAtLabel", createdAt.isValid() ? createdAt.toLocalTime().toString("dd MMM yyyy, HH:mm") : QString());
    map.insert("updatedAtLabel", updatedAt.isValid() ? updatedAt.toLocalTime().toString("dd MMM yyyy, HH:mm") : QString());
    return map;
}

QString VaultEntry::subtitle() const
{
    if (!username.trimmed().isEmpty()) {
        return username.trimmed();
    }

    if (!website.trimmed().isEmpty()) {
        return website.trimmed();
    }

    if (!tags.isEmpty()) {
        return tags.join("  ");
    }

    return QStringLiteral("Secure note");
}

QString VaultEntry::tagsSummary() const
{
    return tags.join(", ");
}

bool VaultEntry::fromJson(const QJsonObject &object, VaultEntry *entry, QString *errorMessage)
{
    if (!entry) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null vault entry pointer.");
        }
        return false;
    }

    const QString idValue = object.value("id").toString().trimmed();
    if (idValue.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault entry is missing an id.");
        }
        return false;
    }

    entry->id = idValue;
    entry->title = object.value("title").toString().trimmed();
    entry->username = object.value("username").toString();
    entry->password = object.value("password").toString();
    entry->website = object.value("website").toString().trimmed();
    entry->notes = object.value("notes").toString();
    entry->favorite = object.value("favorite").toBool(false);
    entry->createdAt = QDateTime::fromString(object.value("createdAt").toString(), Qt::ISODate);
    entry->updatedAt = QDateTime::fromString(object.value("updatedAt").toString(), Qt::ISODate);

    QStringList parsedTags;
    const QJsonArray tagsArray = object.value("tags").toArray();
    for (const QJsonValue &value : tagsArray) {
        const QString tag = value.toString().trimmed();
        if (!tag.isEmpty()) {
            parsedTags.append(tag);
        }
    }
    entry->tags = normalizeTags(parsedTags);
    return true;
}

VaultEntry VaultEntry::fromVariantMap(const QVariantMap &map)
{
    VaultEntry entry;
    entry.id = map.value("id").toString().trimmed();
    entry.title = map.value("title").toString().trimmed();
    entry.username = map.value("username").toString().trimmed();
    entry.password = map.value("password").toString();
    entry.website = map.value("website").toString().trimmed();
    entry.notes = map.value("notes").toString();
    entry.favorite = map.value("favorite").toBool(false);

    if (map.contains("tags") && map.value("tags").canConvert<QStringList>()) {
        entry.tags = normalizeTags(map.value("tags").toStringList());
    } else {
        entry.tags = parseTags(map.value("tagsText").toString());
    }

    const QDateTime created = map.value("createdAt").toDateTime();
    const QDateTime updated = map.value("updatedAt").toDateTime();
    entry.createdAt = created.isValid() ? created : QDateTime::currentDateTimeUtc();
    entry.updatedAt = updated.isValid() ? updated : QDateTime::currentDateTimeUtc();
    return entry;
}

QStringList VaultEntry::parseTags(const QString &text)
{
    return normalizeTags(text.split(QRegularExpression("[,;\\n]"), Qt::SkipEmptyParts));
}

QStringList VaultEntry::normalizeTags(const QStringList &tags)
{
    QStringList normalized;
    for (const QString &tagValue : tags) {
        const QString tag = tagValue.trimmed();
        if (!tag.isEmpty() && !normalized.contains(tag, Qt::CaseInsensitive)) {
            normalized.append(tag);
        }
    }
    return normalized;
}

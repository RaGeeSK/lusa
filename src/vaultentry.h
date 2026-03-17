#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class VaultEntry
{
public:
    QString id;
    QString title;
    QString username;
    QString password;
    QString website;
    QString notes;
    QStringList tags;
    bool favorite = false;
    QDateTime createdAt;
    QDateTime updatedAt;

    QJsonObject toJson() const;
    QVariantMap toVariantMap() const;
    QString subtitle() const;
    QString tagsSummary() const;

    static bool fromJson(const QJsonObject &object, VaultEntry *entry, QString *errorMessage = nullptr);
    static VaultEntry fromVariantMap(const QVariantMap &map);
    static QStringList parseTags(const QString &text);
    static QStringList normalizeTags(const QStringList &tags);
};

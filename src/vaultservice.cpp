#include "vaultservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include "crypto/cryptoservice.h"

namespace
{
constexpr int kPbkdf2Iterations = 210000;

QJsonArray serializeEntries(const QList<VaultEntry> &entries)
{
    QJsonArray array;
    for (const VaultEntry &entry : entries) {
        array.append(entry.toJson());
    }
    return array;
}

bool ensureParentDirectory(const QString &filePath, QString *errorMessage)
{
    const QFileInfo fileInfo(filePath);
    QDir parentDirectory = fileInfo.dir();
    if (parentDirectory.exists()) {
        return true;
    }

    if (parentDirectory.mkpath(".")) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Unable to create application data directory.");
    }
    return false;
}
}

VaultService::VaultService()
{
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_vaultFilePath = QDir(dataDirectory).filePath(QStringLiteral("vault.lusa"));
}

QString VaultService::vaultFilePath() const
{
    return m_vaultFilePath;
}

bool VaultService::vaultExists() const
{
    return QFileInfo::exists(m_vaultFilePath);
}

bool VaultService::createVault(const QString &masterPassword, QString *errorMessage)
{
    if (vaultExists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("An encrypted vault already exists in this location.");
        }
        return false;
    }

    return writeEncryptedDocument({}, masterPassword, errorMessage);
}

bool VaultService::unlockVault(const QString &masterPassword, QList<VaultEntry> *entries, QString *errorMessage) const
{
    if (!entries) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null entries target.");
        }
        return false;
    }

    QFile file(m_vaultFilePath);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Encrypted vault was not found.");
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to open vault file for reading.");
        }
        return false;
    }

    const QByteArray rawDocument = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(rawDocument, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault file is not valid JSON.");
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject kdf = root.value("kdf").toObject();
    const QJsonObject cipher = root.value("cipher").toObject();
    const QByteArray salt = QByteArray::fromBase64(kdf.value("salt").toString().toUtf8());
    const int iterations = kdf.value("iterations").toInt();

    QString cryptoError;
    const QByteArray key = CryptoService::deriveKey(masterPassword, salt, iterations, &cryptoError);
    if (key.isEmpty()) {
        if (errorMessage) {
            *errorMessage = cryptoError;
        }
        return false;
    }

    CryptoService::EncryptedPayload payload;
    payload.nonce = QByteArray::fromBase64(cipher.value("nonce").toString().toUtf8());
    payload.ciphertext = QByteArray::fromBase64(cipher.value("payload").toString().toUtf8());
    payload.tag = QByteArray::fromBase64(cipher.value("tag").toString().toUtf8());

    QByteArray plaintext;
    if (!CryptoService::decrypt(payload, key, &plaintext, &cryptoError)) {
        if (errorMessage) {
            *errorMessage = cryptoError;
        }
        return false;
    }

    const QJsonDocument decryptedDocument = QJsonDocument::fromJson(plaintext, &parseError);
    if (parseError.error != QJsonParseError::NoError || !decryptedDocument.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Decrypted vault payload is invalid.");
        }
        return false;
    }

    QList<VaultEntry> parsedEntries;
    const QJsonArray jsonEntries = decryptedDocument.object().value("entries").toArray();
    for (const QJsonValue &value : jsonEntries) {
        VaultEntry entry;
        QString entryError;
        if (!VaultEntry::fromJson(value.toObject(), &entry, &entryError)) {
            if (errorMessage) {
                *errorMessage = entryError;
            }
            return false;
        }
        parsedEntries.append(entry);
    }

    *entries = parsedEntries;
    return true;
}

bool VaultService::saveVault(const QList<VaultEntry> &entries, const QString &masterPassword, QString *errorMessage) const
{
    return writeEncryptedDocument(entries, masterPassword, errorMessage);
}

bool VaultService::writeEncryptedDocument(const QList<VaultEntry> &entries,
                                          const QString &masterPassword,
                                          QString *errorMessage) const
{
    if (!ensureParentDirectory(m_vaultFilePath, errorMessage)) {
        return false;
    }

    QJsonObject plainObject;
    plainObject["entries"] = serializeEntries(entries);
    plainObject["savedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    const QByteArray plaintext = QJsonDocument(plainObject).toJson(QJsonDocument::Compact);

    QString cryptoError;
    const QByteArray salt = CryptoService::randomBytes(16, &cryptoError);
    if (salt.isEmpty()) {
        if (errorMessage) {
            *errorMessage = cryptoError;
        }
        return false;
    }

    const QByteArray key = CryptoService::deriveKey(masterPassword, salt, kPbkdf2Iterations, &cryptoError);
    if (key.isEmpty()) {
        if (errorMessage) {
            *errorMessage = cryptoError;
        }
        return false;
    }

    CryptoService::EncryptedPayload payload;
    if (!CryptoService::encrypt(plaintext, key, &payload, &cryptoError)) {
        if (errorMessage) {
            *errorMessage = cryptoError;
        }
        return false;
    }

    QJsonObject root;
    root["version"] = 1;
    root["kdf"] = QJsonObject{
        { "algorithm", QStringLiteral("PBKDF2-HMAC-SHA256") },
        { "iterations", kPbkdf2Iterations },
        { "salt", QString::fromLatin1(salt.toBase64()) },
    };
    root["cipher"] = QJsonObject{
        { "algorithm", QStringLiteral("AES-256-GCM") },
        { "nonce", QString::fromLatin1(payload.nonce.toBase64()) },
        { "tag", QString::fromLatin1(payload.tag.toBase64()) },
        { "payload", QString::fromLatin1(payload.ciphertext.toBase64()) },
    };

    QSaveFile file(m_vaultFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to open vault file for writing.");
        }
        return false;
    }

    const QByteArray encryptedDocument = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(encryptedDocument) != encryptedDocument.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to write encrypted vault contents.");
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to finalize vault save operation.");
        }
        return false;
    }

    QFile::setPermissions(m_vaultFilePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

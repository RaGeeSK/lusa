#pragma once

#include <QByteArray>
#include <QString>

class CryptoService
{
public:
    struct EncryptedPayload {
        QByteArray nonce;
        QByteArray ciphertext;
        QByteArray tag;
    };

    static QByteArray randomBytes(int length, QString *errorMessage = nullptr);
    static QByteArray deriveKey(const QString &password, const QByteArray &salt, int iterations, QString *errorMessage = nullptr);
    static bool encrypt(const QByteArray &plaintext, const QByteArray &key, EncryptedPayload *payload, QString *errorMessage = nullptr);
    static bool decrypt(const EncryptedPayload &payload, const QByteArray &key, QByteArray *plaintext, QString *errorMessage = nullptr);
};

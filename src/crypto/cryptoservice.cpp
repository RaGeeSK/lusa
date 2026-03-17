#include "cryptoservice.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <windows.h>
#include <bcrypt.h>

#include <QString>

#pragma comment(lib, "bcrypt.lib")

namespace
{
QString statusMessage(const QString &fallback, NTSTATUS status)
{
    return QStringLiteral("%1 (0x%2)")
        .arg(fallback, QString::number(static_cast<quint32>(status), 16).rightJustified(8, u'0'));
}

bool openAlgorithm(BCRYPT_ALG_HANDLE *handle,
                   LPCWSTR algorithm,
                   ULONG flags,
                   const QString &fallback,
                   QString *errorMessage)
{
    *handle = nullptr;
    const NTSTATUS status = BCryptOpenAlgorithmProvider(handle, algorithm, nullptr, flags);
    if (BCRYPT_SUCCESS(status)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = statusMessage(fallback, status);
    }
    return false;
}

bool queryUlongProperty(BCRYPT_HANDLE handle,
                        LPCWSTR property,
                        ULONG *value,
                        const QString &fallback,
                        QString *errorMessage)
{
    ULONG bytesWritten = 0;
    const NTSTATUS status = BCryptGetProperty(handle,
                                              property,
                                              reinterpret_cast<PUCHAR>(value),
                                              sizeof(*value),
                                              &bytesWritten,
                                              0);
    if (BCRYPT_SUCCESS(status) && bytesWritten == sizeof(*value)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = statusMessage(fallback, status);
    }
    return false;
}
}

QByteArray CryptoService::randomBytes(int length, QString *errorMessage)
{
    if (length <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Requested random byte length must be positive.");
        }
        return {};
    }

    QByteArray buffer(length, Qt::Uninitialized);
    const NTSTATUS status = BCryptGenRandom(nullptr,
                                            reinterpret_cast<PUCHAR>(buffer.data()),
                                            static_cast<ULONG>(buffer.size()),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to generate secure random bytes."), status);
        }
        return {};
    }

    return buffer;
}

QByteArray CryptoService::deriveKey(const QString &password, const QByteArray &salt, int iterations, QString *errorMessage)
{
    if (password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Master password is empty.");
        }
        return {};
    }

    if (salt.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault salt is missing.");
        }
        return {};
    }

    if (iterations <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PBKDF2 iteration count must be positive.");
        }
        return {};
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!openAlgorithm(&algorithm,
                       BCRYPT_SHA256_ALGORITHM,
                       BCRYPT_ALG_HANDLE_HMAC_FLAG,
                       QStringLiteral("Failed to initialize PBKDF2-SHA256."),
                       errorMessage)) {
        return {};
    }

    const QByteArray passwordBytes = password.toUtf8();
    QByteArray key(32, Qt::Uninitialized);
    const NTSTATUS status = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char *>(passwordBytes.constData())),
        static_cast<ULONG>(passwordBytes.size()),
        reinterpret_cast<PUCHAR>(const_cast<char *>(salt.constData())),
        static_cast<ULONG>(salt.size()),
        static_cast<ULONGLONG>(iterations),
        reinterpret_cast<PUCHAR>(key.data()),
        static_cast<ULONG>(key.size()),
        0);

    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!BCRYPT_SUCCESS(status)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to derive encryption key."), status);
        }
        return {};
    }

    return key;
}

bool CryptoService::encrypt(const QByteArray &plaintext, const QByteArray &key, EncryptedPayload *payload, QString *errorMessage)
{
    if (!payload) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null encrypted payload.");
        }
        return false;
    }

    if (key.size() != 32) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Encryption key must be 32 bytes.");
        }
        return false;
    }

    QString randomError;
    payload->nonce = randomBytes(12, &randomError);
    if (payload->nonce.isEmpty()) {
        if (errorMessage) {
            *errorMessage = randomError;
        }
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!openAlgorithm(&algorithm,
                       BCRYPT_AES_ALGORITHM,
                       0,
                       QStringLiteral("Failed to initialize AES provider."),
                       errorMessage)) {
        return false;
    }

    const NTSTATUS chainStatus = BCryptSetProperty(algorithm,
                                                   BCRYPT_CHAINING_MODE,
                                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                                                   sizeof(BCRYPT_CHAIN_MODE_GCM),
                                                   0);
    if (!BCRYPT_SUCCESS(chainStatus)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to configure AES-GCM mode."), chainStatus);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    ULONG objectLength = 0;
    if (!queryUlongProperty(algorithm,
                            BCRYPT_OBJECT_LENGTH,
                            &objectLength,
                            QStringLiteral("Failed to query AES key object size."),
                            errorMessage)) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    QByteArray keyObject(static_cast<int>(objectLength), Qt::Uninitialized);
    BCRYPT_KEY_HANDLE keyHandle = nullptr;
    const NTSTATUS importStatus = BCryptGenerateSymmetricKey(
        algorithm,
        &keyHandle,
        reinterpret_cast<PUCHAR>(keyObject.data()),
        objectLength,
        reinterpret_cast<PUCHAR>(const_cast<char *>(key.constData())),
        static_cast<ULONG>(key.size()),
        0);

    if (!BCRYPT_SUCCESS(importStatus)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to create AES key."), importStatus);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    payload->ciphertext = QByteArray(plaintext.size(), Qt::Uninitialized);
    payload->tag = QByteArray(16, Qt::Uninitialized);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = reinterpret_cast<PUCHAR>(payload->nonce.data());
    authInfo.cbNonce = static_cast<ULONG>(payload->nonce.size());
    authInfo.pbTag = reinterpret_cast<PUCHAR>(payload->tag.data());
    authInfo.cbTag = static_cast<ULONG>(payload->tag.size());

    ULONG encryptedSize = 0;
    const NTSTATUS status = BCryptEncrypt(
        keyHandle,
        reinterpret_cast<PUCHAR>(const_cast<char *>(plaintext.constData())),
        static_cast<ULONG>(plaintext.size()),
        &authInfo,
        nullptr,
        0,
        reinterpret_cast<PUCHAR>(payload->ciphertext.data()),
        static_cast<ULONG>(payload->ciphertext.size()),
        &encryptedSize,
        0);

    BCryptDestroyKey(keyHandle);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!BCRYPT_SUCCESS(status)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed while encrypting vault payload."), status);
        }
        return false;
    }

    payload->ciphertext.resize(static_cast<int>(encryptedSize));
    return true;
}

bool CryptoService::decrypt(const EncryptedPayload &payload, const QByteArray &key, QByteArray *plaintext, QString *errorMessage)
{
    if (!plaintext) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null plaintext buffer.");
        }
        return false;
    }

    if (key.size() != 32) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Decryption key must be 32 bytes.");
        }
        return false;
    }

    if (payload.nonce.size() != 12 || payload.tag.size() != 16) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault payload has invalid AES-GCM metadata.");
        }
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!openAlgorithm(&algorithm,
                       BCRYPT_AES_ALGORITHM,
                       0,
                       QStringLiteral("Failed to initialize AES provider."),
                       errorMessage)) {
        return false;
    }

    const NTSTATUS chainStatus = BCryptSetProperty(algorithm,
                                                   BCRYPT_CHAINING_MODE,
                                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                                                   sizeof(BCRYPT_CHAIN_MODE_GCM),
                                                   0);
    if (!BCRYPT_SUCCESS(chainStatus)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to configure AES-GCM mode."), chainStatus);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    ULONG objectLength = 0;
    if (!queryUlongProperty(algorithm,
                            BCRYPT_OBJECT_LENGTH,
                            &objectLength,
                            QStringLiteral("Failed to query AES key object size."),
                            errorMessage)) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    QByteArray keyObject(static_cast<int>(objectLength), Qt::Uninitialized);
    BCRYPT_KEY_HANDLE keyHandle = nullptr;
    const NTSTATUS importStatus = BCryptGenerateSymmetricKey(
        algorithm,
        &keyHandle,
        reinterpret_cast<PUCHAR>(keyObject.data()),
        objectLength,
        reinterpret_cast<PUCHAR>(const_cast<char *>(key.constData())),
        static_cast<ULONG>(key.size()),
        0);

    if (!BCRYPT_SUCCESS(importStatus)) {
        if (errorMessage) {
            *errorMessage = statusMessage(QStringLiteral("Failed to create AES key."), importStatus);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    plaintext->resize(payload.ciphertext.size());

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char *>(payload.nonce.constData()));
    authInfo.cbNonce = static_cast<ULONG>(payload.nonce.size());
    authInfo.pbTag = reinterpret_cast<PUCHAR>(const_cast<char *>(payload.tag.constData()));
    authInfo.cbTag = static_cast<ULONG>(payload.tag.size());

    ULONG decryptedSize = 0;
    const NTSTATUS status = BCryptDecrypt(
        keyHandle,
        reinterpret_cast<PUCHAR>(const_cast<char *>(payload.ciphertext.constData())),
        static_cast<ULONG>(payload.ciphertext.size()),
        &authInfo,
        nullptr,
        0,
        reinterpret_cast<PUCHAR>(plaintext->data()),
        static_cast<ULONG>(plaintext->size()),
        &decryptedSize,
        0);

    BCryptDestroyKey(keyHandle);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!BCRYPT_SUCCESS(status)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to unlock vault. The master password may be incorrect or the file may be damaged.");
        }
        return false;
    }

    plaintext->resize(static_cast<int>(decryptedSize));
    return true;
}

#else

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace
{
QString opensslErrorMessage(const QString &fallback)
{
    const unsigned long errorCode = ERR_get_error();
    if (errorCode == 0) {
        return fallback;
    }

    char buffer[256] = {};
    ERR_error_string_n(errorCode, buffer, sizeof(buffer));
    return QString::fromLatin1(buffer);
}

const unsigned char *asConstUnsigned(const QByteArray &buffer)
{
    return reinterpret_cast<const unsigned char *>(buffer.constData());
}

unsigned char *asUnsigned(QByteArray &buffer)
{
    return reinterpret_cast<unsigned char *>(buffer.data());
}
}

QByteArray CryptoService::randomBytes(int length, QString *errorMessage)
{
    if (length <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Requested random byte length must be positive.");
        }
        return {};
    }

    QByteArray buffer(length, Qt::Uninitialized);
    if (RAND_bytes(asUnsigned(buffer), length) != 1) {
        if (errorMessage) {
            *errorMessage = opensslErrorMessage(QStringLiteral("Failed to generate secure random bytes."));
        }
        return {};
    }

    return buffer;
}

QByteArray CryptoService::deriveKey(const QString &password, const QByteArray &salt, int iterations, QString *errorMessage)
{
    if (password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Master password is empty.");
        }
        return {};
    }

    if (salt.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault salt is missing.");
        }
        return {};
    }

    if (iterations <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PBKDF2 iteration count must be positive.");
        }
        return {};
    }

    const QByteArray passwordBytes = password.toUtf8();
    QByteArray key(32, Qt::Uninitialized);
    const int ok = PKCS5_PBKDF2_HMAC(
        passwordBytes.constData(),
        passwordBytes.size(),
        asConstUnsigned(salt),
        salt.size(),
        iterations,
        EVP_sha256(),
        key.size(),
        asUnsigned(key));

    if (ok != 1) {
        if (errorMessage) {
            *errorMessage = opensslErrorMessage(QStringLiteral("Failed to derive encryption key."));
        }
        return {};
    }

    return key;
}

bool CryptoService::encrypt(const QByteArray &plaintext, const QByteArray &key, EncryptedPayload *payload, QString *errorMessage)
{
    if (!payload) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null encrypted payload.");
        }
        return false;
    }

    if (key.size() != 32) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Encryption key must be 32 bytes.");
        }
        return false;
    }

    QString randomError;
    payload->nonce = randomBytes(12, &randomError);
    if (payload->nonce.isEmpty()) {
        if (errorMessage) {
            *errorMessage = randomError;
        }
        return false;
    }

    payload->ciphertext = QByteArray(plaintext.size(), Qt::Uninitialized);
    payload->tag = QByteArray(16, Qt::Uninitialized);

    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create encryption context.");
        }
        return false;
    }

    bool success = false;
    int written = 0;
    int finalWritten = 0;

    do {
        if (EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to initialize AES-256-GCM."));
            }
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, payload->nonce.size(), nullptr) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to configure AES-GCM nonce."));
            }
            break;
        }

        if (EVP_EncryptInit_ex(context, nullptr, nullptr, asConstUnsigned(key), asConstUnsigned(payload->nonce)) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to bind AES-GCM key material."));
            }
            break;
        }

        if (EVP_EncryptUpdate(context,
                              asUnsigned(payload->ciphertext),
                              &written,
                              asConstUnsigned(plaintext),
                              plaintext.size()) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed while encrypting vault payload."));
            }
            break;
        }

        if (EVP_EncryptFinal_ex(context, asUnsigned(payload->ciphertext) + written, &finalWritten) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to finalize AES-GCM encryption."));
            }
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, payload->tag.size(), payload->tag.data()) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to extract AES-GCM tag."));
            }
            break;
        }

        payload->ciphertext.resize(written + finalWritten);
        success = true;
    } while (false);

    EVP_CIPHER_CTX_free(context);
    return success;
}

bool CryptoService::decrypt(const EncryptedPayload &payload, const QByteArray &key, QByteArray *plaintext, QString *errorMessage)
{
    if (!plaintext) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null plaintext buffer.");
        }
        return false;
    }

    if (key.size() != 32) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Decryption key must be 32 bytes.");
        }
        return false;
    }

    if (payload.nonce.size() != 12 || payload.tag.size() != 16) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Vault payload has invalid AES-GCM metadata.");
        }
        return false;
    }

    plaintext->resize(payload.ciphertext.size());

    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create decryption context.");
        }
        return false;
    }

    bool success = false;
    int written = 0;
    int finalWritten = 0;

    do {
        if (EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to initialize AES-256-GCM for reading."));
            }
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, payload.nonce.size(), nullptr) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to configure AES-GCM nonce for reading."));
            }
            break;
        }

        if (EVP_DecryptInit_ex(context, nullptr, nullptr, asConstUnsigned(key), asConstUnsigned(payload.nonce)) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to bind AES-GCM decryption key."));
            }
            break;
        }

        if (EVP_DecryptUpdate(context,
                              asUnsigned(*plaintext),
                              &written,
                              asConstUnsigned(payload.ciphertext),
                              payload.ciphertext.size()) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed while decrypting the vault."));
            }
            break;
        }

        if (EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, payload.tag.size(), const_cast<char *>(payload.tag.constData())) != 1) {
            if (errorMessage) {
                *errorMessage = opensslErrorMessage(QStringLiteral("Failed to set AES-GCM authentication tag."));
            }
            break;
        }

        if (EVP_DecryptFinal_ex(context, asUnsigned(*plaintext) + written, &finalWritten) != 1) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unable to unlock vault. The master password may be incorrect or the file may be damaged.");
            }
            break;
        }

        plaintext->resize(written + finalWritten);
        success = true;
    } while (false);

    EVP_CIPHER_CTX_free(context);
    return success;
}

#endif

#include "appcontroller.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace
{
QString generatedId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

VaultEntry makeBlankEntry()
{
    VaultEntry entry;
    entry.id = generatedId();
    entry.createdAt = QDateTime::currentDateTimeUtc();
    entry.updatedAt = entry.createdAt;
    return entry;
}

QString pickCharacter(const QString &source)
{
    if (source.isEmpty()) {
        return {};
    }

    const int index = QRandomGenerator::global()->bounded(source.size());
    return source.mid(index, 1);
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    refreshModel();
}

VaultListModel *AppController::vaultModel()
{
    return &m_vaultModel;
}

bool AppController::vaultExists() const
{
    return m_vaultService.vaultExists();
}

bool AppController::vaultUnlocked() const
{
    return m_vaultUnlocked;
}

bool AppController::hasSelection() const
{
    return !selectedEntryId().isEmpty();
}

QString AppController::selectedEntryId() const
{
    return m_selectedId;
}

QVariantMap AppController::selectedEntry() const
{
    if (m_hasDraft && m_draftEntry.id == m_selectedId) {
        return m_draftEntry.toVariantMap();
    }

    const int entryIndex = indexOfEntry(m_selectedId);
    if (entryIndex < 0) {
        return {};
    }

    return m_entries.at(entryIndex).toVariantMap();
}

QString AppController::storagePath() const
{
    return m_vaultService.vaultFilePath();
}

QString AppController::statusMessage() const
{
    return m_statusMessage;
}

QString AppController::errorMessage() const
{
    return m_errorMessage;
}

bool AppController::createVault(const QString &masterPassword)
{
    clearMessages();

    if (masterPassword.trimmed().size() < 10) {
        setErrorMessage(QStringLiteral("Use a stronger master password with at least 10 characters."));
        return false;
    }

    QString serviceError;
    if (!m_vaultService.createVault(masterPassword, &serviceError)) {
        setErrorMessage(serviceError);
        return false;
    }

    m_entries.clear();
    m_hasDraft = false;
    m_selectedId.clear();
    m_vaultUnlocked = true;
    m_masterPassword = masterPassword;
    refreshModel();
    emit vaultStateChanged();
    emit selectedEntryChanged();
    setStatusMessage(QStringLiteral("Encrypted vault created. Add your first credential when ready."));
    return true;
}

bool AppController::unlockVault(const QString &masterPassword)
{
    clearMessages();

    QList<VaultEntry> loadedEntries;
    QString serviceError;
    if (!m_vaultService.unlockVault(masterPassword, &loadedEntries, &serviceError)) {
        setErrorMessage(serviceError);
        return false;
    }

    m_entries = sortedEntries(loadedEntries);
    m_vaultUnlocked = true;
    m_masterPassword = masterPassword;
    m_hasDraft = false;
    m_selectedId = m_entries.isEmpty() ? QString() : m_entries.constFirst().id;
    refreshModel();
    emit vaultStateChanged();
    emit selectedEntryChanged();
    setStatusMessage(QStringLiteral("Vault unlocked."));
    return true;
}

void AppController::lockVault()
{
    if (!m_vaultUnlocked && m_entries.isEmpty() && !m_hasDraft) {
        return;
    }

    if (!m_masterPassword.isEmpty()) {
        m_masterPassword.fill(u'\0');
        m_masterPassword.clear();
    }

    m_entries.clear();
    m_hasDraft = false;
    m_selectedId.clear();
    m_vaultUnlocked = false;
    refreshModel();
    emit vaultStateChanged();
    emit selectedEntryChanged();
    clearMessages();
    setStatusMessage(QStringLiteral("Vault locked."));
}

void AppController::clearMessages()
{
    if (!m_statusMessage.isEmpty()) {
        m_statusMessage.clear();
        emit statusMessageChanged();
    }

    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged();
    }
}

void AppController::beginCreateEntry()
{
    if (!m_vaultUnlocked) {
        setErrorMessage(QStringLiteral("Unlock the vault before adding entries."));
        return;
    }

    clearMessages();
    m_draftEntry = makeBlankEntry();
    m_hasDraft = true;
    m_selectedId = m_draftEntry.id;
    emit selectedEntryChanged();
    setStatusMessage(QStringLiteral("New entry draft is ready."));
}

void AppController::selectEntryByRow(int row)
{
    const QString id = m_vaultModel.entryIdAt(row);
    if (id.isEmpty()) {
        return;
    }

    if (m_selectedId == id && !m_hasDraft) {
        return;
    }

    m_selectedId = id;
    m_hasDraft = false;
    emit selectedEntryChanged();
}

bool AppController::saveEntry(const QVariantMap &entryData)
{
    if (!m_vaultUnlocked) {
        setErrorMessage(QStringLiteral("Unlock the vault before saving entries."));
        return false;
    }

    VaultEntry entry = VaultEntry::fromVariantMap(entryData);
    if (entry.id.isEmpty()) {
        entry.id = generatedId();
    }

    if (entry.title.isEmpty()) {
        setErrorMessage(QStringLiteral("Every entry needs a title so you can find it later."));
        return false;
    }

    const int existingIndex = indexOfEntry(entry.id);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (existingIndex >= 0) {
        entry.createdAt = m_entries.at(existingIndex).createdAt.isValid() ? m_entries.at(existingIndex).createdAt : now;
        entry.updatedAt = now;
    } else {
        entry.createdAt = entry.createdAt.isValid() ? entry.createdAt : now;
        entry.updatedAt = now;
    }

    QList<VaultEntry> updatedEntries = m_entries;
    if (existingIndex >= 0) {
        updatedEntries[existingIndex] = entry;
    } else {
        updatedEntries.append(entry);
    }

    updatedEntries = sortedEntries(updatedEntries);

    QString serviceError;
    if (!m_vaultService.saveVault(updatedEntries, m_masterPassword, &serviceError)) {
        setErrorMessage(serviceError);
        return false;
    }

    m_entries = updatedEntries;
    m_selectedId = entry.id;
    m_hasDraft = false;
    refreshModel();
    emit selectedEntryChanged();
    setStatusMessage(existingIndex >= 0 ? QStringLiteral("Entry updated.") : QStringLiteral("Entry saved."));
    return true;
}

bool AppController::deleteCurrentEntry()
{
    if (!m_vaultUnlocked) {
        setErrorMessage(QStringLiteral("Unlock the vault before deleting entries."));
        return false;
    }

    if (m_hasDraft && m_draftEntry.id == m_selectedId) {
        m_hasDraft = false;
        m_selectedId.clear();
        emit selectedEntryChanged();
        setStatusMessage(QStringLiteral("Draft discarded."));
        return true;
    }

    const int existingIndex = indexOfEntry(m_selectedId);
    if (existingIndex < 0) {
        setErrorMessage(QStringLiteral("Select an entry before deleting it."));
        return false;
    }

    QList<VaultEntry> updatedEntries = m_entries;
    updatedEntries.removeAt(existingIndex);

    QString serviceError;
    if (!m_vaultService.saveVault(updatedEntries, m_masterPassword, &serviceError)) {
        setErrorMessage(serviceError);
        return false;
    }

    m_entries = sortedEntries(updatedEntries);
    refreshModel();
    m_selectedId = m_entries.isEmpty() ? QString() : m_entries.constFirst().id;
    emit selectedEntryChanged();
    setStatusMessage(QStringLiteral("Entry deleted."));
    return true;
}

bool AppController::copyText(const QString &text)
{
    if (text.isEmpty()) {
        setErrorMessage(QStringLiteral("There is nothing to copy."));
        return false;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        setErrorMessage(QStringLiteral("Clipboard is not available in this session."));
        return false;
    }

    clipboard->setText(text);
    setStatusMessage(QStringLiteral("Copied to clipboard."));
    return true;
}

void AppController::setSearchQuery(const QString &query)
{
    m_vaultModel.setSearchQuery(query);
}

void AppController::setFavoritesOnly(bool enabled)
{
    m_vaultModel.setFavoritesOnly(enabled);
}

QString AppController::generatePassword(int length, bool upper, bool lower, bool digits, bool symbols) const
{
    QStringList enabledSets;
    if (upper) {
        enabledSets.append(QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ"));
    }
    if (lower) {
        enabledSets.append(QStringLiteral("abcdefghijkmnopqrstuvwxyz"));
    }
    if (digits) {
        enabledSets.append(QStringLiteral("23456789"));
    }
    if (symbols) {
        enabledSets.append(QStringLiteral("!@#$%^&*_-+=?"));
    }

    if (enabledSets.isEmpty()) {
        return {};
    }

    const int targetLength = std::max(length, enabledSets.size());
    QString allCharacters;
    QString password;

    for (const QString &set : enabledSets) {
        allCharacters += set;
        password += pickCharacter(set);
    }

    while (password.size() < targetLength) {
        password += pickCharacter(allCharacters);
    }

    for (int i = password.size() - 1; i > 0; --i) {
        const int swapIndex = QRandomGenerator::global()->bounded(i + 1);
        const QChar temporary = password.at(i);
        password[i] = password.at(swapIndex);
        password[swapIndex] = temporary;
    }

    return password;
}

int AppController::estimateStrengthScore(const QString &password) const
{
    if (password.isEmpty()) {
        return 0;
    }

    int score = 0;
    const bool hasLower = password.contains(QRegularExpression(QStringLiteral("[a-z]")));
    const bool hasUpper = password.contains(QRegularExpression(QStringLiteral("[A-Z]")));
    const bool hasDigit = password.contains(QRegularExpression(QStringLiteral("\\d")));
    const bool hasSymbol = password.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9]")));
    const int categories = int(hasLower) + int(hasUpper) + int(hasDigit) + int(hasSymbol);

    if (password.size() >= 8) {
        ++score;
    }
    if (password.size() >= 12) {
        ++score;
    }
    if (password.size() >= 16) {
        ++score;
    }
    if (categories >= 3) {
        ++score;
    }
    if (categories == 4 && password.size() >= 12) {
        ++score;
    }

    return std::clamp(score, 0, 4);
}

QString AppController::strengthLabel(const QString &password) const
{
    switch (estimateStrengthScore(password)) {
    case 0:
        return QStringLiteral("Very weak");
    case 1:
        return QStringLiteral("Weak");
    case 2:
        return QStringLiteral("Fair");
    case 3:
        return QStringLiteral("Strong");
    case 4:
        return QStringLiteral("Excellent");
    default:
        return QStringLiteral("Unknown");
    }
}

int AppController::indexOfEntry(const QString &id) const
{
    if (id.isEmpty()) {
        return -1;
    }

    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id == id) {
            return index;
        }
    }

    return -1;
}

void AppController::refreshModel()
{
    m_vaultModel.setEntries(m_entries);
}

void AppController::setStatusMessage(const QString &message)
{
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged();
    }

    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppController::setErrorMessage(const QString &message)
{
    if (!m_statusMessage.isEmpty()) {
        m_statusMessage.clear();
        emit statusMessageChanged();
    }

    if (m_errorMessage == message) {
        return;
    }

    m_errorMessage = message;
    emit errorMessageChanged();
}

QList<VaultEntry> AppController::sortedEntries(QList<VaultEntry> entries)
{
    std::sort(entries.begin(), entries.end(), [](const VaultEntry &left, const VaultEntry &right) {
        if (left.favorite != right.favorite) {
            return left.favorite && !right.favorite;
        }

        if (left.updatedAt != right.updatedAt) {
            return left.updatedAt > right.updatedAt;
        }

        return QString::localeAwareCompare(left.title, right.title) < 0;
    });

    return entries;
}

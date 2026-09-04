#pragma once
#include <QObject>
#include <QProcess>
#include <QVector>
#include <QString>

// ساختار یک پکیج قابل آپدیت
struct PackageInfo {
    QString name;
    QString id;
    QString version;
    QString availableVersion;
    QString source;
    bool selected = true; // برای آپدیت دسته‌جمعی

    bool isValid() const { return !id.isEmpty() && !availableVersion.isEmpty(); }
};

class WingetManager : public QObject {
    Q_OBJECT
public:
    explicit WingetManager(QObject *parent = nullptr);
    ~WingetManager();

    bool isWingetAvailable() const;
    QString wingetVersion() const;

    // بررسی آپدیت‌ها (غیرهمزمان)
    void checkForUpdates(bool includeUnknown = false, bool includePinned = false);

    // آپدیت یک پکیج خاص
    void upgradePackage(const QString &id, bool silent = true);

    // آپدیت همه
    void upgradeAll(bool silent = true);

    // لغو عملیات جاری
    void cancelCurrentOperation();

    // پارسر خروجی winget (public برای تست)
    static QVector<PackageInfo> parseWingetUpgradeOutput(const QString &output);
    static QVector<PackageInfo> parseWingetListUpgradeAvailable(const QString &output);

signals:
    void checkStarted();
    void checkFinished(const QVector<PackageInfo> &packages);
    void checkError(const QString &error);

    void upgradeStarted(const QString &id); // id = "ALL" برای آپدیت همه
    void upgradeProgress(const QString &id, const QString &outputLine);
    void upgradeFinished(const QString &id, bool success, const QString &message);
    void upgradeError(const QString &id, const QString &error);

private slots:
    void onCheckProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onCheckProcessError(QProcess::ProcessError error);
    void onUpgradeProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onUpgradeProcessReadyRead();

private:
    QProcess *m_checkProcess = nullptr;
    QProcess *m_upgradeProcess = nullptr;
    QString m_currentUpgradeId;
    QString m_checkBuffer;
    QString m_upgradeBuffer;

    QVector<PackageInfo> parseOutputInternal(const QString &raw);

    static QStringList splitTableRow(const QString &header, const QString &dashLine, const QString &row);
    static int findColumnStart(const QString &dashLine, int colIndex);
};

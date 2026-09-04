#include "WingetManager.h"
#include <QProcess>
#include <QRegularExpression>
#include <QTextCodec>
#include <QDebug>

// Helper: اجرای winget --version برای تشخیص نصب بودن
bool WingetManager::isWingetAvailable() const {
    QProcess p;
    p.start("winget", QStringList() << "--version");
    if (!p.waitForFinished(3000)) return false;
    return p.exitCode() == 0;
}

QString WingetManager::wingetVersion() const {
    QProcess p;
    p.start("winget", QStringList() << "--version");
    if (!p.waitForFinished(3000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

WingetManager::WingetManager(QObject *parent) : QObject(parent) {}

WingetManager::~WingetManager() {
    cancelCurrentOperation();
}

void WingetManager::cancelCurrentOperation() {
    if (m_checkProcess) {
        m_checkProcess->kill();
        m_checkProcess->deleteLater();
        m_checkProcess = nullptr;
    }
    if (m_upgradeProcess) {
        m_upgradeProcess->kill();
        m_upgradeProcess->deleteLater();
        m_upgradeProcess = nullptr;
    }
}

void WingetManager::checkForUpdates(bool includeUnknown, bool includePinned) {
    if (m_checkProcess) {
        m_checkProcess->kill();
        m_checkProcess->deleteLater();
    }

    m_checkProcess = new QProcess(this);
    m_checkBuffer.clear();

    connect(m_checkProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        m_checkBuffer += QString::fromUtf8(m_checkProcess->readAllStandardOutput());
    });
    connect(m_checkProcess, &QProcess::readyReadStandardError, this, [this]() {
        m_checkBuffer += QString::fromUtf8(m_checkProcess->readAllStandardError());
    });
    connect(m_checkProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WingetManager::onCheckProcessFinished);
    connect(m_checkProcess, &QProcess::errorOccurred, this, &WingetManager::onCheckProcessError);

    QStringList args;
    args << "upgrade";
    if (includeUnknown) args << "--include-unknown";
    if (includePinned) args << "--include-pinned";
    // برای جلوگیری از پرامپت تعاملی
    args << "--accept-source-agreements";

    // تنظیم UTF8
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    m_checkProcess->setProcessEnvironment(env);

    emit checkStarted();
    m_checkProcess->start("winget", args);
    if (!m_checkProcess->waitForStarted(5000)) {
        emit checkError(QStringLiteral("خطا در اجرای winget: شروع نشد"));
        m_checkProcess->deleteLater();
        m_checkProcess = nullptr;
    }
}

void WingetManager::onCheckProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    if (!m_checkProcess) return;

    // باقی مانده خروجی را بخوان
    m_checkBuffer += QString::fromUtf8(m_checkProcess->readAllStandardOutput());
    m_checkBuffer += QString::fromUtf8(m_checkProcess->readAllStandardError());

    // winget وقتی آپدیتی نیست هم exit 0 میده ولی پیغام خاص دارد
    // فقط خطاهای واقعی رو گزارش کن: exitCode !=0 و بافر خالی یا شامل No installed...
    if (exitCode != 0 && m_checkBuffer.trimmed().isEmpty()) {
        emit checkError(QStringLiteral("winget با کد %1 خارج شد").arg(exitCode));
        m_checkProcess->deleteLater();
        m_checkProcess = nullptr;
        return;
    }

    auto packages = parseWingetUpgradeOutput(m_checkBuffer);
    emit checkFinished(packages);

    m_checkProcess->deleteLater();
    m_checkProcess = nullptr;
}

void WingetManager::onCheckProcessError(QProcess::ProcessError error) {
    Q_UNUSED(error)
    if (m_checkProcess) {
        emit checkError(m_checkProcess->errorString());
    }
}

void WingetManager::upgradePackage(const QString &id, bool silent) {
    if (m_upgradeProcess) {
        m_upgradeProcess->kill();
        m_upgradeProcess->deleteLater();
    }
    m_upgradeProcess = new QProcess(this);
    m_currentUpgradeId = id;
    m_upgradeBuffer.clear();

    connect(m_upgradeProcess, &QProcess::readyReadStandardOutput, this, &WingetManager::onUpgradeProcessReadyRead);
    connect(m_upgradeProcess, &QProcess::readyReadStandardError, this, &WingetManager::onUpgradeProcessReadyRead);
    connect(m_upgradeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WingetManager::onUpgradeProcessFinished);

    QStringList args;
    args << "upgrade" << "--id" << id << "-e"
         << "--accept-package-agreements" << "--accept-source-agreements";
    if (silent) args << "--silent" << "--disable-interactivity";

    emit upgradeStarted(id);
    m_upgradeProcess->start("winget", args);
}

void WingetManager::upgradeAll(bool silent) {
    if (m_upgradeProcess) {
        m_upgradeProcess->kill();
        m_upgradeProcess->deleteLater();
    }
    m_upgradeProcess = new QProcess(this);
    m_currentUpgradeId = QStringLiteral("ALL");
    m_upgradeBuffer.clear();

    connect(m_upgradeProcess, &QProcess::readyReadStandardOutput, this, &WingetManager::onUpgradeProcessReadyRead);
    connect(m_upgradeProcess, &QProcess::readyReadStandardError, this, &WingetManager::onUpgradeProcessReadyRead);
    connect(m_upgradeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WingetManager::onUpgradeProcessFinished);

    QStringList args;
    args << "upgrade" << "--all"
         << "--accept-package-agreements" << "--accept-source-agreements";
    if (silent) args << "--silent" << "--disable-interactivity";

    emit upgradeStarted(QStringLiteral("ALL"));
    m_upgradeProcess->start("winget", args);
}

void WingetManager::onUpgradeProcessReadyRead() {
    if (!m_upgradeProcess) return;
    QString out = QString::fromUtf8(m_upgradeProcess->readAllStandardOutput());
    QString err = QString::fromUtf8(m_upgradeProcess->readAllStandardError());
    QString combined = out + err;
    if (!combined.isEmpty()) {
        m_upgradeBuffer += combined;
        // خط به خط ارسال کن برای لاگ
        QStringList lines = combined.split('\n', Qt::SkipEmptyParts);
        for (const auto &l : lines) {
            emit upgradeProgress(m_currentUpgradeId, l.trimmed());
        }
    }
}

void WingetManager::onUpgradeProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    if (!m_upgradeProcess) return;
    m_upgradeBuffer += QString::fromUtf8(m_upgradeProcess->readAllStandardOutput());
    m_upgradeBuffer += QString::fromUtf8(m_upgradeProcess->readAllStandardError());

    bool success = (exitCode == 0);
    // بعضی نصب‌ها کد 0 نمی‌دهند ولی در خروجی Successfully installed دارند
    if (!success && m_upgradeBuffer.contains("Successfully installed", Qt::CaseInsensitive)) {
        success = true;
    }
    if (!success && m_upgradeBuffer.contains(QStringLiteral("No available upgrade"), Qt::CaseInsensitive)) {
        success = true;
    }

    emit upgradeFinished(m_currentUpgradeId, success, m_upgradeBuffer);

    m_upgradeProcess->deleteLater();
    m_upgradeProcess = nullptr;
}

// ============================================================
// Parser - قلب برنامه: پارس خروجی جدولی winget
// خروجی نمونه:
// Name               Id                          Version   Available Source
// ---------------------------------------------------------------------------
// AB Download Manager amir1376.ABDownloadManager 1.10.1    1.10.2    winget
// ============================================================
QVector<PackageInfo> WingetManager::parseWingetUpgradeOutput(const QString &output) {
    QVector<PackageInfo> result;
    // حذف BOM و نرمال‌سازی
    QString clean = output;
    clean.remove(QChar(0xFEFF));
    clean.remove(QChar(0xFFFE));
    // winget خروجی را با \r\n می‌دهد
    clean.replace("\r\n", "\n");
    clean.replace('\r', '\n');
    if (clean.trimmed().isEmpty()) return result;

    // اگر پیغام "No installed package found" یا "No available upgrade" باشد
    if (clean.contains("No installed package found", Qt::CaseInsensitive) ||
        clean.contains("No available upgrade", Qt::CaseInsensitive) ||
        clean.contains("No upgrade found", Qt::CaseInsensitive) ||
        clean.contains("No package found", Qt::CaseInsensitive)) {
        return result;
    }

    QStringList lines = clean.split('\n');

    int headerIdx = -1;
    int dashIdx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        QString t = lines[i];
        t.remove(QChar(0xFEFF));
        t = t.trimmed();
        // جستجو مقاوم به BOM و فاصله
        if (t.contains("Name") && t.contains("Id") && t.contains("Version")) {
            // اطمینان: باید هدر باشد نه داده
            if (t.contains("Available") || t.contains("Source")) {
                headerIdx = i;
                // خط بعد باید شامل --- باشد (ممکن است یک خط dash طولانی باشد)
                if (i + 1 < lines.size() && lines[i+1].contains("---")) {
                    dashIdx = i + 1;
                } else {
                    // گاهی dash با فاصله جدا نیست، به هر حال قبول کن
                    for (int k = i+1; k < qMin(i+4, lines.size()); ++k) {
                        if (lines[k].contains("---")) { dashIdx = k; break; }
                    }
                }
                break;
            }
        }
    }

    if (headerIdx == -1 || dashIdx == -1) {
        // fallback: تلاش با regex ساده اگر هدر پیدا نشد
        // بعضی نسخه‌های winget هدر فارسی یا ستون‌های متفاوت دارند
        return result;
    }

    QString header = lines[headerIdx];
    QString dashLine = lines[dashIdx];

    // پیدا کردن موقعیت ستون‌ها از خط dash
    // ستون‌ها با فاصله از هم جدا شده‌اند ولی داخل نام ممکن است فاصله باشد
    // روش درست: از موقعیت dash ها استفاده کن
    // ستون‌های استاندارد: Name | Id | Version | Available | Source
    // dashLine مثل: "----  --  ------- --------- ------"
    // ما start هر ستون را از dashLine استخراج می‌کنیم

    // استخراج بازه‌های ستون‌ها
    struct Col { int start; int end; QString name; };
    QVector<Col> cols;

    // شناخته شده‌ترین ستون‌ها به ترتیب
    // از dashLine، هر بلوک از '-' یا '─' یک ستون را نشان می‌دهد
    QRegularExpression dashRe(R"((-+))");
    auto it = dashRe.globalMatch(dashLine);
    while (it.hasNext()) {
        auto m = it.next();
        Col c;
        c.start = m.capturedStart();
        c.end = m.capturedEnd(); // exclusive
        cols.append(c);
    }

    // اگر تعداد ستون‌ها کمتر از 4 باشد یعنی dash یک بلوک طولانی است (winget جدید)
    // در این حالت از موقعیت هدر برای استخراج fixed-width استفاده می‌کنیم — بسیار مقاوم
    if (cols.size() < 4) {
        // پیدا کردن موقعیت هر ستون از هدر
        int namePos = header.indexOf("Name");
        int idPos = header.indexOf("Id");
        int versionPos = header.indexOf("Version");
        int availablePos = header.indexOf("Available");
        int sourcePos = header.indexOf("Source");

        // اگر هدر غیراستاندارد بود، fallback به split
        bool havePos = (idPos > 0 && versionPos > idPos && availablePos > versionPos);
        if (!havePos) {
            for (int i = dashIdx + 1; i < lines.size(); ++i) {
                QString line = lines[i];
                if (line.trimmed().isEmpty()) continue;
                if (line.trimmed().startsWith("---")) continue;
                if (line.contains("upgrades available", Qt::CaseInsensitive)) break;
                QStringList parts = line.trimmed().split(QRegularExpression(R"(\s{2,})"));
                if (parts.size() < 4) continue;
                PackageInfo p;
                if (parts.size() >= 5) {
                    p.name = parts[0].trimmed(); p.id = parts[1].trimmed();
                    p.version = parts[2].trimmed(); p.availableVersion = parts[3].trimmed();
                    p.source = parts[4].trimmed();
                } else if (parts.size() == 4) {
                    p.name = parts[0].trimmed(); p.id = parts[1].trimmed();
                    p.version = parts[2].trimmed(); p.availableVersion = parts[3].trimmed();
                    p.source = QStringLiteral("winget");
                }
                if (p.isValid()) result.append(p);
            }
            return result;
        }

        // استخراج fixed-width
        if (namePos < 0) namePos = 0;
        if (sourcePos < 0) sourcePos = header.length(); // اگر Source نباشد

        for (int i = dashIdx + 1; i < lines.size(); ++i) {
            QString line = lines[i];
            if (line.trimmed().isEmpty()) continue;
            if (line.trimmed().startsWith("---")) continue;
            if (line.contains("upgrades available", Qt::CaseInsensitive)) break;
            if (line.contains("package(s) have version", Qt::CaseInsensitive)) continue;
            if (line.trimmed().length() < 10) continue;

            // پد کردن برای خط‌های کوتاه
            QString padded = line;
            int needed = sourcePos + 10;
            if (padded.length() < needed) padded = padded.leftJustified(needed, ' ');
            // اگر خط طولانی‌تر از هدر است، همان را نگه دار (برای نام‌های خیلی طولانی)
            // ولی ستون‌ها را بر اساس هدر ببر

            auto slice = [&](int start, int end) -> QString {
                if (start < 0) start = 0;
                if (start >= padded.length()) return {};
                if (end > padded.length()) end = padded.length();
                if (end <= start) return {};
                return padded.mid(start, end - start).trimmed();
            };

            PackageInfo p;
            p.name = slice(namePos, idPos);
            p.id = slice(idPos, versionPos);
            p.version = slice(versionPos, availablePos);
            p.availableVersion = slice(availablePos, sourcePos);
            p.source = slice(sourcePos, padded.length());
            if (p.source.isEmpty()) p.source = QStringLiteral("winget");

            // تمیزکاری: بعضی سطرها ممکن است Id با Version چسبیده باشند اگر Id خیلی طولانی باشد
            // در آن صورت Id شامل فاصله نیست — پس معتبر است. اگر Id فاصله داشت، fallback
            if (p.id.contains(' ') || p.id.isEmpty()) {
                // تلاش fallback با split برای این سطر
                QStringList parts = line.trimmed().split(QRegularExpression(R"(\s{2,})"));
                if (parts.size() >= 4) {
                    // حدس: آخرین 3 ستون Version/Available/Source هستند، بقیه Name+Id
                    // ولی fixed-width قابل اعتمادتر است، پس فقط اگر واقعا خراب بود
                    p.name = parts[0];
                    p.id = parts[1];
                    p.version = parts[2];
                    p.availableVersion = parts[3];
                    if (parts.size() >= 5) p.source = parts[4];
                }
            }

            if (p.isValid() && p.id != "Id" && p.name != "Name") {
                result.append(p);
            }
        }
        return result;
    }

    // حالا برای هر خط داده، با استفاده از cols استخراج کن
    // cols[0]=Name, [1]=Id, [2]=Version, [3]=Available, [4]=Source (اگر باشد)
    for (int i = dashIdx + 1; i < lines.size(); ++i) {
        QString line = lines[i];
        if (line.trimmed().isEmpty()) continue;
        if (line.contains("---")) continue;
        // خط خلاصه انتهایی
        if (line.contains("upgrades available", Qt::CaseInsensitive)) break;
        if (line.contains("package(s) have version", Qt::CaseInsensitive)) continue;

        // اگر خط خیلی کوتاه باشد نادیده بگیر
        if (line.trimmed().length() < 10) continue;

        // استخراج بر اساس موقعیت ستون‌ها
        // برای اینکه خط کوتاه‌تر از header باشد، با space پد کن
        QString padded = line;
        if (padded.length() < dashLine.length()) {
            padded = padded.leftJustified(dashLine.length(), ' ');
        }

        auto extractCol = [&](int idx) -> QString {
            if (idx >= cols.size()) return {};
            int start = cols[idx].start;
            int end = (idx + 1 < cols.size()) ? cols[idx+1].start : padded.length();
            // کمی tolerance: ستون‌ها با حداقل 2 space جدا می‌شوند
            // end را منهای 1 کن تا spaceها حذف شوند
            if (start >= padded.length()) return {};
            if (end > padded.length()) end = padded.length();
            QString seg = padded.mid(start, end - start);
            return seg.trimmed();
        };

        PackageInfo p;
        p.name = extractCol(0);
        p.id = extractCol(1);
        p.version = extractCol(2);
        p.availableVersion = extractCol(3);
        if (cols.size() >= 5) p.source = extractCol(4);
        else p.source = QStringLiteral("winget");

        // اعتبارسنجی: Id نباید فاصله داشته باشد و باید نقطه داشته باشد یا حروف
        // بعضی خطاها باعث می‌شود Name خالی شود - در آن صورت line را با split دوباره امتحان کن
        if (p.id.isEmpty() || p.availableVersion.isEmpty()) {
            // fallback برای این سطر خاص
            QStringList parts = line.trimmed().split(QRegularExpression(R"(\s{2,})"));
            if (parts.size() >= 4) {
                p.name = parts[0];
                p.id = parts[1];
                p.version = parts[2];
                p.availableVersion = parts[3];
                if (parts.size() >= 5) p.source = parts[4];
            }
        }

        // فقط رکوردهای معتبر
        if (!p.id.isEmpty() && !p.availableVersion.isEmpty()) {
            // source اگر خالی بود winget بگذار
            if (p.source.isEmpty()) p.source = QStringLiteral("winget");
            // جلوگیری از header تکراری
            if (p.id == "Id" || p.name == "Name") continue;
            result.append(p);
        }
    }

    return result;
}

QVector<PackageInfo> WingetManager::parseWingetListUpgradeAvailable(const QString &output) {
    return parseWingetUpgradeOutput(output);
}

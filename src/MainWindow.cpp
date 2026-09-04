#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QToolButton>
#include <QTimer>
#include <QIcon>

// Helper: ساخت دکمه مدرن
static QPushButton* makeButton(const QString &text, const QString &objectName, QWidget *parent = nullptr) {
    auto *b = new QPushButton(text, parent);
    b->setObjectName(objectName);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(36);
    return b;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Veyo Updater"));
    setWindowIcon(QIcon(":/icons/veyo-logo.png"));
    resize(1080, 720);
    setMinimumSize(900, 600);

    m_manager = new WingetManager(this);

    setupUi();
    applyModernStyle();
    setupConnections();

    // نمایش نسخه winget
    QString ver = m_manager->wingetVersion();
    if (!ver.isEmpty()) m_wingetVersionLabel->setText(QStringLiteral("winget %1").arg(ver));
    else m_wingetVersionLabel->setText(QStringLiteral("winget پیدا نشد!"));

    // لود اولیه
    QTimer::singleShot(300, this, &MainWindow::onRefreshClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    auto *rootLayout = new QVBoxLayout(m_central);
    rootLayout->setContentsMargins(20, 16, 20, 16);
    rootLayout->setSpacing(14);

    // ===== Header =====
    auto *header = new QWidget(m_central);
    header->setObjectName("headerCard");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 14, 18, 14);
    headerLayout->setSpacing(14);

    // لوگو + عنوان
    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);

    auto *iconLabel = new QLabel(header);
    iconLabel->setFixedSize(36, 36);
    iconLabel->setStyleSheet(R"(
        background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #6366f1, stop:1 #8b5cf6);
        border-radius: 10px;
        color: white;
        font-size: 20px;
        font-weight: 900;
    )");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText(QStringLiteral("◈"));

    auto *titleLabel = new QLabel(QStringLiteral("Veyo Updater"), header);
    titleLabel->setObjectName("titleLabel");
    auto *subLabel = new QLabel(QStringLiteral("مدیریت و آپدیت خودکار برنامه‌ها با winget — سبک، سریع، مدرن"), header);
    subLabel->setObjectName("subLabel");

    titleRow->addWidget(iconLabel);
    titleRow->addWidget(titleLabel);
    auto *badge = new QLabel(QStringLiteral("v1.0 • Qt6"), header);
    badge->setObjectName("badge");
    titleRow->addWidget(badge);
    titleRow->addStretch();

    titleCol->addLayout(titleRow);
    titleCol->addWidget(subLabel);

    headerLayout->addLayout(titleCol, 1);

    // سمت راست هدر: شمارنده + نسخه winget
    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(6);
    rightCol->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_countLabel = new QLabel(QStringLiteral("—"), header);
    m_countLabel->setObjectName("countLabel");
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_wingetVersionLabel = new QLabel(QStringLiteral("winget ..."), header);
    m_wingetVersionLabel->setObjectName("wingetLabel");
    m_wingetVersionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rightCol->addWidget(m_countLabel);
    rightCol->addWidget(m_wingetVersionLabel);
    headerLayout->addLayout(rightCol);

    rootLayout->addWidget(header);

    // ===== Toolbar =====
    auto *toolbar = new QWidget(m_central);
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(10);

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("🔍  جستجو در نام یا شناسه پکیج..."));
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMinimumHeight(38);
    m_searchEdit->setClearButtonEnabled(true);

    m_selectAllCheck = new QCheckBox(QStringLiteral("انتخاب همه"), toolbar);
    m_selectAllCheck->setObjectName("selectAllCheck");
    m_selectAllCheck->setChecked(true);
    m_selectAllCheck->setCursor(Qt::PointingHandCursor);

    m_refreshBtn = makeButton(QStringLiteral("↻  بررسی آپدیت‌ها"), "secondaryBtn", toolbar);
    m_upgradeSelectedBtn = makeButton(QStringLiteral("⬆  آپدیت انتخاب‌شده‌ها"), "secondaryBtn", toolbar);
    m_upgradeAllBtn = makeButton(QStringLiteral("⚡  آپدیت همه"), "primaryBtn", toolbar);

    toolLayout->addWidget(m_searchEdit, 1);
    toolLayout->addWidget(m_selectAllCheck);
    toolLayout->addWidget(m_refreshBtn);
    toolLayout->addWidget(m_upgradeSelectedBtn);
    toolLayout->addWidget(m_upgradeAllBtn);

    rootLayout->addWidget(toolbar);

    // ===== Progress =====
    m_progressBar = new QProgressBar(m_central);
    m_progressBar->setObjectName("progressBar");
    m_progressBar->setRange(0, 0); // busy
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(3);
    m_progressBar->setVisible(false);
    rootLayout->addWidget(m_progressBar);

    // ===== Table Container (card) =====
    auto *tableCard = new QWidget(m_central);
    tableCard->setObjectName("tableCard");
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);

    // هدر جدول: وضعیت
    auto *tableHeader = new QWidget(tableCard);
    tableHeader->setObjectName("tableHeader");
    auto *thLayout = new QHBoxLayout(tableHeader);
    thLayout->setContentsMargins(14, 10, 14, 10);
    m_statusLabel = new QLabel(QStringLiteral("آماده"), tableHeader);
    m_statusLabel->setObjectName("statusLabel");
    auto *hint = new QLabel(QStringLiteral("💡 نکته: آپدیت‌ها مستقیما از winget خوانده می‌شوند — بدون نیاز به سرویس اضافی"), tableHeader);
    hint->setObjectName("hintLabel");
    thLayout->addWidget(m_statusLabel);
    thLayout->addStretch();
    thLayout->addWidget(hint);
    tableLayout->addWidget(tableHeader);

    m_table = new QTableWidget(0, 6, tableCard);
    m_table->setObjectName("mainTable");
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setMouseTracking(true);
    m_table->verticalHeader()->setDefaultSectionSize(54);
    m_table->setSortingEnabled(true);

    QStringList headers = {QStringLiteral(""), QStringLiteral("نام برنامه"), QStringLiteral("شناسه"), QStringLiteral("نسخه فعلی"), QStringLiteral("نسخه جدید"), QStringLiteral("عملیات")};
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 44);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->setColumnWidth(2, 260);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 120);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(4, 120);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_table->setColumnWidth(5, 130);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setHighlightSections(false);

    // Empty state overlay
    m_emptyWidget = new QWidget(m_table);
    m_emptyWidget->setObjectName("emptyWidget");
    m_emptyWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    auto *emptyLayout = new QVBoxLayout(m_emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    m_emptyLabel = new QLabel(m_emptyWidget);
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    emptyLayout->addWidget(m_emptyLabel);
    m_emptyWidget->hide();

    tableLayout->addWidget(m_table, 1);
    rootLayout->addWidget(tableCard, 1);

    // ===== Log panel =====
    auto *logHeader = new QWidget(m_central);
    auto *logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(0, 0, 0, 0);
    auto *logTitle = new QLabel(QStringLiteral("📋  گزارش (Log)"), logHeader);
    logTitle->setObjectName("logTitle");
    m_toggleLogBtn = new QPushButton(QStringLiteral("— پنهان"), logHeader);
    m_toggleLogBtn->setObjectName("ghostBtn");
    m_toggleLogBtn->setCursor(Qt::PointingHandCursor);
    m_toggleLogBtn->setFixedHeight(26);
    m_clearLogBtn = new QPushButton(QStringLiteral("پاک کردن"), logHeader);
    m_clearLogBtn->setObjectName("ghostBtn");
    m_clearLogBtn->setCursor(Qt::PointingHandCursor);
    m_clearLogBtn->setFixedHeight(26);
    logHeaderLayout->addWidget(logTitle);
    logHeaderLayout->addStretch();
    logHeaderLayout->addWidget(m_clearLogBtn);
    logHeaderLayout->addWidget(m_toggleLogBtn);
    rootLayout->addWidget(logHeader);

    m_logView = new QPlainTextEdit(m_central);
    m_logView->setObjectName("logView");
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(2000);
    m_logView->setFixedHeight(140);
    m_logView->setPlaceholderText(QStringLiteral("لاگ اجرای winget اینجا نمایش داده می‌شود..."));
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
    rootLayout->addWidget(m_logView);
}

void MainWindow::setupConnections() {
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_upgradeAllBtn, &QPushButton::clicked, this, &MainWindow::onUpgradeAllClicked);
    connect(m_upgradeSelectedBtn, &QPushButton::clicked, this, &MainWindow::onUpgradeSelectedClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_clearLogBtn, &QPushButton::clicked, m_logView, &QPlainTextEdit::clear);
    connect(m_toggleLogBtn, &QPushButton::clicked, this, [this]() {
        m_logVisible = !m_logVisible;
        m_logView->setVisible(m_logVisible);
        m_toggleLogBtn->setText(m_logVisible ? QStringLiteral("— پنهان") : QStringLiteral("+ نمایش"));
    });
    connect(m_selectAllCheck, &QCheckBox::toggled, this, [this](bool checked) {
        // بدون ایجاد سیگنال‌های اضافی
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (m_table->isRowHidden(r)) continue;
            auto *w = qobject_cast<QCheckBox*>(m_table->cellWidget(r, 0));
            if (w) {
                w->blockSignals(true);
                w->setChecked(checked);
                w->blockSignals(false);
            }
            if (checked) m_selectedRows.insert(r);
            else m_selectedRows.remove(r);
        }
        updateStats();
    });

    connect(m_manager, &WingetManager::checkStarted, this, [this]() {
        setLoading(true);
        m_statusLabel->setText(QStringLiteral("⏳ در حال بررسی آپدیت‌ها..."));
        log(QStringLiteral("▶ winget upgrade  — بررسی شروع شد"), "#7dd3fc");
    });
    connect(m_manager, &WingetManager::checkFinished, this, &MainWindow::onCheckFinished);
    connect(m_manager, &WingetManager::checkError, this, &MainWindow::onCheckError);
    connect(m_manager, &WingetManager::upgradeStarted, this, &MainWindow::onUpgradeStarted);
    connect(m_manager, &WingetManager::upgradeProgress, this, &MainWindow::onUpgradeProgress);
    connect(m_manager, &WingetManager::upgradeFinished, this, &MainWindow::onUpgradeFinished);

    // resize empty overlay
    m_table->installEventFilter(this);
}

void MainWindow::applyModernStyle() {
    // تم تیره مدرن، الهام از Fluent + Linear
    QString qss = R"(
        QMainWindow { background: #0f1115; }
        QWidget#headerCard {
            background: #181b20;
            border: 1px solid #23272f;
            border-radius: 14px;
        }
        QLabel#titleLabel {
            color: #f8fafc;
            font-size: 18px;
            font-weight: 800;
            letter-spacing: -0.3px;
        }
        QLabel#subLabel {
            color: #94a3b8;
            font-size: 11px;
        }
        QLabel#badge {
            background: #23272f;
            color: #94a3b8;
            border: 1px solid #2e3440;
            border-radius: 6px;
            padding: 2px 8px;
            font-size: 10px;
            font-weight: 600;
        }
        QLabel#countLabel {
            color: #f1f5f9;
            font-size: 22px;
            font-weight: 800;
        }
        QLabel#wingetLabel {
            color: #64748b;
            font-size: 11px;
            font-family: "Cascadia Code", "Consolas", monospace;
        }
        QLineEdit#searchEdit {
            background: #181b20;
            border: 1px solid #23272f;
            border-radius: 10px;
            padding: 0 14px;
            color: #e2e8f0;
            font-size: 13px;
            selection-background-color: #6366f1;
        }
        QLineEdit#searchEdit:focus {
            border: 1px solid #6366f1;
            background: #1e2128;
        }
        QLineEdit#searchEdit::placeholder { color: #64748b; }
        QCheckBox#selectAllCheck {
            color: #cbd5e1;
            font-size: 12px;
            font-weight: 600;
            spacing: 6px;
        }
        QCheckBox#selectAllCheck::indicator {
            width: 18px; height: 18px;
            border-radius: 5px;
            border: 1px solid #334155;
            background: #181b20;
        }
        QCheckBox#selectAllCheck::indicator:checked {
            background: #6366f1;
            border-color: #6366f1;
            image: url(:/icons/check.svg);
        }
        QPushButton#primaryBtn {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #6366f1, stop:1 #8b5cf6);
            color: white;
            border: none;
            border-radius: 10px;
            padding: 0 18px;
            font-size: 13px;
            font-weight: 700;
        }
        QPushButton#primaryBtn:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5457e5, stop:1 #7c3aed); }
        QPushButton#primaryBtn:pressed { background: #4f46e5; }
        QPushButton#primaryBtn:disabled { background: #23272f; color: #475569; }

        QPushButton#secondaryBtn {
            background: #181b20;
            color: #e2e8f0;
            border: 1px solid #23272f;
            border-radius: 10px;
            padding: 0 16px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#secondaryBtn:hover { background: #23272f; border-color: #334155; }
        QPushButton#secondaryBtn:pressed { background: #1e293b; }
        QPushButton#secondaryBtn:disabled { color: #475569; border-color: #1e293b; }

        QPushButton#ghostBtn {
            background: transparent;
            color: #64748b;
            border: 1px solid #23272f;
            border-radius: 6px;
            padding: 0 10px;
            font-size: 11px;
        }
        QPushButton#ghostBtn:hover { color: #e2e8f0; border-color: #334155; background: #181b20; }

        QWidget#tableCard {
            background: #181b20;
            border: 1px solid #23272f;
            border-radius: 14px;
        }
        QWidget#tableHeader {
            background: #1e2128;
            border-bottom: 1px solid #23272f;
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
        }
        QLabel#statusLabel { color: #e2e8f0; font-size: 12px; font-weight: 700; }
        QLabel#hintLabel { color: #64748b; font-size: 11px; }

        QTableWidget#mainTable {
            background: transparent;
            border: none;
            gridline-color: transparent;
            color: #e2e8f0;
            font-size: 12.5px;
            outline: 0;
        }
        QHeaderView::section {
            background: #181b20;
            color: #94a3b8;
            padding: 8px 10px;
            border: none;
            border-bottom: 1px solid #23272f;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.4px;
        }
        QTableWidget::item {
            padding: 6px 8px;
            border-bottom: 1px solid #1e2128;
        }
        QTableWidget::item:selected {
            background: #1e2128;
            color: #f8fafc;
        }
        QTableWidget::item:hover { background: #1a1d23; }

        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #2e3440;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: #334155; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { height: 0; }

        QProgressBar#progressBar {
            background: #23272f;
            border: none;
            border-radius: 2px;
        }
        QProgressBar#progressBar::chunk {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #6366f1, stop:1 #22d3ee);
            border-radius: 2px;
        }

        QLabel#logTitle { color: #94a3b8; font-size: 11px; font-weight: 700; letter-spacing: 0.3px; }
        QPlainTextEdit#logView {
            background: #0f1115;
            border: 1px solid #23272f;
            border-radius: 10px;
            color: #cbd5e1;
            font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 11px;
            padding: 8px;
            selection-background-color: #6366f1;
        }

        QWidget#emptyWidget { background: transparent; }
        QLabel#emptyLabel {
            color: #64748b;
            font-size: 13px;
            padding: 30px;
        }

        QToolTip {
            background: #1e2128;
            color: #e2e8f0;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 11px;
        }
    )";

    // استایل دکمه‌های داخل جدول
    qApp->setStyleSheet(qss);

    // استایل چک‌باکس سلول — از طریق QSS سراسری هم اعمال شده
}

void MainWindow::onRefreshClicked() {
    if (m_isLoading) return;
    m_searchEdit->clear();
    m_manager->checkForUpdates(false, false);
}

void MainWindow::onCheckFinished(const QVector<PackageInfo> &packages) {
    setLoading(false);
    m_packages = packages;
    m_selectedRows.clear();
    for (int i = 0; i < packages.size(); ++i) m_selectedRows.insert(i);

    m_selectAllCheck->blockSignals(true);
    m_selectAllCheck->setChecked(!packages.isEmpty());
    m_selectAllCheck->blockSignals(false);

    populateTable(packages);

    if (packages.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("✅ همه برنامه‌ها به‌روز هستند"));
        showEmptyState(true, QStringLiteral("🎉 هیچ آپدیتی پیدا نشد — همه چیز به‌روز است!\nاگر انتظار آپدیت دارید، «بررسی آپدیت‌ها» را دوباره بزنید یا winget را به‌روزرسانی کنید."));
        log(QStringLiteral("✔ هیچ آپدیتی یافت نشد"), "#22c55e");
    } else {
        m_statusLabel->setText(QStringLiteral("📦 %1 آپدیت در دسترس").arg(packages.size()));
        showEmptyState(false);
        log(QStringLiteral("✔ %1 آپدیت پیدا شد").arg(packages.size()), "#22c55e");
        for (const auto &p : packages) {
            log(QStringLiteral("  • %1 (%2)  %3 → %4").arg(p.name, p.id, p.version, p.availableVersion), "#e2e8f0");
        }
    }
    updateStats();
    setUpgradeButtonsEnabled(!packages.isEmpty());
}

void MainWindow::onCheckError(const QString &error) {
    setLoading(false);
    m_statusLabel->setText(QStringLiteral("❌ خطا در بررسی"));
    log(QStringLiteral("✘ خطا: %1").arg(error), "#ef4444");
    QMessageBox::warning(this, QStringLiteral("خطا"), error);
}

void MainWindow::populateTable(const QVector<PackageInfo> &packages) {
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    m_table->setRowCount(packages.size());

    for (int row = 0; row < packages.size(); ++row) {
        const auto &p = packages[row];

        // ستون 0: چک‌باکس
        auto *chk = new QCheckBox(m_table);
        chk->setChecked(true);
        chk->setCursor(Qt::PointingHandCursor);
        chk->setStyleSheet(R"(
            QCheckBox::indicator { width:18px; height:18px; border-radius:5px; border:1px solid #334155; background:#0f1115; }
            QCheckBox::indicator:checked { background:#6366f1; border-color:#6366f1; }
        )");
        // center
        auto *chkContainer = new QWidget(m_table);
        auto *chkLayout = new QHBoxLayout(chkContainer);
        chkLayout->setContentsMargins(0,0,0,0);
        chkLayout->setAlignment(Qt::AlignCenter);
        chkLayout->addWidget(chk);
        m_table->setCellWidget(row, 0, chkContainer);

        connect(chk, &QCheckBox::toggled, this, [this, row](bool c){ onItemCheckboxChanged(row, c); });

        // ستون 1: نام + (source badge کوچک در tooltip)
        auto *nameItem = new QTableWidgetItem(p.name);
        nameItem->setToolTip(p.name + QStringLiteral("\n") + p.id);
        // آیکون حرف اول
        QString initial = p.name.isEmpty() ? "?" : QString(p.name[0]).toUpper();
        nameItem->setData(Qt::UserRole, p.id); // برای فیلتر
        // استایل فونت
        QFont f = nameItem->font();
        f.setBold(true);
        nameItem->setFont(f);
        m_table->setItem(row, 1, nameItem);

        auto *idItem = new QTableWidgetItem(p.id);
        idItem->setForeground(QBrush(QColor("#94a3b8")));
        QFont mono = idItem->font();
        mono.setFamily("Consolas");
        mono.setPointSize(9);
        idItem->setFont(mono);
        m_table->setItem(row, 2, idItem);

        auto *verItem = new QTableWidgetItem(p.version);
        verItem->setTextAlignment(Qt::AlignCenter);
        verItem->setForeground(QBrush(QColor("#94a3b8")));
        m_table->setItem(row, 3, verItem);

        auto *availItem = new QTableWidgetItem(p.availableVersion);
        availItem->setTextAlignment(Qt::AlignCenter);
        availItem->setForeground(QBrush(QColor("#22c55e")));
        QFont bf = availItem->font();
        bf.setBold(true);
        availItem->setFont(bf);
        m_table->setItem(row, 4, availItem);

        // ستون 5: دکمه آپدیت تکی
        auto *btn = new QPushButton(QStringLiteral("آپدیت"), m_table);
        btn->setObjectName("rowBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(28);
        btn->setStyleSheet(R"(
            QPushButton#rowBtn { background:#23272f; color:#e2e8f0; border:1px solid #2e3440; border-radius:7px; padding:0 12px; font-size:11px; font-weight:600; }
            QPushButton#rowBtn:hover { background:#6366f1; border-color:#6366f1; color:white; }
            QPushButton#rowBtn:pressed { background:#4f46e5; }
            QPushButton#rowBtn:disabled { background:#1a1d23; color:#475569; border-color:#1e2128; }
        )");
        m_table->setCellWidget(row, 5, btn);
        connect(btn, &QPushButton::clicked, this, [this, row](){ onUpgradeSingleClicked(row); });
    }

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(1, Qt::AscendingOrder);
    // ردیف را قابل انتخاب کن
    m_table->resizeRowsToContents();
}

void MainWindow::onItemCheckboxChanged(int row, bool checked) {
    if (checked) m_selectedRows.insert(row);
    else m_selectedRows.remove(row);

    // به‌روزرسانی selectAll
    bool allChecked = true;
    for (int r=0; r<m_table->rowCount(); ++r) {
        if (m_table->isRowHidden(r)) continue;
        auto *cw = qobject_cast<QWidget*>(m_table->cellWidget(r,0));
        auto *cb = cw ? cw->findChild<QCheckBox*>() : nullptr;
        if (cb && !cb->isChecked()) { allChecked = false; break; }
    }
    m_selectAllCheck->blockSignals(true);
    m_selectAllCheck->setChecked(allChecked);
    m_selectAllCheck->blockSignals(false);
    updateStats();
}

void MainWindow::onUpgradeSingleClicked(int row) {
    if (row < 0 || row >= m_packages.size()) return;
    // به خاطر sorting، باید id را از جدول بخوانیم نه از m_packages[row]
    auto *idItem = m_table->item(row, 2);
    if (!idItem) return;
    QString id = idItem->text();
    QString name = m_table->item(row, 1) ? m_table->item(row,1)->text() : id;

    auto res = QMessageBox::question(this, QStringLiteral("آپدیت"),
        QStringLiteral("آیا می‌خواهید «%1» (%2) آپدیت شود؟").arg(name, id),
        QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    m_upgradingRow = row;
    // disable دکمه‌ها
    setUpgradeButtonsEnabled(false);
    m_table->cellWidget(row,5)->setEnabled(false);
    m_manager->upgradePackage(id, true);
}

void MainWindow::onUpgradeAllClicked() {
    if (m_packages.isEmpty()) return;
    auto res = QMessageBox::question(this, QStringLiteral("آپدیت همه"),
        QStringLiteral("آیا می‌خواهید همهٔ %1 برنامه آپدیت شوند؟\nاین کار ممکن است چند دقیقه طول بکشد.").arg(m_packages.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    setUpgradeButtonsEnabled(false);
    m_manager->upgradeAll(true);
}

void MainWindow::onUpgradeSelectedClicked() {
    auto ids = selectedIds();
    if (ids.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("انتخاب"), QStringLiteral("هیچ برنامه‌ای انتخاب نشده است."));
        return;
    }
    auto res = QMessageBox::question(this, QStringLiteral("آپدیت انتخاب‌شده‌ها"),
        QStringLiteral("آیا %1 برنامه انتخاب‌شده آپدیت شوند؟").arg(ids.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    setUpgradeButtonsEnabled(false);
    // برای سبک ماندن، از upgrade تکی پشت سر هم استفاده می‌کنیم — فعلا اولی را اجرا کن، بقیه در finished زنجیره می‌شوند
    // ساده: اگر چند تاست، به کاربر بگو از «آپدیت همه» استفاده کند یا دانه‌دانه
    if (ids.size() == 1) {
        m_manager->upgradePackage(ids.first(), true);
    } else {
        // نمایش دیالوگ: فعلا همه را با --all انجام می‌دهیم ولی فقط انتخاب‌شده‌ها logical نیست — پس تکی sequentially
        // پیاده‌سازی sequential:
        m_upgradingRow = -1; // حالت دسته‌ای
        // برای جلوگیری از پیچیدگی، از winget upgrade --all استفاده می‌کنیم و به کاربر اطلاع می‌دهیم
        // اما بهتر است تکی زنجیره‌ای انجام دهیم — ساده‌ترین: upgradeAll
        // اگر کاربر انتخاب محدود دارد ولی --all همه را می‌زند، باید تکی بزنیم
        // اینجا sequential تکی اجرا می‌کنیم: اولین را شروع کن، بقیه در onUpgradeFinished ادامه می‌یابند
        // ذخیره لیست برای زنجیره
        static QStringList queue;
        queue = ids;
        QString first = queue.takeFirst();
        // store queue in dynamic property
        setProperty("upgradeQueue", queue);
        m_manager->upgradePackage(first, true);
    }
}

QStringList MainWindow::selectedIds() const {
    QStringList ids;
    for (int r=0; r<m_table->rowCount(); ++r) {
        if (m_table->isRowHidden(r)) continue;
        auto *cw = qobject_cast<QWidget*>(m_table->cellWidget(r,0));
        auto *cb = cw ? cw->findChild<QCheckBox*>() : nullptr;
        if (cb && cb->isChecked()) {
            auto *it = m_table->item(r, 2);
            if (it) ids << it->text();
        }
    }
    return ids;
}

void MainWindow::onUpgradeStarted(const QString &id) {
    setLoading(true);
    if (id == "ALL") {
        m_statusLabel->setText(QStringLiteral("⚡ در حال آپدیت همه..."));
        log(QStringLiteral("▶ winget upgrade --all  — شروع"), "#f59e0b");
    } else {
        m_statusLabel->setText(QStringLiteral("⬆ در حال آپدیت %1...").arg(id));
        log(QStringLiteral("▶ winget upgrade --id %1  — شروع").arg(id), "#f59e0b");
    }
}

void MainWindow::onUpgradeProgress(const QString &id, const QString &line) {
    Q_UNUSED(id)
    if (line.isEmpty()) return;
    // فیلتر خطوط بی‌اهمیت
    if (line.contains("Downloading", Qt::CaseInsensitive) || line.contains("Installing", Qt::CaseInsensitive) || line.contains("%")) {
        log(QStringLiteral("  %1").arg(line), "#7dd3fc");
    } else {
        log(line, "#94a3b8");
    }
}

void MainWindow::onUpgradeFinished(const QString &id, bool success, const QString &msg) {
    setLoading(false);
    setUpgradeButtonsEnabled(true);
    // فعال‌سازی دوباره دکمه سطری
    if (m_upgradingRow >=0 && m_upgradingRow < m_table->rowCount()) {
        if (auto *w = m_table->cellWidget(m_upgradingRow,5)) w->setEnabled(true);
    }

    if (success) {
        m_statusLabel->setText(QStringLiteral("✅ آپدیت %1 موفق").arg(id == "ALL" ? QStringLiteral("همه") : id));
        log(QStringLiteral("✔ آپدیت %1 موفق").arg(id), "#22c55e");
        // بعد از آپدیت موفق، لیست را رفرش کن
        QTimer::singleShot(1200, this, &MainWindow::onRefreshClicked);
    } else {
        m_statusLabel->setText(QStringLiteral("❌ خطا در آپدیت %1").arg(id));
        log(QStringLiteral("✘ آپدیت %1 ناموفق").arg(id), "#ef4444");
        log(msg.left(2000), "#ef4444");
        QMessageBox::warning(this, QStringLiteral("خطا در آپدیت"), msg.left(3000));
    }

    // اگر صف آپدیت انتخاب‌شده‌ها وجود دارد، بعدی را اجرا کن
    QVariant v = property("upgradeQueue");
    if (v.isValid() && v.type() == QVariant::StringList) {
        QStringList queue = v.toStringList();
        if (!queue.isEmpty()) {
            QString next = queue.takeFirst();
            setProperty("upgradeQueue", queue);
            QTimer::singleShot(800, this, [this, next](){
                setUpgradeButtonsEnabled(false);
                m_manager->upgradePackage(next, true);
            });
            return;
        } else {
            setProperty("upgradeQueue", QVariant());
        }
    }

    m_upgradingRow = -1;
}

void MainWindow::onSearchTextChanged(const QString &text) {
    Q_UNUSED(text)
    filterTable();
}

void MainWindow::filterTable() {
    QString q = m_searchEdit->text().trimmed().toLower();
    int visible = 0;
    for (int r=0; r<m_table->rowCount(); ++r) {
        auto *nameIt = m_table->item(r,1);
        auto *idIt = m_table->item(r,2);
        QString hay = (nameIt ? nameIt->text() : "") + " " + (idIt ? idIt->text() : "");
        bool match = q.isEmpty() || hay.toLower().contains(q);
        m_table->setRowHidden(r, !match);
        if (match) ++visible;
    }
    if (visible==0 && m_table->rowCount()>0) {
        showEmptyState(true, QStringLiteral("🔍 نتیجه‌ای برای «%1» یافت نشد.").arg(m_searchEdit->text()));
    } else if (m_table->rowCount()==0) {
        // empty قبلی حفظ شود
    } else {
        if (m_packages.isEmpty()) {} else showEmptyState(false);
    }
    updateStats();
}

void MainWindow::updateStats() {
    int total = m_table->rowCount();
    int visible = 0;
    int selected = 0;
    for (int r=0; r<total; ++r) {
        if (!m_table->isRowHidden(r)) {
            ++visible;
            auto *cw = qobject_cast<QWidget*>(m_table->cellWidget(r,0));
            auto *cb = cw ? cw->findChild<QCheckBox*>() : nullptr;
            if (cb && cb->isChecked()) ++selected;
        }
    }
    if (total==0) {
        m_countLabel->setText(QStringLiteral("—"));
    } else {
        m_countLabel->setText(QStringLiteral("%1 آپدیت  •  %2 انتخاب‌شده").arg(visible).arg(selected));
    }
}

void MainWindow::setLoading(bool loading) {
    m_isLoading = loading;
    m_progressBar->setVisible(loading);
    m_refreshBtn->setEnabled(!loading);
    if (loading) m_refreshBtn->setText(QStringLiteral("⏳ در حال بررسی..."));
    else m_refreshBtn->setText(QStringLiteral("↻  بررسی آپدیت‌ها"));
}

void MainWindow::setUpgradeButtonsEnabled(bool enabled) {
    bool hasData = m_table->rowCount() > 0;
    m_upgradeAllBtn->setEnabled(enabled && hasData && !m_isLoading);
    m_upgradeSelectedBtn->setEnabled(enabled && hasData && !m_isLoading);
    // دکمه‌های سطری
    for (int r=0; r<m_table->rowCount(); ++r) {
        if (auto *w = m_table->cellWidget(r,5)) w->setEnabled(enabled);
    }
}

void MainWindow::log(const QString &msg, const QString &color) {
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QStringLiteral("<span style='color:%1'>[%2] %3</span>").arg(color, ts, msg.toHtmlEscaped());
    m_logView->appendHtml(html);
    // اسکرول به پایین
    auto *sb = m_logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::showEmptyState(bool show, const QString &msg) {
    if (show) {
        m_emptyLabel->setText(msg.isEmpty() ? QStringLiteral("داده‌ای نیست") : msg);
        m_emptyWidget->setGeometry(m_table->viewport()->geometry());
        m_emptyWidget->raise();
        m_emptyWidget->show();
    } else {
        m_emptyWidget->hide();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_table && event->type() == QEvent::Resize) {
        if (m_emptyWidget && m_emptyWidget->isVisible()) {
            m_emptyWidget->setGeometry(m_table->viewport()->geometry());
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_isLoading) {
        auto r = QMessageBox::question(this, QStringLiteral("خروج"),
            QStringLiteral("عملیاتی در حال اجراست. آیا می‌خواهید خارج شوید؟"),
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) { event->ignore(); return; }
        m_manager->cancelCurrentOperation();
    }
    event->accept();
}

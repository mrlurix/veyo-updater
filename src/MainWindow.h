#pragma once
#include <QMainWindow>
#include <QVector>
#include <QSet>
#include "WingetManager.h"

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QCheckBox;
class QPlainTextEdit;
class QTableWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onRefreshClicked();
    void onUpgradeAllClicked();
    void onUpgradeSelectedClicked();
    void onSearchTextChanged(const QString &text);
    void onCheckFinished(const QVector<PackageInfo> &packages);
    void onCheckError(const QString &error);
    void onUpgradeStarted(const QString &id);
    void onUpgradeProgress(const QString &id, const QString &line);
    void onUpgradeFinished(const QString &id, bool success, const QString &msg);
    void onItemCheckboxChanged(int row, bool checked);
    void onUpgradeSingleClicked(int row);
    void updateStats();
    void filterTable();

private:
    void setupUi();
    void setupConnections();
    void applyModernStyle();
    void populateTable(const QVector<PackageInfo> &packages);
    void setLoading(bool loading);
    void log(const QString &msg, const QString &color = "#a0aec0");
    void setUpgradeButtonsEnabled(bool enabled);
    QStringList selectedIds() const;
    void showEmptyState(bool show, const QString &msg = {});

    // UI elements
    QWidget *m_central = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_upgradeAllBtn = nullptr;
    QPushButton *m_upgradeSelectedBtn = nullptr;
    QCheckBox *m_selectAllCheck = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_wingetVersionLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton *m_clearLogBtn = nullptr;
    QPushButton *m_toggleLogBtn = nullptr;
    QWidget *m_emptyWidget = nullptr;
    QLabel *m_emptyLabel = nullptr;

    WingetManager *m_manager = nullptr;
    QVector<PackageInfo> m_packages;
    QSet<int> m_selectedRows;
    bool m_isLoading = false;
    bool m_logVisible = true;
    int m_upgradingRow = -1;
};

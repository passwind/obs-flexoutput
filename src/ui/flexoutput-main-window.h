#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QCloseEvent>

#include "../flexoutput-types.h"

// Forward declarations
class FlexOutputOutputWidget;
class FlexOutputMappingWidget;
class FlexOutputSourceListWidget;
class FlexOutputStatusWidget;

// Main UI window class
class FlexOutputMainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputMainWindow(QWidget *parent = nullptr);
    ~FlexOutputMainWindow();

    void updateUI();
    void refreshOutputs();
    void refreshSources();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddOutputClicked();
    void onRemoveOutputClicked();
    void onStartOutputClicked();
    void onStopOutputClicked();
    void onConfigureOutputClicked();
    void onOutputSelectionChanged();
    void onRefreshClicked();
    void onSaveConfigClicked();
    void onLoadConfigClicked();
    void onExportConfigClicked();
    void onImportConfigClicked();
    void onAboutClicked();
    void onUpdateTimer();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();
    void updateStatus();
    
    // UI components
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    QMenuBar *m_menuBar;
    QStatusBar *m_statusBar;
    
    // Tabs
    FlexOutputOutputWidget *m_outputWidget;
    FlexOutputMappingWidget *m_mappingWidget;
    FlexOutputSourceListWidget *m_sourceWidget;
    FlexOutputStatusWidget *m_statusWidget;
    
    // Menus
    QMenu *m_fileMenu;
    QMenu *m_editMenu;
    QMenu *m_viewMenu;
    QMenu *m_helpMenu;
    
    // Status bar
    QLabel *m_statusLabel;
    QLabel *m_outputCountLabel;
    QLabel *m_sourceCountLabel;
    
    // Timer for updates
    QTimer *m_updateTimer;
    
    // Current state
    QString m_currentOutputName;
    bool m_isClosing;
};

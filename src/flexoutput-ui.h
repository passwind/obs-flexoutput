#pragma once

#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTimer>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QCloseEvent>

#include "flexoutput-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// UI initialization and cleanup
void flexoutput_ui_init(void);
void flexoutput_ui_cleanup(void);

// Show/hide main UI
void flexoutput_ui_show(void);
void flexoutput_ui_hide(void);

#ifdef __cplusplus
}
#endif

// Forward declarations
class FlexOutputMainWindow;
class FlexOutputConfigDialog;
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
    void updateOutputList();
    void updateSourceList();
    void updateMappingRules();
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

// Output management widget
class FlexOutputOutputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputOutputWidget(QWidget *parent = nullptr);
    ~FlexOutputOutputWidget();

    void updateOutputList();
    void setCurrentOutput(const QString &outputName);
    QString getCurrentOutput() const;

signals:
    void outputSelectionChanged(const QString &outputName);
    void addOutputRequested();
    void removeOutputRequested(const QString &outputName);
    void startOutputRequested(const QString &outputName);
    void stopOutputRequested(const QString &outputName);
    void configureOutputRequested(const QString &outputName);

private slots:
    void onOutputItemChanged();
    void onOutputDoubleClicked();
    void onContextMenuRequested(const QPoint &pos);

private:
    void setupUI();
    void updateOutputStatus();
    
    QVBoxLayout *m_layout;
    QHBoxLayout *m_buttonLayout;
    QTableWidget *m_outputTable;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_configureButton;
    QPushButton *m_refreshButton;
    
    QString m_currentOutput;
};

// Mapping rules widget
class FlexOutputMappingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputMappingWidget(QWidget *parent = nullptr);
    ~FlexOutputMappingWidget();

    void setCurrentOutput(const QString &outputName);
    void updateMappingRules();

signals:
    void mappingRulesChanged();

private slots:
    void onMappingTypeChanged();
    void onAddSourceClicked();
    void onRemoveSourceClicked();
    void onClearSourcesClicked();
    void onSourceItemChanged();
    void onAvailableSourceDoubleClicked();
    void onMappedSourceDoubleClicked();

private:
    void setupUI();
    void updateAvailableSources();
    void updateMappedSources();
    void applyMappingChanges();
    
    QVBoxLayout *m_layout;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_middleLayout;
    QHBoxLayout *m_bottomLayout;
    
    QLabel *m_outputLabel;
    QComboBox *m_mappingTypeCombo;
    QCheckBox *m_enabledCheckBox;
    
    QSplitter *m_splitter;
    QGroupBox *m_availableGroup;
    QGroupBox *m_mappedGroup;
    
    QListWidget *m_availableSourcesList;
    QListWidget *m_mappedSourcesList;
    
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_clearButton;
    QPushButton *m_addAllButton;
    QPushButton *m_removeAllButton;
    
    QString m_currentOutput;
    flexoutput_mapping_rule_t *m_currentRule;
};

// Source list widget
class FlexOutputSourceListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputSourceListWidget(QWidget *parent = nullptr);
    ~FlexOutputSourceListWidget();

    void updateSourceList();

private slots:
    void onSourceItemChanged();
    void onSourceTypeFilterChanged();
    void onRefreshClicked();

private:
    void setupUI();
    void filterSources();
    
    QVBoxLayout *m_layout;
    QHBoxLayout *m_filterLayout;
    QHBoxLayout *m_buttonLayout;
    
    QLabel *m_filterLabel;
    QComboBox *m_sourceTypeFilter;
    QPushButton *m_refreshButton;
    
    QTableWidget *m_sourceTable;
};

// Status monitoring widget
class FlexOutputStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputStatusWidget(QWidget *parent = nullptr);
    ~FlexOutputStatusWidget();

    void updateStatus();

private slots:
    void onClearLogClicked();
    void onSaveLogClicked();

private:
    void setupUI();
    void addLogEntry(const QString &level, const QString &message);
    
    QVBoxLayout *m_layout;
    QGridLayout *m_statsLayout;
    QHBoxLayout *m_logButtonLayout;
    
    QGroupBox *m_statsGroup;
    QGroupBox *m_logGroup;
    
    // Statistics
    QLabel *m_totalOutputsLabel;
    QLabel *m_activeOutputsLabel;
    QLabel *m_totalSourcesLabel;
    QLabel *m_mappedSourcesLabel;
    QLabel *m_uptimeLabel;
    QLabel *m_memoryUsageLabel;
    
    // Log
    QPlainTextEdit *m_logTextEdit;
    QPushButton *m_clearLogButton;
    QPushButton *m_saveLogButton;
};

// Configuration dialog
class FlexOutputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FlexOutputConfigDialog(const QString &outputName, QWidget *parent = nullptr);
    ~FlexOutputConfigDialog();

    flexoutput_output_config_t getConfiguration() const;
    void setConfiguration(const flexoutput_output_config_t &config);

private slots:
    void onOutputTypeChanged();
    void onTestConnectionClicked();
    void onResetToDefaultsClicked();
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void setupOutputSettings();
    void setupVideoSettings();
    void setupAudioSettings();
    void setupAdvancedSettings();
    void updateUI();
    void validateSettings();
    
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    QHBoxLayout *m_buttonLayout;
    
    // Output settings
    QWidget *m_outputTab;
    QGridLayout *m_outputLayout;
    QLineEdit *m_nameEdit;
    QComboBox *m_typeCombo;
    QLineEdit *m_serverEdit;
    QLineEdit *m_keyEdit;
    QCheckBox *m_enabledCheckBox;
    QPushButton *m_testButton;
    
    // Video settings
    QWidget *m_videoTab;
    QGridLayout *m_videoLayout;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;
    QSpinBox *m_fpsSpin;
    QSpinBox *m_bitrateSpin;
    QComboBox *m_encoderCombo;
    QComboBox *m_presetCombo;
    QComboBox *m_profileCombo;
    
    // Audio settings
    QWidget *m_audioTab;
    QGridLayout *m_audioLayout;
    QSpinBox *m_sampleRateSpin;
    QSpinBox *m_audioBitrateSpin;
    QComboBox *m_audioEncoderCombo;
    QComboBox *m_channelsCombo;
    
    // Advanced settings
    QWidget *m_advancedTab;
    QGridLayout *m_advancedLayout;
    QSpinBox *m_keyframeSpin;
    QCheckBox *m_cbr_CheckBox;
    QLineEdit *m_customSettingsEdit;
    
    // Buttons
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QPushButton *m_resetButton;
    
    QString m_outputName;
    flexoutput_output_config_t m_config;
    bool m_isValid;
};

// Global UI instance
extern FlexOutputMainWindow *g_flexoutput_main_window;

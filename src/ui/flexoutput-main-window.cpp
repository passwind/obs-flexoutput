#include "flexoutput-main-window.h"
#include "flexoutput-output-widget.h"
#include "flexoutput-mapping-widget.h"
#include "flexoutput-source-list-widget.h"
#include "flexoutput-status-widget.h"
#include "flexoutput-config-dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QCloseEvent>
#include <QIcon>
#include <QKeySequence>

extern "C" {
#include "../flexoutput-types.h"
#include "../flexoutput-config.h"
#include "../flexoutput-output.h"
#include "../flexoutput-source.h"
}

#include "plugin-support.h"

#include "moc_flexoutput-main-window.cpp"

FlexOutputMainWindow::FlexOutputMainWindow(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
    , m_outputWidget(nullptr)
    , m_mappingWidget(nullptr)
    , m_sourceWidget(nullptr)
    , m_statusWidget(nullptr)
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_helpMenu(nullptr)
    , m_statusLabel(nullptr)
    , m_outputCountLabel(nullptr)
    , m_sourceCountLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_isClosing(false)
{
    setWindowTitle("FlexOutput");
    setObjectName("FlexOutputDock");
    setWindowIcon(QIcon(":/icons/flexoutput.png"));
    
    // Set appropriate size for dock widget
    setMinimumSize(300, 200);
    resize(400, 300);
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupConnections();
    
    // Start update timer
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &FlexOutputMainWindow::onUpdateTimer);
    m_updateTimer->start(1000); // Update every second
    
    updateUI();
}

FlexOutputMainWindow::~FlexOutputMainWindow()
{
    m_isClosing = true;
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

void FlexOutputMainWindow::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    
    // Create tabs
    m_outputWidget = new FlexOutputOutputWidget(this);
    m_mappingWidget = new FlexOutputMappingWidget(this);
    m_sourceWidget = new FlexOutputSourceListWidget(this);
    m_statusWidget = new FlexOutputStatusWidget(this);
    
    // Add tabs
    m_tabWidget->addTab(m_outputWidget, "Outputs");
    m_tabWidget->addTab(m_mappingWidget, "Mapping Rules");
    m_tabWidget->addTab(m_sourceWidget, "Sources");
    m_tabWidget->addTab(m_statusWidget, "Status");
    
    m_mainLayout->addWidget(m_tabWidget);
}

void FlexOutputMainWindow::setupMenuBar()
{
    m_menuBar = new QMenuBar(this);
    m_mainLayout->setMenuBar(m_menuBar);
    
    // File menu
    m_fileMenu = m_menuBar->addMenu("&File");
    m_fileMenu->addAction("&Save Configuration", QKeySequence::Save, this, &FlexOutputMainWindow::onSaveConfigClicked);
    m_fileMenu->addAction("&Load Configuration", QKeySequence::Open, this, &FlexOutputMainWindow::onLoadConfigClicked);
    m_fileMenu->addSeparator();
    QAction *exportAction = m_fileMenu->addAction("&Export Configuration...");
    connect(exportAction, &QAction::triggered, this, &FlexOutputMainWindow::onExportConfigClicked);
    QAction *importAction = m_fileMenu->addAction("&Import Configuration...");
    connect(importAction, &QAction::triggered, this, &FlexOutputMainWindow::onImportConfigClicked);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);
    
    // Edit menu
    m_editMenu = m_menuBar->addMenu("&Edit");
    m_editMenu->addAction("&Add Output", QKeySequence("Ctrl+N"), this, &FlexOutputMainWindow::onAddOutputClicked);
    m_editMenu->addAction("&Remove Output", QKeySequence::Delete, this, &FlexOutputMainWindow::onRemoveOutputClicked);
    m_editMenu->addSeparator();
    m_editMenu->addAction("&Configure Output", QKeySequence("Ctrl+E"), this, &FlexOutputMainWindow::onConfigureOutputClicked);
    
    // View menu
    m_viewMenu = m_menuBar->addMenu("&View");
    m_viewMenu->addAction("&Refresh", QKeySequence::Refresh, this, &FlexOutputMainWindow::onRefreshClicked);
    
    // Help menu
    m_helpMenu = m_menuBar->addMenu("&Help");
    QAction *aboutAction = m_helpMenu->addAction("&About FlexOutput");
    connect(aboutAction, &QAction::triggered, this, &FlexOutputMainWindow::onAboutClicked);
}

void FlexOutputMainWindow::setupStatusBar()
{
    m_statusBar = new QStatusBar(this);
    m_mainLayout->addWidget(m_statusBar);
    
    m_statusLabel = new QLabel("Ready", this);
    m_outputCountLabel = new QLabel("Outputs: 0", this);
    m_sourceCountLabel = new QLabel("Sources: 0", this);
    
    m_statusBar->addWidget(m_statusLabel, 1);
    m_statusBar->addPermanentWidget(m_outputCountLabel);
    m_statusBar->addPermanentWidget(m_sourceCountLabel);
}

void FlexOutputMainWindow::setupConnections()
{
    // Connect output widget signals
    connect(m_outputWidget, &FlexOutputOutputWidget::outputSelectionChanged,
            this, &FlexOutputMainWindow::onOutputSelectionChanged);
    connect(m_outputWidget, &FlexOutputOutputWidget::addOutputRequested,
            this, &FlexOutputMainWindow::onAddOutputClicked);
    connect(m_outputWidget, &FlexOutputOutputWidget::removeOutputRequested,
            this, &FlexOutputMainWindow::onRemoveOutputClicked);
    connect(m_outputWidget, &FlexOutputOutputWidget::startOutputRequested,
            this, &FlexOutputMainWindow::onStartOutputClicked);
    connect(m_outputWidget, &FlexOutputOutputWidget::stopOutputRequested,
            this, &FlexOutputMainWindow::onStopOutputClicked);
    connect(m_outputWidget, &FlexOutputOutputWidget::configureOutputRequested,
            this, &FlexOutputMainWindow::onConfigureOutputClicked);
    
    // Connect mapping widget signals
    connect(m_mappingWidget, &FlexOutputMappingWidget::mappingRulesChanged,
            this, &FlexOutputMainWindow::updateUI);
}

void FlexOutputMainWindow::updateUI()
{
    if (m_isClosing) {
        return;
    }
    
    refreshOutputs();
    refreshSources();
    updateStatus();
}

void FlexOutputMainWindow::refreshOutputs()
{
    if (m_outputWidget) {
        m_outputWidget->updateOutputList();
    }
}

void FlexOutputMainWindow::refreshSources()
{
    if (m_sourceWidget) {
        m_sourceWidget->updateSourceList();
    }
}

void FlexOutputMainWindow::updateStatus()
{
    if (m_statusWidget) {
        m_statusWidget->updateStatus();
    }
    
    // Update status bar
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (plugin) {
        size_t output_count = plugin->outputs.num;
        
        char **sources = nullptr;
        size_t source_count = 0;
        flexoutput_source_enumerate_all(&sources, &source_count);
        
        m_outputCountLabel->setText(QString("Outputs: %1").arg(output_count));
        m_sourceCountLabel->setText(QString("Sources: %1").arg(source_count));
        
        if (sources) {
            flexoutput_source_free_string_array(sources, source_count);
        }
    }
}

void FlexOutputMainWindow::closeEvent(QCloseEvent *event)
{
    m_isClosing = true;
    event->accept();
}

void FlexOutputMainWindow::onAddOutputClicked()
{
    FlexOutputConfigDialog dialog("", this);
    if (dialog.exec() == QDialog::Accepted) {
        flexoutput_output_config_t config = dialog.getConfiguration();
        flexoutput_error_t result = flexoutput_create_output(config.name, &config);
        
        if (result == FLEXOUTPUT_SUCCESS) {
            m_statusLabel->setText(QString("Output '%1' created successfully").arg(config.name));
            updateUI();
        } else {
            QMessageBox::warning(this, "Error", 
                QString("Failed to create output '%1': Error %2").arg(config.name).arg(result));
        }
    }
}

void FlexOutputMainWindow::onRemoveOutputClicked()
{
    QString outputName = m_outputWidget->getCurrentOutput();
    if (outputName.isEmpty()) {
        QMessageBox::information(this, "Information", "Please select an output to remove.");
        return;
    }
    
    int ret = QMessageBox::question(this, "Confirm Removal",
        QString("Are you sure you want to remove output '%1'?").arg(outputName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        flexoutput_error_t result = flexoutput_destroy_output(outputName.toUtf8().constData());
        if (result == FLEXOUTPUT_SUCCESS) {
            m_statusLabel->setText(QString("Output '%1' removed successfully").arg(outputName));
            updateUI();
        } else {
            QMessageBox::warning(this, "Error",
                QString("Failed to remove output '%1': Error %2").arg(outputName).arg(result));
        }
    }
}

void FlexOutputMainWindow::onStartOutputClicked()
{
    QString outputName = m_outputWidget->getCurrentOutput();
    if (outputName.isEmpty()) {
        QMessageBox::information(this, "Information", "Please select an output to start.");
        return;
    }
    
    // Find output instance
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    for (size_t i = 0; i < plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = plugin->outputs.array[i];
        if (output && strcmp(output->config.name, outputName.toUtf8().constData()) == 0) {
            flexoutput_error_t result = flexoutput_output_start(output);
            if (result == FLEXOUTPUT_SUCCESS) {
                m_statusLabel->setText(QString("Output '%1' started successfully").arg(outputName));
            } else {
                QMessageBox::warning(this, "Error",
                    QString("Failed to start output '%1': Error %2").arg(outputName).arg(result));
            }
            break;
        }
    }
    
    updateUI();
}

void FlexOutputMainWindow::onStopOutputClicked()
{
    QString outputName = m_outputWidget->getCurrentOutput();
    if (outputName.isEmpty()) {
        QMessageBox::information(this, "Information", "Please select an output to stop.");
        return;
    }
    
    // Find output instance
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    for (size_t i = 0; i < plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = plugin->outputs.array[i];
        if (output && strcmp(output->config.name, outputName.toUtf8().constData()) == 0) {
            flexoutput_error_t result = flexoutput_output_stop(output);
            if (result == FLEXOUTPUT_SUCCESS) {
                m_statusLabel->setText(QString("Output '%1' stopped successfully").arg(outputName));
            } else {
                QMessageBox::warning(this, "Error",
                    QString("Failed to stop output '%1': Error %2").arg(outputName).arg(result));
            }
            break;
        }
    }
    
    updateUI();
}

void FlexOutputMainWindow::onConfigureOutputClicked()
{
    QString outputName = m_outputWidget->getCurrentOutput();
    if (outputName.isEmpty()) {
        QMessageBox::information(this, "Information", "Please select an output to configure.");
        return;
    }
    
    // Find output instance
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    for (size_t i = 0; i < plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = plugin->outputs.array[i];
        if (output && strcmp(output->config.name, outputName.toUtf8().constData()) == 0) {
            FlexOutputConfigDialog dialog(outputName, this);
            dialog.setConfiguration(output->config);
            
            if (dialog.exec() == QDialog::Accepted) {
                flexoutput_output_config_t new_config = dialog.getConfiguration();
                flexoutput_error_t result = flexoutput_output_update_config(output, &new_config);
                
                if (result == FLEXOUTPUT_SUCCESS) {
                    m_statusLabel->setText(QString("Output '%1' configuration updated").arg(outputName));
                    updateUI();
                } else {
                    QMessageBox::warning(this, "Error",
                        QString("Failed to update output '%1': Error %2").arg(outputName).arg(result));
                }
            }
            break;
        }
    }
}

void FlexOutputMainWindow::onOutputSelectionChanged()
{
    QString outputName = m_outputWidget->getCurrentOutput();
    m_currentOutputName = outputName;
    
    if (m_mappingWidget) {
        m_mappingWidget->setCurrentOutput(outputName);
    }
}

void FlexOutputMainWindow::onRefreshClicked()
{
    updateUI();
    m_statusLabel->setText("UI refreshed");
}

void FlexOutputMainWindow::onSaveConfigClicked()
{
    flexoutput_error_t result = flexoutput_config_save();
    if (result == FLEXOUTPUT_SUCCESS) {
        m_statusLabel->setText("Configuration saved successfully");
    } else {
        QMessageBox::warning(this, "Error", QString("Failed to save configuration: Error %1").arg(result));
    }
}

void FlexOutputMainWindow::onLoadConfigClicked()
{
    flexoutput_error_t result = flexoutput_config_load();
    if (result == FLEXOUTPUT_SUCCESS) {
        m_statusLabel->setText("Configuration loaded successfully");
        updateUI();
    } else {
        QMessageBox::warning(this, "Error", QString("Failed to load configuration: Error %1").arg(result));
    }
}

void FlexOutputMainWindow::onExportConfigClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Configuration", 
        "flexoutput_config.json", "JSON Files (*.json)");
    
    if (!fileName.isEmpty()) {
        char *json_str = flexoutput_config_export_json();
        if (json_str) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << json_str;
                file.close();
                m_statusLabel->setText(QString("Configuration exported to %1").arg(fileName));
            } else {
                QMessageBox::warning(this, "Error", 
                    QString("Failed to write to file: %1").arg(fileName));
            }
            bfree(json_str);
        } else {
            QMessageBox::warning(this, "Error", 
                "Failed to export configuration: Unable to generate JSON");
        }
    }
}

void FlexOutputMainWindow::onImportConfigClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import Configuration", 
        "", "JSON Files (*.json)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();
            
            flexoutput_error_t result = flexoutput_config_import_json(content.toUtf8().constData());
            if (result == FLEXOUTPUT_SUCCESS) {
                m_statusLabel->setText(QString("Configuration imported from %1").arg(fileName));
                updateUI();
            } else {
                QMessageBox::warning(this, "Error", 
                    QString("Failed to import configuration: Error %1").arg(result));
            }
        } else {
            QMessageBox::warning(this, "Error", 
                QString("Failed to read file: %1").arg(fileName));
        }
    }
}

void FlexOutputMainWindow::onAboutClicked()
{
    QMessageBox::about(this, "About FlexOutput",
        QString("FlexOutput Plugin v%1\n\n"
                "A flexible multi-output plugin for OBS Studio.\n"
                "Allows routing any source to any output with whitelist/blacklist rules.\n\n"
                "Copyright (C) 2024 FlexOutput Team")
                .arg(PLUGIN_VERSION));
}

void FlexOutputMainWindow::onUpdateTimer()
{
    if (!m_isClosing) {
        updateStatus();
    }
}

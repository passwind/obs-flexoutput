#include "flexoutput-ui.h"
#include "flexoutput-config.h"
#include "flexoutput-source.h"
#include "flexoutput-output.h"
#include "flexoutput-mapping.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include "plugin-support.h"

// Global UI instance
FlexOutputMainWindow *g_flexoutput_main_window = nullptr;

// C interface functions
extern "C" {

void flexoutput_ui_init(void)
{
    if (g_flexoutput_main_window) {
        return;
    }

    QWidget *main_window = (QWidget*)obs_frontend_get_main_window();
    g_flexoutput_main_window = new FlexOutputMainWindow(main_window);
    
    obs_log(LOG_INFO, "FlexOutput UI initialized");
}

void flexoutput_ui_cleanup(void)
{
    if (g_flexoutput_main_window) {
        delete g_flexoutput_main_window;
        g_flexoutput_main_window = nullptr;
    }
    
    obs_log(LOG_INFO, "FlexOutput UI cleaned up");
}

void flexoutput_ui_show(void)
{
    if (g_flexoutput_main_window) {
        g_flexoutput_main_window->show();
        g_flexoutput_main_window->raise();
        g_flexoutput_main_window->activateWindow();
    }
}

void flexoutput_ui_hide(void)
{
    if (g_flexoutput_main_window) {
        g_flexoutput_main_window->hide();
    }
}

} // extern "C"

// FlexOutputMainWindow implementation
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
    setWindowTitle("FlexOutput - Multi-Output Manager");
    setWindowIcon(QIcon(":/icons/flexoutput.png"));
    resize(1000, 700);
    
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

// FlexOutputOutputWidget implementation
FlexOutputOutputWidget::FlexOutputOutputWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_buttonLayout(nullptr)
    , m_outputTable(nullptr)
    , m_addButton(nullptr)
    , m_removeButton(nullptr)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_configureButton(nullptr)
    , m_refreshButton(nullptr)
{
    setupUI();
    updateOutputList();
}

FlexOutputOutputWidget::~FlexOutputOutputWidget()
{
}

void FlexOutputOutputWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    
    // Create output table
    m_outputTable = new QTableWidget(this);
    m_outputTable->setColumnCount(5);
    QStringList headers;
    headers << "Name" << "Type" << "Status" << "Server" << "Enabled";
    m_outputTable->setHorizontalHeaderLabels(headers);
    m_outputTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_outputTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_outputTable->horizontalHeader()->setStretchLastSection(true);
    m_outputTable->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Create buttons
    m_buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton("Add Output", this);
    m_removeButton = new QPushButton("Remove", this);
    m_startButton = new QPushButton("Start", this);
    m_stopButton = new QPushButton("Stop", this);
    m_configureButton = new QPushButton("Configure", this);
    m_refreshButton = new QPushButton("Refresh", this);
    
    m_buttonLayout->addWidget(m_addButton);
    m_buttonLayout->addWidget(m_removeButton);
    m_buttonLayout->addSpacing(20);
    m_buttonLayout->addWidget(m_startButton);
    m_buttonLayout->addWidget(m_stopButton);
    m_buttonLayout->addSpacing(20);
    m_buttonLayout->addWidget(m_configureButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_refreshButton);
    
    m_layout->addWidget(m_outputTable);
    m_layout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_outputTable, &QTableWidget::itemSelectionChanged,
            this, &FlexOutputOutputWidget::onOutputItemChanged);
    connect(m_outputTable, &QTableWidget::itemDoubleClicked,
            this, &FlexOutputOutputWidget::onOutputDoubleClicked);
    connect(m_outputTable, &QTableWidget::customContextMenuRequested,
            this, &FlexOutputOutputWidget::onContextMenuRequested);
    
    connect(m_addButton, &QPushButton::clicked, this, &FlexOutputOutputWidget::addOutputRequested);
    connect(m_removeButton, &QPushButton::clicked, [this]() {
        emit removeOutputRequested(getCurrentOutput());
    });
    connect(m_startButton, &QPushButton::clicked, [this]() {
        emit startOutputRequested(getCurrentOutput());
    });
    connect(m_stopButton, &QPushButton::clicked, [this]() {
        emit stopOutputRequested(getCurrentOutput());
    });
    connect(m_configureButton, &QPushButton::clicked, [this]() {
        emit configureOutputRequested(getCurrentOutput());
    });
    connect(m_refreshButton, &QPushButton::clicked, [this]() {
        updateOutputList();
    });
    
    // Initial button states
    m_removeButton->setEnabled(false);
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_configureButton->setEnabled(false);
}

void FlexOutputOutputWidget::updateOutputList()
{
    m_outputTable->setRowCount(0);
    
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    for (size_t i = 0; i < plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = plugin->outputs.array[i];
        if (!output) {
            continue;
        }
        
        int row = m_outputTable->rowCount();
        m_outputTable->insertRow(row);
        
        // Name
        m_outputTable->setItem(row, 0, new QTableWidgetItem(output->config.name));
        
        // Type
        const char *type_str = "Unknown";
        switch (output->config.type) {
        case FLEXOUTPUT_OUTPUT_RTMP: type_str = "RTMP"; break;
        case FLEXOUTPUT_OUTPUT_FILE: type_str = "File"; break;
        case FLEXOUTPUT_OUTPUT_UDP: type_str = "UDP"; break;
        case FLEXOUTPUT_OUTPUT_SRT: type_str = "SRT"; break;
        case FLEXOUTPUT_OUTPUT_WEBRTC: type_str = "WebRTC"; break;
        case FLEXOUTPUT_OUTPUT_CUSTOM: type_str = "Custom"; break;
        }
        m_outputTable->setItem(row, 1, new QTableWidgetItem(type_str));
        
        // Status
        const char *status_str = flexoutput_output_is_active(output) ? "Active" : "Inactive";
        m_outputTable->setItem(row, 2, new QTableWidgetItem(status_str));
        
        // Server
        m_outputTable->setItem(row, 3, new QTableWidgetItem(output->config.server));
        
        // Enabled
        QTableWidgetItem *enabledItem = new QTableWidgetItem();
        enabledItem->setCheckState(output->config.enabled ? Qt::Checked : Qt::Unchecked);
        m_outputTable->setItem(row, 4, enabledItem);
    }
    
    updateOutputStatus();
}

void FlexOutputOutputWidget::updateOutputStatus()
{
    QString current = getCurrentOutput();
    bool hasSelection = !current.isEmpty();
    
    m_removeButton->setEnabled(hasSelection);
    m_startButton->setEnabled(hasSelection);
    m_stopButton->setEnabled(hasSelection);
    m_configureButton->setEnabled(hasSelection);
}

void FlexOutputOutputWidget::setCurrentOutput(const QString &outputName)
{
    m_currentOutput = outputName;
    
    // Select the row in table
    for (int i = 0; i < m_outputTable->rowCount(); i++) {
        QTableWidgetItem *item = m_outputTable->item(i, 0);
        if (item && item->text() == outputName) {
            m_outputTable->selectRow(i);
            break;
        }
    }
    
    updateOutputStatus();
}

QString FlexOutputOutputWidget::getCurrentOutput() const
{
    int currentRow = m_outputTable->currentRow();
    if (currentRow >= 0) {
        QTableWidgetItem *item = m_outputTable->item(currentRow, 0);
        if (item) {
            return item->text();
        }
    }
    return QString();
}

void FlexOutputOutputWidget::onOutputItemChanged()
{
    QString outputName = getCurrentOutput();
    if (outputName != m_currentOutput) {
        m_currentOutput = outputName;
        emit outputSelectionChanged(outputName);
    }
    updateOutputStatus();
}

void FlexOutputOutputWidget::onOutputDoubleClicked()
{
    QString outputName = getCurrentOutput();
    if (!outputName.isEmpty()) {
        emit configureOutputRequested(outputName);
    }
}

void FlexOutputOutputWidget::onContextMenuRequested(const QPoint &pos)
{
    QMenu contextMenu(this);
    
    QString outputName = getCurrentOutput();
    bool hasSelection = !outputName.isEmpty();
    
    contextMenu.addAction("Add Output", [this]() { emit addOutputRequested(); });
    
    if (hasSelection) {
        contextMenu.addSeparator();
        contextMenu.addAction("Configure", [this, outputName]() { 
            emit configureOutputRequested(outputName); 
        });
        contextMenu.addAction("Start", [this, outputName]() { 
            emit startOutputRequested(outputName); 
        });
        contextMenu.addAction("Stop", [this, outputName]() { 
            emit stopOutputRequested(outputName); 
        });
        contextMenu.addSeparator();
        contextMenu.addAction("Remove", [this, outputName]() { 
            emit removeOutputRequested(outputName); 
        });
    }
    
    contextMenu.exec(m_outputTable->mapToGlobal(pos));
}

// Include the MOC file for Qt's meta-object system
#include "flexoutput-ui.moc"

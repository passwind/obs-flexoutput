#include "flexoutput-ui.h"
#include "flexoutput-config.h"
#include "flexoutput-source.h"
#include "flexoutput-output.h"
#include "flexoutput-mapping.h"

#include <util/platform.h>

// FlexOutputMappingWidget implementation
FlexOutputMappingWidget::FlexOutputMappingWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_topLayout(nullptr)
    , m_middleLayout(nullptr)
    , m_bottomLayout(nullptr)
    , m_outputLabel(nullptr)
    , m_mappingTypeCombo(nullptr)
    , m_enabledCheckBox(nullptr)
    , m_splitter(nullptr)
    , m_availableGroup(nullptr)
    , m_mappedGroup(nullptr)
    , m_availableSourcesList(nullptr)
    , m_mappedSourcesList(nullptr)
    , m_addButton(nullptr)
    , m_removeButton(nullptr)
    , m_clearButton(nullptr)
    , m_addAllButton(nullptr)
    , m_removeAllButton(nullptr)
    , m_currentRule(nullptr)
{
    setupUI();
}

FlexOutputMappingWidget::~FlexOutputMappingWidget()
{
}

void FlexOutputMappingWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    
    // Top layout - output selection and mapping type
    m_topLayout = new QHBoxLayout();
    m_outputLabel = new QLabel("No output selected", this);
    m_mappingTypeCombo = new QComboBox(this);
    m_mappingTypeCombo->addItem("Whitelist", static_cast<int>(FLEXOUTPUT_MAPPING_WHITELIST));
    m_mappingTypeCombo->addItem("Blacklist", static_cast<int>(FLEXOUTPUT_MAPPING_BLACKLIST));
    m_enabledCheckBox = new QCheckBox("Enabled", this);
    
    m_topLayout->addWidget(new QLabel("Output:", this));
    m_topLayout->addWidget(m_outputLabel);
    m_topLayout->addStretch();
    m_topLayout->addWidget(new QLabel("Type:", this));
    m_topLayout->addWidget(m_mappingTypeCombo);
    m_topLayout->addWidget(m_enabledCheckBox);
    
    // Middle layout - source lists
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Available sources
    m_availableGroup = new QGroupBox("Available Sources", this);
    QVBoxLayout *availableLayout = new QVBoxLayout(m_availableGroup);
    m_availableSourcesList = new QListWidget(this);
    m_availableSourcesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    availableLayout->addWidget(m_availableSourcesList);
    
    // Mapped sources
    m_mappedGroup = new QGroupBox("Mapped Sources", this);
    QVBoxLayout *mappedLayout = new QVBoxLayout(m_mappedGroup);
    m_mappedSourcesList = new QListWidget(this);
    m_mappedSourcesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mappedLayout->addWidget(m_mappedSourcesList);
    
    m_splitter->addWidget(m_availableGroup);
    m_splitter->addWidget(m_mappedGroup);
    m_splitter->setSizes({400, 400});
    
    // Bottom layout - buttons
    m_bottomLayout = new QHBoxLayout();
    m_addButton = new QPushButton("Add →", this);
    m_removeButton = new QPushButton("← Remove", this);
    m_addAllButton = new QPushButton("Add All →", this);
    m_removeAllButton = new QPushButton("← Remove All", this);
    m_clearButton = new QPushButton("Clear", this);
    
    m_bottomLayout->addWidget(m_addAllButton);
    m_bottomLayout->addWidget(m_addButton);
    m_bottomLayout->addStretch();
    m_bottomLayout->addWidget(m_removeButton);
    m_bottomLayout->addWidget(m_removeAllButton);
    m_bottomLayout->addStretch();
    m_bottomLayout->addWidget(m_clearButton);
    
    // Add to main layout
    m_layout->addLayout(m_topLayout);
    m_layout->addWidget(m_splitter);
    m_layout->addLayout(m_bottomLayout);
    
    // Connect signals
    connect(m_mappingTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FlexOutputMappingWidget::onMappingTypeChanged);
    connect(m_enabledCheckBox, &QCheckBox::toggled,
            this, &FlexOutputMappingWidget::onMappingTypeChanged);
    
    connect(m_addButton, &QPushButton::clicked, this, &FlexOutputMappingWidget::onAddSourceClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &FlexOutputMappingWidget::onRemoveSourceClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &FlexOutputMappingWidget::onClearSourcesClicked);
    
    connect(m_availableSourcesList, &QListWidget::itemDoubleClicked,
            this, &FlexOutputMappingWidget::onAvailableSourceDoubleClicked);
    connect(m_mappedSourcesList, &QListWidget::itemDoubleClicked,
            this, &FlexOutputMappingWidget::onMappedSourceDoubleClicked);
    
    // Initial state
    setEnabled(false);
}

void FlexOutputMappingWidget::setCurrentOutput(const QString &outputName)
{
    m_currentOutput = outputName;
    m_outputLabel->setText(outputName.isEmpty() ? "No output selected" : outputName);
    
    setEnabled(!outputName.isEmpty());
    
    if (!outputName.isEmpty()) {
        updateMappingRules();
        updateAvailableSources();
    }
}

void FlexOutputMappingWidget::updateMappingRules()
{
    if (m_currentOutput.isEmpty()) {
        return;
    }
    
    // Find or create mapping rule for current output
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    m_currentRule = nullptr;
    for (size_t i = 0; i < plugin->mapping_rules.num; i++) {
        flexoutput_mapping_rule_t *rule = plugin->mapping_rules.array[i];
        if (rule && strcmp(rule->output_name, m_currentOutput.toUtf8().constData()) == 0) {
            m_currentRule = rule;
            break;
        }
    }
    
    // Create rule if it doesn't exist
    if (!m_currentRule) {
        m_currentRule = flexoutput_mapping_create_rule(
            m_currentOutput.toUtf8().constData(), 
            FLEXOUTPUT_MAPPING_WHITELIST);
        if (m_currentRule) {
            da_push_back(plugin->mapping_rules, &m_currentRule);
        }
    }
    
    if (m_currentRule) {
        // Update UI controls
        m_mappingTypeCombo->setCurrentIndex(static_cast<int>(m_currentRule->type));
        m_enabledCheckBox->setChecked(m_currentRule->enabled);
        
        updateMappedSources();
    }
}

void FlexOutputMappingWidget::updateAvailableSources()
{
    m_availableSourcesList->clear();
    
    char **sources = nullptr;
    size_t count = 0;
    if (flexoutput_source_enumerate_all(&sources, &count) == FLEXOUTPUT_SUCCESS) {
        for (size_t i = 0; i < count; i++) {
            // Only show sources that are not already mapped
            if (!m_currentRule || !flexoutput_mapping_has_source(m_currentRule, sources[i])) {
                m_availableSourcesList->addItem(sources[i]);
            }
        }
        flexoutput_source_free_string_array(sources, count);
    }
}

void FlexOutputMappingWidget::updateMappedSources()
{
    m_mappedSourcesList->clear();
    
    if (m_currentRule) {
        for (size_t i = 0; i < m_currentRule->source_names.num; i++) {
            m_mappedSourcesList->addItem(m_currentRule->source_names.array[i]);
        }
    }
}

void FlexOutputMappingWidget::onMappingTypeChanged()
{
    if (m_currentRule) {
        flexoutput_mapping_type_t newType = static_cast<flexoutput_mapping_type_t>(
            m_mappingTypeCombo->currentData().toInt());
        bool enabled = m_enabledCheckBox->isChecked();
        
        m_currentRule->type = newType;
        m_currentRule->enabled = enabled;
        
        applyMappingChanges();
        emit mappingRulesChanged();
    }
}

void FlexOutputMappingWidget::onAddSourceClicked()
{
    if (!m_currentRule) {
        return;
    }
    
    QList<QListWidgetItem*> selectedItems = m_availableSourcesList->selectedItems();
    for (QListWidgetItem *item : selectedItems) {
        QString sourceName = item->text();
        flexoutput_mapping_add_source(m_currentRule, sourceName.toUtf8().constData());
    }
    
    updateAvailableSources();
    updateMappedSources();
    applyMappingChanges();
    emit mappingRulesChanged();
}

void FlexOutputMappingWidget::onRemoveSourceClicked()
{
    if (!m_currentRule) {
        return;
    }
    
    QList<QListWidgetItem*> selectedItems = m_mappedSourcesList->selectedItems();
    for (QListWidgetItem *item : selectedItems) {
        QString sourceName = item->text();
        flexoutput_mapping_remove_source(m_currentRule, sourceName.toUtf8().constData());
    }
    
    updateAvailableSources();
    updateMappedSources();
    applyMappingChanges();
    emit mappingRulesChanged();
}

void FlexOutputMappingWidget::onClearSourcesClicked()
{
    if (!m_currentRule) {
        return;
    }
    
    flexoutput_mapping_clear_sources(m_currentRule);
    
    updateAvailableSources();
    updateMappedSources();
    applyMappingChanges();
    emit mappingRulesChanged();
}

void FlexOutputMappingWidget::onAvailableSourceDoubleClicked()
{
    onAddSourceClicked();
}

void FlexOutputMappingWidget::onMappedSourceDoubleClicked()
{
    onRemoveSourceClicked();
}

void FlexOutputMappingWidget::applyMappingChanges()
{
    // This would trigger the main plugin to update source mappings
    // The actual implementation would call the plugin's update function
}

// FlexOutputSourceListWidget implementation
FlexOutputSourceListWidget::FlexOutputSourceListWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_filterLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_filterLabel(nullptr)
    , m_sourceTypeFilter(nullptr)
    , m_refreshButton(nullptr)
    , m_sourceTable(nullptr)
{
    setupUI();
    updateSourceList();
}

FlexOutputSourceListWidget::~FlexOutputSourceListWidget()
{
}

void FlexOutputSourceListWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    
    // Filter layout
    m_filterLayout = new QHBoxLayout();
    m_filterLabel = new QLabel("Filter by type:", this);
    m_sourceTypeFilter = new QComboBox(this);
    m_sourceTypeFilter->addItem("All Types", "");
    m_sourceTypeFilter->addItem("Input Sources", "input");
    m_sourceTypeFilter->addItem("Filters", "filter");
    m_sourceTypeFilter->addItem("Transitions", "transition");
    m_sourceTypeFilter->addItem("Scenes", "scene");
    
    m_filterLayout->addWidget(m_filterLabel);
    m_filterLayout->addWidget(m_sourceTypeFilter);
    m_filterLayout->addStretch();
    
    // Source table
    m_sourceTable = new QTableWidget(this);
    m_sourceTable->setColumnCount(4);
    QStringList headers;
    headers << "Name" << "Type" << "Status" << "Properties";
    m_sourceTable->setHorizontalHeaderLabels(headers);
    m_sourceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sourceTable->horizontalHeader()->setStretchLastSection(true);
    
    // Button layout
    m_buttonLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton("Refresh", this);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_refreshButton);
    
    // Add to main layout
    m_layout->addLayout(m_filterLayout);
    m_layout->addWidget(m_sourceTable);
    m_layout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_sourceTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FlexOutputSourceListWidget::onSourceTypeFilterChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, &FlexOutputSourceListWidget::onRefreshClicked);
}

void FlexOutputSourceListWidget::updateSourceList()
{
    m_sourceTable->setRowCount(0);
    
    char **sources = nullptr;
    size_t count = 0;
    if (flexoutput_source_enumerate_all(&sources, &count) == FLEXOUTPUT_SUCCESS) {
        for (size_t i = 0; i < count; i++) {
            obs_weak_source_t *weak_source = flexoutput_source_get_weak_ref(sources[i]);
            if (!weak_source) {
                continue;
            }
            obs_source_t *source = obs_weak_source_get_source(weak_source);
            
            int row = m_sourceTable->rowCount();
            m_sourceTable->insertRow(row);
            
            // Name
            m_sourceTable->setItem(row, 0, new QTableWidgetItem(sources[i]));
            
            // Type
            const char *source_id = obs_source_get_id(source);
            m_sourceTable->setItem(row, 1, new QTableWidgetItem(source_id ? source_id : "Unknown"));
            
            // Status
            bool active = obs_source_active(source);
            m_sourceTable->setItem(row, 2, new QTableWidgetItem(active ? "Active" : "Inactive"));
            
            // Properties
            QString properties;
            if (obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT) {
                uint32_t width = obs_source_get_width(source);
                uint32_t height = obs_source_get_height(source);
                if (width > 0 && height > 0) {
                    properties = QString("%1x%2").arg(width).arg(height);
                }
            }
            m_sourceTable->setItem(row, 3, new QTableWidgetItem(properties));
            
            if (source) {
                obs_source_release(source);
            }
            obs_weak_source_release(weak_source);
        }
        flexoutput_source_free_string_array(sources, count);
    }
    
    filterSources();
}

void FlexOutputSourceListWidget::onSourceTypeFilterChanged()
{
    filterSources();
}

void FlexOutputSourceListWidget::onRefreshClicked()
{
    updateSourceList();
}

void FlexOutputSourceListWidget::filterSources()
{
    QString filterType = m_sourceTypeFilter->currentData().toString();
    
    for (int i = 0; i < m_sourceTable->rowCount(); i++) {
        bool visible = true;
        
        if (!filterType.isEmpty()) {
            QTableWidgetItem *typeItem = m_sourceTable->item(i, 1);
            if (typeItem) {
                QString sourceType = typeItem->text().toLower();
                visible = sourceType.contains(filterType);
            }
        }
        
        m_sourceTable->setRowHidden(i, !visible);
    }
}

// FlexOutputStatusWidget implementation
FlexOutputStatusWidget::FlexOutputStatusWidget(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_statsLayout(nullptr)
    , m_logButtonLayout(nullptr)
    , m_statsGroup(nullptr)
    , m_logGroup(nullptr)
    , m_totalOutputsLabel(nullptr)
    , m_activeOutputsLabel(nullptr)
    , m_totalSourcesLabel(nullptr)
    , m_mappedSourcesLabel(nullptr)
    , m_uptimeLabel(nullptr)
    , m_memoryUsageLabel(nullptr)
    , m_logTextEdit(nullptr)
    , m_clearLogButton(nullptr)
    , m_saveLogButton(nullptr)
{
    setupUI();
    updateStatus();
}

FlexOutputStatusWidget::~FlexOutputStatusWidget()
{
}

void FlexOutputStatusWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    
    // Statistics group
    m_statsGroup = new QGroupBox("Statistics", this);
    m_statsLayout = new QGridLayout(m_statsGroup);
    
    m_totalOutputsLabel = new QLabel("0", this);
    m_activeOutputsLabel = new QLabel("0", this);
    m_totalSourcesLabel = new QLabel("0", this);
    m_mappedSourcesLabel = new QLabel("0", this);
    m_uptimeLabel = new QLabel("00:00:00", this);
    m_memoryUsageLabel = new QLabel("0 MB", this);
    
    m_statsLayout->addWidget(new QLabel("Total Outputs:"), 0, 0);
    m_statsLayout->addWidget(m_totalOutputsLabel, 0, 1);
    m_statsLayout->addWidget(new QLabel("Active Outputs:"), 0, 2);
    m_statsLayout->addWidget(m_activeOutputsLabel, 0, 3);
    
    m_statsLayout->addWidget(new QLabel("Total Sources:"), 1, 0);
    m_statsLayout->addWidget(m_totalSourcesLabel, 1, 1);
    m_statsLayout->addWidget(new QLabel("Mapped Sources:"), 1, 2);
    m_statsLayout->addWidget(m_mappedSourcesLabel, 1, 3);
    
    m_statsLayout->addWidget(new QLabel("Uptime:"), 2, 0);
    m_statsLayout->addWidget(m_uptimeLabel, 2, 1);
    m_statsLayout->addWidget(new QLabel("Memory Usage:"), 2, 2);
    m_statsLayout->addWidget(m_memoryUsageLabel, 2, 3);
    
    // Log group
    m_logGroup = new QGroupBox("Log", this);
    QVBoxLayout *logLayout = new QVBoxLayout(m_logGroup);
    
    m_logTextEdit = new QPlainTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMaximumBlockCount(1000); // Limit log entries
    
    m_logButtonLayout = new QHBoxLayout();
    m_clearLogButton = new QPushButton("Clear Log", this);
    m_saveLogButton = new QPushButton("Save Log", this);
    m_logButtonLayout->addStretch();
    m_logButtonLayout->addWidget(m_clearLogButton);
    m_logButtonLayout->addWidget(m_saveLogButton);
    
    logLayout->addWidget(m_logTextEdit);
    logLayout->addLayout(m_logButtonLayout);
    
    // Add to main layout
    m_layout->addWidget(m_statsGroup);
    m_layout->addWidget(m_logGroup);
    
    // Connect signals
    connect(m_clearLogButton, &QPushButton::clicked, this, &FlexOutputStatusWidget::onClearLogClicked);
    connect(m_saveLogButton, &QPushButton::clicked, this, &FlexOutputStatusWidget::onSaveLogClicked);
}

void FlexOutputStatusWidget::updateStatus()
{
    flexoutput_plugin_t *plugin = flexoutput_get_plugin_instance();
    if (!plugin) {
        return;
    }
    
    // Update statistics
    size_t totalOutputs = plugin->outputs.num;
    size_t activeOutputs = 0;
    
    for (size_t i = 0; i < plugin->outputs.num; i++) {
        flexoutput_output_instance_t *output = plugin->outputs.array[i];
        if (output && flexoutput_output_is_active(output)) {
            activeOutputs++;
        }
    }
    
    char **sources = nullptr;
    size_t totalSources = 0;
    flexoutput_source_enumerate_all(&sources, &totalSources);
    if (sources) {
        flexoutput_source_free_string_array(sources, totalSources);
    }
    
    // Count mapped sources
    size_t mappedSources = 0;
    for (size_t i = 0; i < plugin->mapping_rules.num; i++) {
        flexoutput_mapping_rule_t *rule = plugin->mapping_rules.array[i];
        if (rule) {
            mappedSources += rule->source_names.num;
        }
    }
    
    // Update labels
    m_totalOutputsLabel->setText(QString::number(totalOutputs));
    m_activeOutputsLabel->setText(QString::number(activeOutputs));
    m_totalSourcesLabel->setText(QString::number(totalSources));
    m_mappedSourcesLabel->setText(QString::number(mappedSources));
    
    // Update uptime (simplified)
    static uint64_t start_time = os_gettime_ns();
    uint64_t current_time = os_gettime_ns();
    uint64_t uptime_ns = current_time - start_time;
    uint64_t uptime_seconds = uptime_ns / 1000000000ULL;
    
    uint64_t hours = uptime_seconds / 3600;
    uint64_t minutes = (uptime_seconds % 3600) / 60;
    uint64_t seconds = uptime_seconds % 60;
    
    m_uptimeLabel->setText(QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));
    
    // Memory usage (simplified)
    m_memoryUsageLabel->setText("N/A");
}

void FlexOutputStatusWidget::onClearLogClicked()
{
    m_logTextEdit->clear();
}

void FlexOutputStatusWidget::onSaveLogClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Log", 
        "flexoutput_log.txt", "Text Files (*.txt)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_logTextEdit->toPlainText();
            file.close();
        }
    }
}

void FlexOutputStatusWidget::addLogEntry(const QString &level, const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2: %3").arg(timestamp, level, message);
    m_logTextEdit->appendPlainText(logEntry);
}

// FlexOutputConfigDialog implementation
FlexOutputConfigDialog::FlexOutputConfigDialog(const QString &outputName, QWidget *parent)
    : QDialog(parent)
    , m_outputName(outputName)
    , m_isValid(false)
{
    setWindowTitle(outputName.isEmpty() ? "Add Output" : QString("Configure Output: %1").arg(outputName));
    setModal(true);
    resize(500, 400);
    
    setupUI();
    
    if (!outputName.isEmpty()) {
        m_nameEdit->setText(outputName);
        m_nameEdit->setEnabled(false); // Don't allow renaming existing outputs
    }
}

FlexOutputConfigDialog::~FlexOutputConfigDialog()
{
}

void FlexOutputConfigDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    
    setupOutputSettings();
    setupVideoSettings();
    setupAudioSettings();
    setupAdvancedSettings();
    
    // Button layout
    m_buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_resetButton = new QPushButton("Reset to Defaults", this);
    
    m_buttonLayout->addWidget(m_resetButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addWidget(m_cancelButton);
    
    // Add to main layout
    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_okButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onRejected);
    connect(m_resetButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onResetToDefaultsClicked);
    
    // Set default values
    onResetToDefaultsClicked();
}

void FlexOutputConfigDialog::setupOutputSettings()
{
    m_outputTab = new QWidget();
    m_outputLayout = new QGridLayout(m_outputTab);
    
    m_nameEdit = new QLineEdit(this);
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("RTMP", static_cast<int>(FLEXOUTPUT_OUTPUT_RTMP));
    m_typeCombo->addItem("File", static_cast<int>(FLEXOUTPUT_OUTPUT_FILE));
    m_typeCombo->addItem("UDP", static_cast<int>(FLEXOUTPUT_OUTPUT_UDP));
    m_typeCombo->addItem("SRT", static_cast<int>(FLEXOUTPUT_OUTPUT_SRT));
    m_typeCombo->addItem("WebRTC", static_cast<int>(FLEXOUTPUT_OUTPUT_WEBRTC));
    m_typeCombo->addItem("Custom", static_cast<int>(FLEXOUTPUT_OUTPUT_CUSTOM));
    
    m_serverEdit = new QLineEdit(this);
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_enabledCheckBox = new QCheckBox(this);
    m_testButton = new QPushButton("Test Connection", this);
    
    m_outputLayout->addWidget(new QLabel("Name:"), 0, 0);
    m_outputLayout->addWidget(m_nameEdit, 0, 1);
    
    m_outputLayout->addWidget(new QLabel("Type:"), 1, 0);
    m_outputLayout->addWidget(m_typeCombo, 1, 1);
    
    m_outputLayout->addWidget(new QLabel("Server:"), 2, 0);
    m_outputLayout->addWidget(m_serverEdit, 2, 1);
    
    m_outputLayout->addWidget(new QLabel("Stream Key:"), 3, 0);
    m_outputLayout->addWidget(m_keyEdit, 3, 1);
    
    m_outputLayout->addWidget(new QLabel("Enabled:"), 4, 0);
    m_outputLayout->addWidget(m_enabledCheckBox, 4, 1);
    
    m_outputLayout->addWidget(m_testButton, 5, 1);
    
    m_tabWidget->addTab(m_outputTab, "Output");
    
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FlexOutputConfigDialog::onOutputTypeChanged);
    connect(m_testButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onTestConnectionClicked);
}

void FlexOutputConfigDialog::setupVideoSettings()
{
    m_videoTab = new QWidget();
    m_videoLayout = new QGridLayout(m_videoTab);
    
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(128, 7680);
    m_widthSpin->setValue(1920);
    
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(128, 4320);
    m_heightSpin->setValue(1080);
    
    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(1, 120);
    m_fpsSpin->setValue(30);
    
    m_bitrateSpin = new QSpinBox(this);
    m_bitrateSpin->setRange(100, 50000);
    m_bitrateSpin->setValue(2500);
    m_bitrateSpin->setSuffix(" kbps");
    
    m_encoderCombo = new QComboBox(this);
    m_encoderCombo->addItem("x264");
    m_encoderCombo->addItem("Hardware (NVENC)");
    m_encoderCombo->addItem("Hardware (AMD)");
    m_encoderCombo->addItem("Hardware (QuickSync)");
    
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("ultrafast");
    m_presetCombo->addItem("superfast");
    m_presetCombo->addItem("veryfast");
    m_presetCombo->addItem("faster");
    m_presetCombo->addItem("fast");
    m_presetCombo->addItem("medium");
    m_presetCombo->addItem("slow");
    m_presetCombo->addItem("slower");
    m_presetCombo->addItem("veryslow");
    
    m_profileCombo = new QComboBox(this);
    m_profileCombo->addItem("baseline");
    m_profileCombo->addItem("main");
    m_profileCombo->addItem("high");
    
    m_videoLayout->addWidget(new QLabel("Width:"), 0, 0);
    m_videoLayout->addWidget(m_widthSpin, 0, 1);
    
    m_videoLayout->addWidget(new QLabel("Height:"), 1, 0);
    m_videoLayout->addWidget(m_heightSpin, 1, 1);
    
    m_videoLayout->addWidget(new QLabel("FPS:"), 2, 0);
    m_videoLayout->addWidget(m_fpsSpin, 2, 1);
    
    m_videoLayout->addWidget(new QLabel("Bitrate:"), 3, 0);
    m_videoLayout->addWidget(m_bitrateSpin, 3, 1);
    
    m_videoLayout->addWidget(new QLabel("Encoder:"), 4, 0);
    m_videoLayout->addWidget(m_encoderCombo, 4, 1);
    
    m_videoLayout->addWidget(new QLabel("Preset:"), 5, 0);
    m_videoLayout->addWidget(m_presetCombo, 5, 1);
    
    m_videoLayout->addWidget(new QLabel("Profile:"), 6, 0);
    m_videoLayout->addWidget(m_profileCombo, 6, 1);
    
    m_tabWidget->addTab(m_videoTab, "Video");
}

void FlexOutputConfigDialog::setupAudioSettings()
{
    m_audioTab = new QWidget();
    m_audioLayout = new QGridLayout(m_audioTab);
    
    m_sampleRateSpin = new QSpinBox(this);
    m_sampleRateSpin->setRange(8000, 192000);
    m_sampleRateSpin->setValue(44100);
    m_sampleRateSpin->setSuffix(" Hz");
    
    m_audioBitrateSpin = new QSpinBox(this);
    m_audioBitrateSpin->setRange(32, 320);
    m_audioBitrateSpin->setValue(128);
    m_audioBitrateSpin->setSuffix(" kbps");
    
    m_audioEncoderCombo = new QComboBox(this);
    m_audioEncoderCombo->addItem("AAC");
    m_audioEncoderCombo->addItem("MP3");
    
    m_channelsCombo = new QComboBox(this);
    m_channelsCombo->addItem("Mono", 1);
    m_channelsCombo->addItem("Stereo", 2);
    m_channelsCombo->addItem("2.1", 3);
    m_channelsCombo->addItem("4.0", 4);
    m_channelsCombo->addItem("4.1", 5);
    m_channelsCombo->addItem("5.1", 6);
    m_channelsCombo->addItem("7.1", 8);
    
    m_audioLayout->addWidget(new QLabel("Sample Rate:"), 0, 0);
    m_audioLayout->addWidget(m_sampleRateSpin, 0, 1);
    
    m_audioLayout->addWidget(new QLabel("Bitrate:"), 1, 0);
    m_audioLayout->addWidget(m_audioBitrateSpin, 1, 1);
    
    m_audioLayout->addWidget(new QLabel("Encoder:"), 2, 0);
    m_audioLayout->addWidget(m_audioEncoderCombo, 2, 1);
    
    m_audioLayout->addWidget(new QLabel("Channels:"), 3, 0);
    m_audioLayout->addWidget(m_channelsCombo, 3, 1);
    
    m_tabWidget->addTab(m_audioTab, "Audio");
}

void FlexOutputConfigDialog::setupAdvancedSettings()
{
    m_advancedTab = new QWidget();
    m_advancedLayout = new QGridLayout(m_advancedTab);
    
    m_keyframeSpin = new QSpinBox(this);
    m_keyframeSpin->setRange(1, 10);
    m_keyframeSpin->setValue(2);
    m_keyframeSpin->setSuffix(" seconds");
    
    m_cbr_CheckBox = new QCheckBox("Use Constant Bitrate (CBR)", this);
    
    m_customSettingsEdit = new QLineEdit(this);
    m_customSettingsEdit->setPlaceholderText("Custom encoder settings...");
    
    m_advancedLayout->addWidget(new QLabel("Keyframe Interval:"), 0, 0);
    m_advancedLayout->addWidget(m_keyframeSpin, 0, 1);
    
    m_advancedLayout->addWidget(m_cbr_CheckBox, 1, 0, 1, 2);
    
    m_advancedLayout->addWidget(new QLabel("Custom Settings:"), 2, 0);
    m_advancedLayout->addWidget(m_customSettingsEdit, 2, 1);
    
    m_tabWidget->addTab(m_advancedTab, "Advanced");
}

flexoutput_output_config_t FlexOutputConfigDialog::getConfiguration() const
{
    flexoutput_output_config_t config = {};
    
    // Basic settings
    strncpy(config.name, m_nameEdit->text().toUtf8().constData(), sizeof(config.name) - 1);
    strncpy(config.server, m_serverEdit->text().toUtf8().constData(), sizeof(config.server) - 1);
    strncpy(config.key, m_keyEdit->text().toUtf8().constData(), sizeof(config.key) - 1);
    
    config.type = static_cast<flexoutput_output_type_t>(m_typeCombo->currentData().toInt());
    config.enabled = m_enabledCheckBox->isChecked();
    
    // Video settings
    config.video.width = m_widthSpin->value();
    config.video.height = m_heightSpin->value();
    config.video.fps = m_fpsSpin->value();
    config.video.bitrate = m_bitrateSpin->value();
    
    strncpy(config.video.encoder, m_encoderCombo->currentText().toUtf8().constData(), 
            sizeof(config.video.encoder) - 1);
    strncpy(config.video.preset, m_presetCombo->currentText().toUtf8().constData(), 
            sizeof(config.video.preset) - 1);
    strncpy(config.video.profile, m_profileCombo->currentText().toUtf8().constData(), 
            sizeof(config.video.profile) - 1);
    
    // Audio settings
    config.audio.sample_rate = m_sampleRateSpin->value();
    config.audio.bitrate = m_audioBitrateSpin->value();
    config.audio.channels = m_channelsCombo->currentData().toInt();
    
    strncpy(config.audio.encoder, m_audioEncoderCombo->currentText().toUtf8().constData(), 
            sizeof(config.audio.encoder) - 1);
    
    // Advanced settings
    config.keyframe_interval = m_keyframeSpin->value();
    config.use_cbr = m_cbr_CheckBox->isChecked();
    
    strncpy(config.custom_settings, m_customSettingsEdit->text().toUtf8().constData(), 
            sizeof(config.custom_settings) - 1);
    
    return config;
}

void FlexOutputConfigDialog::setConfiguration(const flexoutput_output_config_t &config)
{
    m_config = config;
    
    // Basic settings
    m_nameEdit->setText(config.name);
    m_serverEdit->setText(config.server);
    m_keyEdit->setText(config.key);
    
    // Find and set type
    for (int i = 0; i < m_typeCombo->count(); i++) {
        if (m_typeCombo->itemData(i).toInt() == static_cast<int>(config.type)) {
            m_typeCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_enabledCheckBox->setChecked(config.enabled);
    
    // Video settings
    m_widthSpin->setValue(config.video.width);
    m_heightSpin->setValue(config.video.height);
    m_fpsSpin->setValue(config.video.fps);
    m_bitrateSpin->setValue(config.video.bitrate);
    
    // Find and set encoder, preset, profile
    int encoderIndex = m_encoderCombo->findText(config.video.encoder);
    if (encoderIndex >= 0) m_encoderCombo->setCurrentIndex(encoderIndex);
    
    int presetIndex = m_presetCombo->findText(config.video.preset);
    if (presetIndex >= 0) m_presetCombo->setCurrentIndex(presetIndex);
    
    int profileIndex = m_profileCombo->findText(config.video.profile);
    if (profileIndex >= 0) m_profileCombo->setCurrentIndex(profileIndex);
    
    // Audio settings
    m_sampleRateSpin->setValue(config.audio.sample_rate);
    m_audioBitrateSpin->setValue(config.audio.bitrate);
    
    // Find and set channels
    for (int i = 0; i < m_channelsCombo->count(); i++) {
        if (m_channelsCombo->itemData(i).toInt() == static_cast<int>(config.audio.channels)) {
            m_channelsCombo->setCurrentIndex(i);
            break;
        }
    }
    
    int audioEncoderIndex = m_audioEncoderCombo->findText(config.audio.encoder);
    if (audioEncoderIndex >= 0) m_audioEncoderCombo->setCurrentIndex(audioEncoderIndex);
    
    // Advanced settings
    m_keyframeSpin->setValue(config.keyframe_interval);
    m_cbr_CheckBox->setChecked(config.use_cbr);
    m_customSettingsEdit->setText(config.custom_settings);
}

void FlexOutputConfigDialog::onOutputTypeChanged()
{
    // Update UI based on output type
    flexoutput_output_type_t type = static_cast<flexoutput_output_type_t>(
        m_typeCombo->currentData().toInt());
    
    switch (type) {
    case FLEXOUTPUT_OUTPUT_RTMP:
        m_serverEdit->setPlaceholderText("rtmp://live.twitch.tv/live/");
        m_keyEdit->setPlaceholderText("Stream key...");
        m_testButton->setEnabled(true);
        break;
    case FLEXOUTPUT_OUTPUT_FILE:
        m_serverEdit->setPlaceholderText("/path/to/output/file.mp4");
        m_keyEdit->setPlaceholderText("Not used for file output");
        m_keyEdit->setEnabled(false);
        m_testButton->setEnabled(false);
        break;
    case FLEXOUTPUT_OUTPUT_UDP:
        m_serverEdit->setPlaceholderText("udp://192.168.1.100:1234");
        m_keyEdit->setPlaceholderText("Not used for UDP");
        m_keyEdit->setEnabled(false);
        m_testButton->setEnabled(false);
        break;
    case FLEXOUTPUT_OUTPUT_SRT:
        m_serverEdit->setPlaceholderText("srt://192.168.1.100:1234");
        m_keyEdit->setPlaceholderText("Not used for SRT");
        m_keyEdit->setEnabled(false);
        m_testButton->setEnabled(false);
        break;
    case FLEXOUTPUT_OUTPUT_WEBRTC:
        m_serverEdit->setPlaceholderText("https://webrtc.example.com/");
        m_keyEdit->setPlaceholderText("WebRTC session key...");
        m_testButton->setEnabled(true);
        break;
    case FLEXOUTPUT_OUTPUT_CUSTOM:
        m_serverEdit->setPlaceholderText("Custom output URL...");
        m_keyEdit->setPlaceholderText("Custom parameters...");
        m_testButton->setEnabled(false);
        break;
    }
}

void FlexOutputConfigDialog::onTestConnectionClicked()
{
    // Implement connection testing
    QMessageBox::information(this, "Test Connection", "Connection test not implemented yet.");
}

void FlexOutputConfigDialog::onResetToDefaultsClicked()
{
    // Reset to default values
    m_nameEdit->clear();
    m_typeCombo->setCurrentIndex(0);
    m_serverEdit->clear();
    m_keyEdit->clear();
    m_enabledCheckBox->setChecked(true);
    
    // Video defaults
    m_widthSpin->setValue(1920);
    m_heightSpin->setValue(1080);
    m_fpsSpin->setValue(30);
    m_bitrateSpin->setValue(2500);
    m_encoderCombo->setCurrentIndex(0);
    m_presetCombo->setCurrentText("veryfast");
    m_profileCombo->setCurrentText("high");
    
    // Audio defaults
    m_sampleRateSpin->setValue(44100);
    m_audioBitrateSpin->setValue(128);
    m_audioEncoderCombo->setCurrentText("AAC");
    m_channelsCombo->setCurrentText("Stereo");
    
    // Advanced defaults
    m_keyframeSpin->setValue(2);
    m_cbr_CheckBox->setChecked(false);
    m_customSettingsEdit->clear();
    
    onOutputTypeChanged();
}

void FlexOutputConfigDialog::onAccepted()
{
    validateSettings();
    if (m_isValid) {
        accept();
    }
}

void FlexOutputConfigDialog::onRejected()
{
    reject();
}

void FlexOutputConfigDialog::validateSettings()
{
    m_isValid = true;
    QString errors;
    
    // Validate name
    if (m_nameEdit->text().trimmed().isEmpty()) {
        errors += "- Output name cannot be empty\n";
        m_isValid = false;
    }
    
    // Validate server
    if (m_serverEdit->text().trimmed().isEmpty()) {
        errors += "- Server/URL cannot be empty\n";
        m_isValid = false;
    }
    
    // Validate stream key for RTMP
    flexoutput_output_type_t type = static_cast<flexoutput_output_type_t>(
        m_typeCombo->currentData().toInt());
    if (type == FLEXOUTPUT_OUTPUT_RTMP && m_keyEdit->text().trimmed().isEmpty()) {
        errors += "- Stream key is required for RTMP output\n";
        m_isValid = false;
    }
    
    // Validate video settings
    if (m_widthSpin->value() % 2 != 0) {
        errors += "- Video width must be even\n";
        m_isValid = false;
    }
    
    if (m_heightSpin->value() % 2 != 0) {
        errors += "- Video height must be even\n";
        m_isValid = false;
    }
    
    if (!m_isValid) {
        QMessageBox::warning(this, "Validation Error", 
            QString("Please fix the following errors:\n\n%1").arg(errors));
    }
}

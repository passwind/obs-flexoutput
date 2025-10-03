#include "flexoutput-status-widget.h"
#include "../flexoutput-types.h"
#include "../flexoutput-source.h"
#include "../flexoutput-output.h"
#include <obs.h>
#include <util/platform.h>
#include <QFileDialog>
#include <QDateTime>
#include <QTextStream>
#include <QFile>

#include "moc_flexoutput-status-widget.cpp"
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

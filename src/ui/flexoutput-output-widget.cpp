#include "flexoutput-output-widget.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QStringList>
#include <QTableWidgetItem>
#include <QMenu>
#include <QPoint>

extern "C" {
#include "../flexoutput-types.h"
#include "../flexoutput-output.h"
}

#include "moc_flexoutput-output-widget.cpp"

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

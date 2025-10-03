#include "flexoutput-source-list-widget.h"
#include "../flexoutput-source.h"
#include <obs.h>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QAbstractItemView>

#include "moc_flexoutput-source-list-widget.cpp"

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

void FlexOutputSourceListWidget::onSourceItemChanged()
{
    // Implementation for source item changes if needed
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

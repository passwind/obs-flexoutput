#include "flexoutput-mapping-widget.h"

#include <QSplitter>
#include <QAbstractItemView>
#include <QListWidgetItem>
#include <QList>

extern "C" {
#include "../flexoutput-types.h"
#include "../flexoutput-source.h"
#include "../flexoutput-output.h"
}

#include "moc_flexoutput-mapping-widget.cpp"

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

void FlexOutputMappingWidget::onSourceItemChanged()
{
    // Implementation for source item changes if needed
}

void FlexOutputMappingWidget::applyMappingChanges()
{
    // This would trigger the main plugin to update source mappings
    // The actual implementation would call the plugin's update function
}

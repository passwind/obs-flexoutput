#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSplitter>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QString>

extern "C" {
#include "../flexoutput-mapping.h"
}

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

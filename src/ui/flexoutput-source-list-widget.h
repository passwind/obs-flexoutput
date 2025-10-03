#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>

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

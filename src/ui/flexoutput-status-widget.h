#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

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

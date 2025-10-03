#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QString>

class FlexOutputOutputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FlexOutputOutputWidget(QWidget *parent = nullptr);
    ~FlexOutputOutputWidget();

    void updateOutputList();
    void setCurrentOutput(const QString &outputName);
    QString getCurrentOutput() const;

signals:
    void outputSelectionChanged(const QString &outputName);
    void addOutputRequested();
    void removeOutputRequested(const QString &outputName);
    void startOutputRequested(const QString &outputName);
    void stopOutputRequested(const QString &outputName);
    void configureOutputRequested(const QString &outputName);

private slots:
    void onOutputItemChanged();
    void onOutputDoubleClicked();
    void onContextMenuRequested(const QPoint &pos);

private:
    void setupUI();
    void updateOutputStatus();
    
    QVBoxLayout *m_layout;
    QHBoxLayout *m_buttonLayout;
    QTableWidget *m_outputTable;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_configureButton;
    QPushButton *m_refreshButton;
    
    QString m_currentOutput;
};

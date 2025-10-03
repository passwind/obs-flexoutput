#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include "../flexoutput-types.h"

class FlexOutputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FlexOutputConfigDialog(const QString &outputName, QWidget *parent = nullptr);
    ~FlexOutputConfigDialog();

    flexoutput_output_config_t getConfiguration() const;
    void setConfiguration(const flexoutput_output_config_t &config);

private slots:
    void onOutputTypeChanged();
    void onTestConnectionClicked();
    void onResetToDefaultsClicked();
    void onAccepted();
    void onRejected();

private:
    void setupUI();
    void setupOutputSettings();
    void setupVideoSettings();
    void setupAudioSettings();
    void setupAdvancedSettings();
    void updateUI();
    void validateSettings();
    
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    QHBoxLayout *m_buttonLayout;
    
    // Output settings
    QWidget *m_outputTab;
    QGridLayout *m_outputLayout;
    QLineEdit *m_nameEdit;
    QComboBox *m_typeCombo;
    QLineEdit *m_serverEdit;
    QLineEdit *m_keyEdit;
    QCheckBox *m_enabledCheckBox;
    QPushButton *m_testButton;
    
    // Video settings
    QWidget *m_videoTab;
    QGridLayout *m_videoLayout;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;
    QSpinBox *m_fpsSpin;
    QSpinBox *m_bitrateSpin;
    QComboBox *m_encoderCombo;
    QComboBox *m_presetCombo;
    QComboBox *m_profileCombo;
    
    // Audio settings
    QWidget *m_audioTab;
    QGridLayout *m_audioLayout;
    QSpinBox *m_sampleRateSpin;
    QSpinBox *m_audioBitrateSpin;
    QComboBox *m_audioEncoderCombo;
    QComboBox *m_channelsCombo;
    
    // Advanced settings
    QWidget *m_advancedTab;
    QGridLayout *m_advancedLayout;
    QSpinBox *m_keyframeSpin;
    QCheckBox *m_cbr_CheckBox;
    QLineEdit *m_customSettingsEdit;
    
    // Buttons
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    QPushButton *m_resetButton;
    
    QString m_outputName;
    flexoutput_output_config_t m_config;
    bool m_isValid;
};

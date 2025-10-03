#include "flexoutput-config-dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QWidget>

#include "moc_flexoutput-config-dialog.cpp"

FlexOutputConfigDialog::FlexOutputConfigDialog(const QString &outputName, QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_outputTab(nullptr)
    , m_videoTab(nullptr)
    , m_audioTab(nullptr)
    , m_advancedTab(nullptr)
    , m_outputLayout(nullptr)
    , m_videoLayout(nullptr)
    , m_audioLayout(nullptr)
    , m_advancedLayout(nullptr)
    , m_nameEdit(nullptr)
    , m_typeCombo(nullptr)
    , m_serverEdit(nullptr)
    , m_keyEdit(nullptr)
    , m_enabledCheckBox(nullptr)
    , m_widthSpin(nullptr)
    , m_heightSpin(nullptr)
    , m_fpsSpin(nullptr)
    , m_bitrateSpin(nullptr)
    , m_encoderCombo(nullptr)
    , m_presetCombo(nullptr)
    , m_profileCombo(nullptr)
    , m_sampleRateSpin(nullptr)
    , m_audioBitrateSpin(nullptr)
    , m_audioEncoderCombo(nullptr)
    , m_channelsCombo(nullptr)
    , m_keyframeSpin(nullptr)
    , m_cbr_CheckBox(nullptr)
    , m_customSettingsEdit(nullptr)
    , m_testButton(nullptr)
    , m_resetButton(nullptr)
    , m_buttonBox(nullptr)
    , m_isValid(false)
{
    setupUI();
    
    // Set the output name if provided
    if (m_nameEdit && !outputName.isEmpty()) {
        m_nameEdit->setText(outputName);
    }
}

FlexOutputConfigDialog::~FlexOutputConfigDialog()
{
}

void FlexOutputConfigDialog::setupUI()
{
    setWindowTitle("FlexOutput Configuration");
    setModal(true);
    resize(500, 400);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);
    
    setupOutputSettings();
    setupVideoSettings();
    setupAudioSettings();
    setupAdvancedSettings();
    
    // Button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_testButton = new QPushButton("Test Connection", this);
    m_resetButton = new QPushButton("Reset to Defaults", this);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_testButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_buttonBox);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &FlexOutputConfigDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &FlexOutputConfigDialog::onRejected);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FlexOutputConfigDialog::onOutputTypeChanged);
    connect(m_testButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onTestConnectionClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &FlexOutputConfigDialog::onResetToDefaultsClicked);
}

void FlexOutputConfigDialog::setupOutputSettings()
{
    m_outputTab = new QWidget();
    m_outputLayout = new QGridLayout(m_outputTab);
    
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Output name...");
    
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("RTMP Stream", static_cast<int>(FLEXOUTPUT_OUTPUT_RTMP));
    m_typeCombo->addItem("File Output", static_cast<int>(FLEXOUTPUT_OUTPUT_FILE));
    m_typeCombo->addItem("UDP Stream", static_cast<int>(FLEXOUTPUT_OUTPUT_UDP));
    m_typeCombo->addItem("SRT Stream", static_cast<int>(FLEXOUTPUT_OUTPUT_SRT));
    m_typeCombo->addItem("WebRTC", static_cast<int>(FLEXOUTPUT_OUTPUT_WEBRTC));
    m_typeCombo->addItem("Custom", static_cast<int>(FLEXOUTPUT_OUTPUT_CUSTOM));
    
    m_serverEdit = new QLineEdit(this);
    m_serverEdit->setPlaceholderText("Server URL...");
    
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setPlaceholderText("Stream key...");
    m_keyEdit->setEchoMode(QLineEdit::Password);
    
    m_enabledCheckBox = new QCheckBox("Enable this output", this);
    m_enabledCheckBox->setChecked(true);
    
    m_outputLayout->addWidget(new QLabel("Name:"), 0, 0);
    m_outputLayout->addWidget(m_nameEdit, 0, 1);
    
    m_outputLayout->addWidget(new QLabel("Type:"), 1, 0);
    m_outputLayout->addWidget(m_typeCombo, 1, 1);
    
    m_outputLayout->addWidget(new QLabel("Server:"), 2, 0);
    m_outputLayout->addWidget(m_serverEdit, 2, 1);
    
    m_outputLayout->addWidget(new QLabel("Stream Key:"), 3, 0);
    m_outputLayout->addWidget(m_keyEdit, 3, 1);
    
    m_outputLayout->addWidget(m_enabledCheckBox, 4, 0, 1, 2);
    
    m_tabWidget->addTab(m_outputTab, "Output");
}

void FlexOutputConfigDialog::setupVideoSettings()
{
    m_videoTab = new QWidget();
    m_videoLayout = new QGridLayout(m_videoTab);
    
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(128, 7680);
    m_widthSpin->setValue(1920);
    m_widthSpin->setSingleStep(2);
    
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(128, 4320);
    m_heightSpin->setValue(1080);
    m_heightSpin->setSingleStep(2);
    
    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(1, 120);
    m_fpsSpin->setValue(30);
    
    m_bitrateSpin = new QSpinBox(this);
    m_bitrateSpin->setRange(100, 50000);
    m_bitrateSpin->setValue(2500);
    m_bitrateSpin->setSuffix(" kbps");
    
    m_encoderCombo = new QComboBox(this);
    m_encoderCombo->addItems({"x264", "x265", "NVENC", "QuickSync", "AMF"});
    
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItems({"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"});
    m_presetCombo->setCurrentText("veryfast");
    
    m_profileCombo = new QComboBox(this);
    m_profileCombo->addItems({"baseline", "main", "high"});
    m_profileCombo->setCurrentText("high");
    
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
    m_audioEncoderCombo->addItems({"AAC", "MP3", "Opus"});
    
    m_channelsCombo = new QComboBox(this);
    m_channelsCombo->addItem("Mono", 1);
    m_channelsCombo->addItem("Stereo", 2);
    m_channelsCombo->addItem("5.1", 6);
    m_channelsCombo->setCurrentText("Stereo");
    
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

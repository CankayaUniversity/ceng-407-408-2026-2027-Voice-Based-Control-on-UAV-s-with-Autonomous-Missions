#include "AudioDeviceManagerTest.h"

#include <QtTest/QSignalSpy>

#include "AudioDeviceManager.h"
#include "Fact.h"

void AudioDeviceManagerTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.multimedia.pipewire.registry", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Failed to connect to pipewire")));
    ignoreLogMessage("default", QtWarningMsg, QRegularExpression(QStringLiteral("PulseAudioService|secure directory")));
}

void AudioDeviceManagerTest::cleanup()
{
    AudioDeviceManager::instance()->init(nullptr);
    UnitTest::cleanup();
}

void AudioDeviceManagerTest::_testInitialState()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    QVERIFY(manager != nullptr);

    const QStringList mics = manager->availableMicrophones();
    QCOMPARE(mics.count(), manager->availableMicrophones().count());
}

void AudioDeviceManagerTest::_testAvailableMicrophones()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    const QStringList mics = manager->availableMicrophones();
    for (const QString& mic : mics) {
        QVERIFY(!mic.isEmpty());
    }
}

void AudioDeviceManagerTest::_testSelectionAndPersistence()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    Fact fact(0, QStringLiteral("selectedMicrophone"), FactMetaData::valueTypeString, this);
    fact.setRawValue(QString());

    manager->init(&fact);

    const QStringList mics = manager->availableMicrophones();
    if (!mics.isEmpty()) {
        const QString targetMic = mics.first();
        manager->setSelectedMicrophone(targetMic);

        QCOMPARE(manager->selectedMicrophone(), targetMic);
        QCOMPARE(fact.rawValue().toString(), targetMic);
        QCOMPARE(manager->selectedMicrophoneIndex(), 0);
        QVERIFY(manager->isMicrophoneActive());
        QVERIFY(!manager->selectedAudioDevice().isNull());
        QCOMPARE(manager->selectedAudioDevice().description(), targetMic);

        manager->setSelectedMicrophoneIndex(0);
        QCOMPARE(manager->selectedMicrophone(), targetMic);
    }
}

void AudioDeviceManagerTest::_testInvalidDeviceState()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    Fact fact(0, QStringLiteral("selectedMicrophone"), FactMetaData::valueTypeString, this);

    const QString invalidMicName = QStringLiteral("__NonExistent_Microphone_XYZ__");
    fact.setRawValue(invalidMicName);
    manager->init(&fact);

    // Setting an invalid microphone must be maintained internally without crash
    manager->setSelectedMicrophone(invalidMicName);
    QCOMPARE(manager->selectedMicrophone(), invalidMicName);
    QCOMPARE(fact.rawValue().toString(), invalidMicName);

    // Invalid state should be reflected internally:
    QVERIFY(!manager->isMicrophoneActive());
    QVERIFY(manager->selectedAudioDevice().isNull());
    QCOMPARE(manager->selectedMicrophoneIndex(), -1);
    QCOMPARE(manager->sampleRate(), 0);
    QCOMPARE(manager->channelCount(), 0);
    QVERIFY(manager->sampleFormat().isEmpty());
    QVERIFY(manager->deviceId().isEmpty());
    QVERIFY(!manager->isDefaultDevice());
}

void AudioDeviceManagerTest::_testSignalEmissions()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    QSignalSpy spySelectedMic(manager, &AudioDeviceManager::selectedMicrophoneChanged);
    QSignalSpy spySelectedIndex(manager, &AudioDeviceManager::selectedMicrophoneIndexChanged);
    QSignalSpy spyActive(manager, &AudioDeviceManager::isMicrophoneActiveChanged);
    QSignalSpy spySpecs(manager, &AudioDeviceManager::specsChanged);

    const QString dummyName1 = QStringLiteral("__Dummy_Mic_1__");
    manager->setSelectedMicrophone(dummyName1);

    QVERIFY(spySelectedMic.count() >= 1);
    QVERIFY(spySelectedIndex.count() >= 1);
    QVERIFY(spySpecs.count() >= 1);

    const QString dummyName2 = QStringLiteral("__Dummy_Mic_2__");
    manager->setSelectedMicrophone(dummyName2);

    QVERIFY(spySelectedMic.count() >= 2);
    QVERIFY(spySelectedIndex.count() >= 2);
}

void AudioDeviceManagerTest::_testSpecs()
{
    AudioDeviceManager* manager = AudioDeviceManager::instance();
    const QStringList mics = manager->availableMicrophones();

    if (!mics.isEmpty()) {
        manager->setSelectedMicrophone(mics.first());
        if (manager->isMicrophoneActive()) {
            QVERIFY(manager->sampleRate() > 0);
            QVERIFY(manager->channelCount() > 0);
            QVERIFY(!manager->sampleFormat().isEmpty());
            QVERIFY(!manager->deviceId().isEmpty());
        }
    }
}

UT_REGISTER_TEST(AudioDeviceManagerTest, TestLabel::Unit, TestLabel::Utilities)

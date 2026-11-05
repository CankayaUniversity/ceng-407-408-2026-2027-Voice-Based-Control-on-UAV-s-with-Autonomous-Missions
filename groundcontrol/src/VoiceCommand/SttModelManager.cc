#include "SttModelManager.h"

#include <QtCore/QCoreApplication>
#include <algorithm>

#include "Fact.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(SttModelManagerLog, "VoiceCommand.SttModelManager")

Q_APPLICATION_STATIC(SttModelManager, _sttModelManager);

SttModelManager::SttModelManager(QObject* parent) : QObject(parent)
{
    refreshModels();
}

SttModelManager* SttModelManager::instance()
{
    return _sttModelManager();
}

void SttModelManager::init(Fact* selectedModelFact)
{
    if (_selectedModelFact) {
        disconnect(_selectedModelFact, &Fact::rawValueChanged, this, &SttModelManager::_onSelectedModelFactChanged);
    }

    _selectedModelFact = selectedModelFact;

    if (_selectedModelFact) {
        _selectedModelName = _selectedModelFact->rawValue().toString().trimmed();
        (void) connect(_selectedModelFact, &Fact::rawValueChanged, this,
                       &SttModelManager::_onSelectedModelFactChanged);
    }

    refreshModels();
}

QStringList SttModelManager::availableModels() const
{
    QStringList names;
    names.reserve(_discoveredModels.size());
    for (const SttModelDescriptor& desc : _discoveredModels) {
        names.append(desc.displayName);
    }
    return names;
}

QString SttModelManager::selectedModel() const
{
    return _selectedModelName;
}

void SttModelManager::setSelectedModel(const QString& modelIdOrName)
{
    const QString trimmed = modelIdOrName.trimmed();
    if (_selectedModelName == trimmed) {
        return;
    }

    _selectedModelName = trimmed;
    _updateSelectedModel();
}

int SttModelManager::selectedModelIndex() const
{
    return _selectedModelIndex;
}

void SttModelManager::setSelectedModelIndex(int index)
{
    if (index < 0 || index >= _discoveredModels.size()) {
        return;
    }
    setSelectedModel(_discoveredModels.at(index).id);
}

bool SttModelManager::isModelAvailable() const
{
    return !_discoveredModels.isEmpty();
}

void SttModelManager::refreshModels()
{
    const bool prevAvailable = isModelAvailable();

    _discoveredModels = SttModelResolver::discoverModels(QString(), _searchOverrideDir,
                                                         _isolateSearchToOverride);

    emit availableModelsChanged();
    _updateSelectedModel();

    if (prevAvailable != isModelAvailable()) {
        emit isModelAvailableChanged(isModelAvailable());
    }
}

SttConfig SttModelManager::currentResolvedConfig(int languageSettingIndex, bool useGpu) const
{
    return SttModelResolver::resolveConfig(languageSettingIndex, _selectedModelName, useGpu,
                                           QString(), _searchOverrideDir, _isolateSearchToOverride);
}

void SttModelManager::setSearchOverride(const QString& envDirOverride, bool isolateToOverrides)
{
    _searchOverrideDir = envDirOverride;
    _isolateSearchToOverride = isolateToOverrides;
    refreshModels();
}

void SttModelManager::_onSelectedModelFactChanged()
{
    if (!_selectedModelFact) {
        return;
    }

    const QString newModel = _selectedModelFact->rawValue().toString().trimmed();
    if (newModel != _selectedModelName) {
        _selectedModelName = newModel;
        _updateSelectedModel();
    }
}

void SttModelManager::_updateSelectedModel()
{
    if (_discoveredModels.isEmpty()) {
        if (_selectedModelIndex != -1) {
            _selectedModelIndex = -1;
            emit selectedModelIndexChanged(_selectedModelIndex);
        }
        return;
    }

    int matchedIdx = -1;
    if (!_selectedModelName.isEmpty()) {
        for (int i = 0; i < _discoveredModels.size(); ++i) {
            const SttModelDescriptor& desc = _discoveredModels.at(i);
            if (desc.id.compare(_selectedModelName, Qt::CaseInsensitive) == 0 ||
                desc.displayName.compare(_selectedModelName, Qt::CaseInsensitive) == 0) {
                matchedIdx = i;
                break;
            }
        }
    }

    matchedIdx = std::max(matchedIdx, 0);

    const QString resolvedId = _discoveredModels.at(matchedIdx).id;
    const bool nameChanged = (_selectedModelName != resolvedId);
    const bool indexChanged = (_selectedModelIndex != matchedIdx);

    _selectedModelName = resolvedId;
    _selectedModelIndex = matchedIdx;

    if (_selectedModelFact && _selectedModelFact->rawValue().toString() != _selectedModelName) {
        _selectedModelFact->setRawValue(_selectedModelName);
    }

    if (nameChanged) {
        emit selectedModelChanged(_selectedModelName);
    }
    if (indexChanged) {
        emit selectedModelIndexChanged(_selectedModelIndex);
    }
}

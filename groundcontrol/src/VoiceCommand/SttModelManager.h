#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "SttModelResolver.h"

class Fact;

/// \brief SttModelManager manages the discovered STT models list, active selection,
/// Settings Fact persistence, and availability state
///
class SttModelManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SttModelManager)
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged)
    Q_PROPERTY(
        int selectedModelIndex READ selectedModelIndex WRITE setSelectedModelIndex NOTIFY selectedModelIndexChanged)
    Q_PROPERTY(bool isModelAvailable READ isModelAvailable NOTIFY isModelAvailableChanged)

public:
    explicit SttModelManager(QObject* parent = nullptr);
    ~SttModelManager() override = default;

    static SttModelManager* instance();

    /// Initializes Fact bindings for selected model, then refreshes models.
    void init(Fact* selectedModelFact = nullptr);

    /// Human-readable display names of dynamically discovered STT models on disk.
    QStringList availableModels() const;

    /// Currently selected STT model identifier/filename.
    QString selectedModel() const;
    void setSelectedModel(const QString& modelIdOrName);

    /// Index of the selected model in availableModels (-1 if no models found).
    int selectedModelIndex() const;
    void setSelectedModelIndex(int index);

    /// True if at least one valid STT model is available on disk.
    bool isModelAvailable() const;

    /// Re-scans the model directory cascade via SttModelResolver and updates selection/availability.
    Q_INVOKABLE void refreshModels();

    /// Resolves the active SttConfig for the current selection and given language/GPU settings.
    SttConfig currentResolvedConfig(int languageSettingIndex, bool useGpu) const;

    /// Test helper to override/isolate the model search directory.
    void setSearchOverride(const QString& envDirOverride, bool isolateToOverrides = true);

signals:
    void availableModelsChanged();
    void selectedModelChanged(const QString& model);
    void selectedModelIndexChanged(int index);
    void isModelAvailableChanged(bool available);

private slots:
    void _onSelectedModelFactChanged();

private:
    void _updateSelectedModel();

    QPointer<Fact> _selectedModelFact;

    QString _selectedModelName;
    QList<SttModelDescriptor> _discoveredModels;
    int _selectedModelIndex = -1;

    QString _searchOverrideDir;
    bool _isolateSearchToOverride = false;
};

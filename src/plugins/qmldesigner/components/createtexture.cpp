// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "createtexture.h"

#include "abstractview.h"
#include "documentmanager.h"
#include "modelnodeoperations.h"
#include "nodelistproperty.h"
#include "nodemetainfo.h"
#include "qmlobjectnode.h"
#include "variantproperty.h"

#include <coreplugin/messagebox.h>

#include <QTimer>

namespace QmlDesigner {

CreateTexture::CreateTexture(AbstractView *view, bool importFile)
    : m_view{view}
    , m_importFile{importFile}
{}

void CreateTexture::execute(const QString &filePath, AddTextureMode mode, int sceneId)
{
    if (m_importFile && !addFileToProject(filePath))
        return;

    ModelNode texture = createTextureFromImage(filePath, mode);
    if (!texture.isValid())
        return;

    if (mode == AddTextureMode::LightProbe && sceneId != -1)
        assignTextureAsLightProbe(texture, sceneId);

    QTimer::singleShot(0, m_view, [this, texture]() {
        if (m_view->model())
            m_view->emitCustomNotification("selected_texture_changed", {texture});
    });
}

bool CreateTexture::addFileToProject(const QString &filePath)
{
    AddFilesResult result = ModelNodeOperations::addImageToProject(
                {filePath}, ModelNodeOperations::getImagesDefaultDirectory().toString(), false);

    if (result.status() == AddFilesResult::Failed) {
        Core::AsynchronousMessageBox::warning(QObject::tr("Failed to Add Texture"),
                                              QObject::tr("Could not add %1 to project.").arg(filePath));
        return false;
    }

    return true;
}

ModelNode CreateTexture::createTextureFromImage(const QString &assetPath, AddTextureMode mode)
{
    if (mode != AddTextureMode::Texture && mode != AddTextureMode::LightProbe)
        return {};

    ModelNode matLib = m_view->materialLibraryNode();
    if (!matLib.isValid())
        return {};

    NodeMetaInfo metaInfo = m_view->model()->qtQuick3DTextureMetaInfo();

    Utils::FilePath imagePath = ModelNodeOperations::getImagesDefaultDirectory()
            .pathAppended(Utils::FilePath::fromString(assetPath).fileName());
    QString sourceVal = imagePath.relativePathFrom(
            QmlDesigner::DocumentManager::currentFilePath()).toString();

    ModelNode newTexNode = m_view->getTextureDefaultInstance(sourceVal);
    if (!newTexNode.isValid()) {
        newTexNode = m_view->createModelNode("QtQuick3D.Texture",
                                             metaInfo.majorVersion(),
                                             metaInfo.minorVersion());
        newTexNode.validId();
        VariantProperty sourceProp = newTexNode.variantProperty("source");
        sourceProp.setValue(sourceVal);
        matLib.defaultNodeListProperty().reparentHere(newTexNode);
    }

    return newTexNode;
}

void CreateTexture::assignTextureAsLightProbe(const ModelNode &texture, int sceneId)
{
    ModelNode sceneEnvNode = resolveSceneEnv(sceneId);
    QmlObjectNode sceneEnv = sceneEnvNode;
    if (sceneEnv.isValid()) {
        sceneEnv.setBindingProperty("lightProbe", texture.id());
        sceneEnv.setVariantProperty("backgroundMode",
                                    QVariant::fromValue(Enumeration("SceneEnvironment",
                                                                    "SkyBox")));
    }
}

ModelNode CreateTexture::resolveSceneEnv(int sceneId)
{
    ModelNode activeSceneEnv;

    if (sceneId != -1) {
        ModelNode activeScene = m_view->active3DSceneNode();
        if (activeScene.isValid()) {
            QmlObjectNode view3D;
            if (activeScene.metaInfo().isQtQuick3DView3D()) {
                view3D = activeScene;
            } else {
                ModelNode sceneParent = activeScene.parentProperty().parentModelNode();
                if (sceneParent.metaInfo().isQtQuick3DView3D())
                    view3D = sceneParent;
            }
            if (view3D.isValid())
                activeSceneEnv = m_view->modelNodeForId(view3D.expression("environment"));
        }
    }

    return activeSceneEnv;
}

} // namespace QmlDesigner

/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "materialbrowsermodel.h"

#include <bindingproperty.h>
#include <designmodewidget.h>
#include <qmldesignerplugin.h>
#include <qmlobjectnode.h>
#include "variantproperty.h"
#include "utils/qtcassert.h"

namespace QmlDesigner {

MaterialBrowserModel::MaterialBrowserModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

MaterialBrowserModel::~MaterialBrowserModel()
{
}

int MaterialBrowserModel::rowCount(const QModelIndex &) const
{
    return m_materialList.count();
}

QVariant MaterialBrowserModel::data(const QModelIndex &index, int role) const
{
    QTC_ASSERT(index.isValid() && index.row() < m_materialList.count(), return {});
    QTC_ASSERT(roleNames().contains(role), return {});

    QByteArray roleName = roleNames().value(role);
    if (roleName == "materialName") {
        QVariant objName = m_materialList.at(index.row()).variantProperty("objectName").value();
        return objName.isValid() ? objName : "";
    }

    if (roleName == "materialInternalId")
        return m_materialList.at(index.row()).internalId();

    if (roleName == "materialVisible")
        return isMaterialVisible(index.row());

    if (roleName == "materialType") {
        QString matType = QString::fromLatin1(m_materialList.at(index.row()).type());
        if (matType.startsWith("QtQuick3D."))
            matType.remove("QtQuick3D.");
        return matType;
    }

    return {};
}

bool MaterialBrowserModel::isMaterialVisible(int idx) const
{
    if (!isValidIndex(idx))
        return false;

    return m_searchText.isEmpty() || m_materialList.at(idx).variantProperty("objectName")
            .value().toString().contains(m_searchText, Qt::CaseInsensitive);
}

bool MaterialBrowserModel::isValidIndex(int idx) const
{
    return idx > -1 && idx < rowCount();
}

/**
 * @brief Loads and parses propertyGroups.json from QtQuick3D module's designer folder
 *
 * propertyGroups.json contains lists of QtQuick3D objects' properties grouped by sections
 *
 * @param path path to propertyGroups.json file
 * @return load successful
 */
bool MaterialBrowserModel::loadPropertyGroups(const QString &path)
{
    bool ok = true;

    if (m_propertyGroupsObj.isEmpty()) {
        QFile matPropsFile(path);

        if (!matPropsFile.open(QIODevice::ReadOnly)) {
            qWarning("Couldn't open propertyGroups.json");
            ok = false;
        }

        if (ok) {
            QJsonDocument matPropsJsonDoc = QJsonDocument::fromJson(matPropsFile.readAll());
            if (matPropsJsonDoc.isNull()) {
                qWarning("Invalid propertyGroups.json file");
                ok = false;
            } else {
                m_propertyGroupsObj = matPropsJsonDoc.object();
            }
        }
    }

    m_defaultMaterialSections.clear();
    m_principledMaterialSections.clear();
    m_customMaterialSections.clear();
    if (ok) {
        m_defaultMaterialSections.append(m_propertyGroupsObj.value("DefaultMaterial").toObject().keys());
        m_principledMaterialSections.append(m_propertyGroupsObj.value("PrincipledMaterial").toObject().keys());

        QStringList customMatSections = m_propertyGroupsObj.value("CustomMaterial").toObject().keys();
        if (customMatSections.size() > 1) // as of now custom material has only 1 section, so we don't add it
            m_customMaterialSections.append(customMatSections);
    }
    emit materialSectionsChanged();

    return ok;
}

QHash<int, QByteArray> MaterialBrowserModel::roleNames() const
{
    static const QHash<int, QByteArray> roles {
        {Qt::UserRole + 1, "materialName"},
        {Qt::UserRole + 2, "materialInternalId"},
        {Qt::UserRole + 3, "materialVisible"},
        {Qt::UserRole + 4, "materialType"}
    };
    return roles;
}

bool MaterialBrowserModel::hasQuick3DImport() const
{
    return m_hasQuick3DImport;
}

void MaterialBrowserModel::setHasQuick3DImport(bool b)
{
    if (b == m_hasQuick3DImport)
        return;

    m_hasQuick3DImport = b;
    emit hasQuick3DImportChanged();
}

bool MaterialBrowserModel::hasModelSelection() const
{
    return m_hasModelSelection;
}

void MaterialBrowserModel::setHasModelSelection(bool b)
{
    if (b == m_hasModelSelection)
        return;

    m_hasModelSelection = b;
    emit hasModelSelectionChanged();
}

bool MaterialBrowserModel::hasMaterialRoot() const
{
    return m_hasMaterialRoot;
}

void MaterialBrowserModel::setHasMaterialRoot(bool b)
{
    if (m_hasMaterialRoot == b)
        return;

    m_hasMaterialRoot = b;
    emit hasMaterialRootChanged();
}

QString MaterialBrowserModel::copiedMaterialType() const
{
    return m_copiedMaterialType;
}

void MaterialBrowserModel::setCopiedMaterialType(const QString &matType)
{
    if (matType == m_copiedMaterialType)
        return;

    m_copiedMaterialType = matType;
    emit copiedMaterialTypeChanged();
}

QList<ModelNode> MaterialBrowserModel::materials() const
{
    return m_materialList;
}

void MaterialBrowserModel::setSearchText(const QString &searchText)
{
    QString lowerSearchText = searchText.toLower();

    if (m_searchText == lowerSearchText)
        return;

    m_searchText = lowerSearchText;

    bool isEmpty = false;

    // if selected material goes invisible, select nearest material
    if (!isMaterialVisible(m_selectedIndex)) {
        int inc = 1;
        int incCap = m_materialList.count();
        while (!isEmpty && inc < incCap) {
            if (isMaterialVisible(m_selectedIndex - inc)) {
                selectMaterial(m_selectedIndex - inc);
                break;
            } else if (isMaterialVisible(m_selectedIndex + inc)) {
                selectMaterial(m_selectedIndex + inc);
                break;
            }
            ++inc;
            isEmpty = !isValidIndex(m_selectedIndex + inc)
                   && !isValidIndex(m_selectedIndex - inc);
        }
    }

    if (isEmpty != m_isEmpty) {
        m_isEmpty = isEmpty;
        emit isEmptyChanged();
    }

    resetModel();
}

void MaterialBrowserModel::setMaterials(const QList<ModelNode> &materials, bool hasQuick3DImport)
{
    m_materialList = materials;
    m_materialIndexHash.clear();
    for (int i = 0; i < materials.size(); ++i)
        m_materialIndexHash.insert(materials.at(i).internalId(), i);

    bool isEmpty = materials.size() == 0;
    if (isEmpty != m_isEmpty) {
        m_isEmpty = isEmpty;
        emit isEmptyChanged();
    }

    setHasQuick3DImport(hasQuick3DImport);
    updateSelectedMaterial();
    resetModel();
}

void MaterialBrowserModel::removeMaterial(const ModelNode &material)
{
    if (!m_materialIndexHash.contains(material.internalId()))
        return;

    m_materialList.removeOne(material);
    int idx = m_materialIndexHash.value(material.internalId());
    m_materialIndexHash.remove(material.internalId());

    // update index hash
    for (int i = idx; i < rowCount(); ++i)
        m_materialIndexHash.insert(m_materialList.at(i).internalId(), i);

    resetModel();

    if (m_materialList.isEmpty()) {
        m_isEmpty = true;
        emit isEmptyChanged();
    }
}

void MaterialBrowserModel::deleteSelectedMaterial()
{
    if (isValidIndex(m_selectedIndex))
        m_materialList[m_selectedIndex].destroy();
}

void MaterialBrowserModel::updateSelectedMaterial()
{
    selectMaterial(m_selectedIndex, true);
}

void MaterialBrowserModel::updateMaterialName(const ModelNode &material)
{
    int idx = materialIndex(material);
    if (idx != -1)
        emit dataChanged(index(idx, 0), index(idx, 0), {roleNames().key("materialName")});
}

int MaterialBrowserModel::materialIndex(const ModelNode &material) const
{
    if (m_materialIndexHash.contains(material.internalId()))
        return m_materialIndexHash.value(material.internalId());

    return -1;
}

ModelNode MaterialBrowserModel::materialAt(int idx) const
{
    if (isValidIndex(idx))
        return m_materialList.at(idx);

    return {};
}

void MaterialBrowserModel::resetModel()
{
    beginResetModel();
    endResetModel();
}

void MaterialBrowserModel::selectMaterial(int idx, bool force)
{
    if (m_materialList.size() == 0) {
        m_selectedIndex = -1;
        emit selectedIndexChanged(m_selectedIndex);
        return;
    }

    idx = std::max(0, std::min(idx, rowCount() - 1));

    if (idx != m_selectedIndex || force) {
        m_selectedIndex = idx;
        emit selectedIndexChanged(idx);
    }
}

void MaterialBrowserModel::duplicateMaterial(int idx)
{
    emit duplicateMaterialTriggered(m_materialList.at(idx));
}

void MaterialBrowserModel::copyMaterialProperties(int idx, const QString &section)
{
    ModelNode mat = m_materialList.at(idx);
    QString matType = QString::fromLatin1(mat.type());

    if (matType.startsWith("QtQuick3D."))
        matType.remove("QtQuick3D.");

    setCopiedMaterialType(matType);
    m_allPropsCopied = section == "All";

    if (m_allPropsCopied || m_propertyGroupsObj.empty()) {
        m_copiedMaterialProps = mat.properties();
    } else {
        QJsonObject propsSpecObj = m_propertyGroupsObj.value(m_copiedMaterialType).toObject();
        if (propsSpecObj.contains(section)) { // should always be true
           m_copiedMaterialProps.clear();
           const QJsonArray propNames = propsSpecObj.value(section).toArray();
           for (const QJsonValueRef &propName : propNames)
               m_copiedMaterialProps.append(mat.property(propName.toString().toLatin1()));

           if (section == "Base") { // add QtQuick3D.Material base props as well
               QJsonObject propsMatObj = m_propertyGroupsObj.value("Material").toObject();
               const QJsonArray propNames = propsMatObj.value("Base").toArray();
               for (const QJsonValueRef &propName : propNames)
                   m_copiedMaterialProps.append(mat.property(propName.toString().toLatin1()));
           }
        }
    }
}

void MaterialBrowserModel::pasteMaterialProperties(int idx)
{
    emit pasteMaterialPropertiesTriggered(m_materialList.at(idx), m_copiedMaterialProps, m_allPropsCopied);
}

void MaterialBrowserModel::deleteMaterial(int idx)
{
    m_materialList[idx].destroy();
}

void MaterialBrowserModel::renameMaterial(int idx, const QString &newName)
{
    ModelNode mat = m_materialList.at(idx);
    emit renameMaterialTriggered(mat, newName);
}

void MaterialBrowserModel::addNewMaterial()
{
    emit addNewMaterialTriggered();
}

void MaterialBrowserModel::applyToSelected(qint64 internalId, bool add)
{
    int idx = m_materialIndexHash.value(internalId);
    if (idx != -1) {
        ModelNode mat = m_materialList.at(idx);
        emit applyToSelectedTriggered(mat, add);
    }
}

void MaterialBrowserModel::openMaterialEditor()
{
    QmlDesignerPlugin::instance()->mainWidget()->showDockWidget("MaterialEditor", true);
}

} // namespace QmlDesigner

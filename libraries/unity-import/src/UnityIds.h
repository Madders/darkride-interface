//
//  UnityIds.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnityIds_h
#define hifi_UnityIds_h

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace unity {

// The local file ID Unity gives a class compiled into a DLL: the first 4 bytes (little-endian) of
// MD4("s\0\0\0" + namespace + class name). A MonoBehaviour names its script by this ID and the DLL's GUID,
// so an avatar package's components are known by their script IDs alone.
qint32 dllScriptFileID(const QString& nameSpace, const QString& className);

// The avatar component class a MonoBehaviour's m_Script fileID stands for ("VRCPhysBone", ...), or an empty string.
QString componentClassName(qint64 scriptFileID);

// Unity's name for a class ID: 1 GameObject, 4 Transform, 114 MonoBehaviour, 137 SkinnedMeshRenderer, ...
QString classIDName(int classID);

// The local file ID Unity (2019.3 and later: a model's "fileIdsGeneration: 2") gives each object it makes from a model
// file: xxHash64, seed 0, of the UTF-8 text "Type:<class>-><path><n>", read as a signed number; n counts earlier
// objects with the same text, 0 for the first. A GameObject's path is "//RootNode/root" for the model's root object,
// then the node names down the hierarchy ("//RootNode/root/Armature/Hips"); a component's is its GameObject's path,
// "/" and its class ("//RootNode/root/Body/SkinnedMeshRenderer"); a resource's (a mesh, a material embedded in the
// model) is just its name, with no "//" ("Type:Mesh->Body0"; every .obj's mesh is "Type:Mesh->default0",
// -2432090755550338912, per Lyuma). A prefab names an
// object in its model by this ID. (Lyuma's hint, 2026-10-14; it gives all 459 model objects Unity 6000.3 lists for a
// test avatar, which has no repeated names, so n > 0 is unchecked; Lyuma's Unidot importer counts n per text in the
// order the model's nodes are visited, depth first.) A model with a single
// top-level node is still rooted at "//RootNode/root": that node becomes the root and its own name leaves the paths.
// A model first imported before Unity 2019.3 (fileIdsGeneration 1) keeps its IDs in its .meta instead, in
// internalIDToNameTable (or fileIDToRecycleName), by object name: look them up there. A prefab names the model itself
// (m_SourcePrefab) by 100100000.
qint64 modelObjectID(const QString& className, const QString& path, int n = 0);
QString modelNodePath(const QStringList& namesBelowRoot);  // {} gives the root, {"Armature", "Hips"} its Hips
qint64 modelGameObjectID(const QString& nodePath, int n = 0);
qint64 modelComponentID(const QString& className, const QString& nodePath, int n = 0);
qint64 modelMeshID(const QString& meshName, int n = 0);  // other resources: modelObjectID("Material", name)
// The model's Avatar (its humanoid description) keeps the old fixed ID: class 90 * 100000.
const qint64 MODEL_AVATAR_ID = 9000000;

quint64 xxHash64(const QByteArray& data, quint64 seed = 0);

}

#endif // hifi_UnityIds_h

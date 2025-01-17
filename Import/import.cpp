#include <assert.h>
#include <rapidxml_ext.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <functional>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <string>
#include <filesystem>
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/DefaultLogger.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <Util.h>
#include <ToolKit.h>
#include <rapidxml.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using std::string;
using std::vector;
using std::unordered_map;
using std::to_string;
using std::ifstream;
using std::ios;
using std::fstream;
using std::ofstream;
using std::cout;
using std::endl;
namespace fs = std::filesystem;

char GetPathSeparator()
{
  static char sep = '/';
  const wchar_t pref = fs::path::preferred_separator;
  wcstombs(&sep, &pref, 1);
  return sep;
}

void NormalizePath(string& path)
{
  fs::path patify = path;
  path = patify.lexically_normal().u8string();
}

void TrunckToFileName(string& fullPath)
{
  fs::path patify = fullPath;
  fullPath = patify.filename().u8string();
}

namespace ToolKit
{

  class BoneNode
  {
   public:
    BoneNode()
    {
    }

    BoneNode(aiNode* node, unsigned int index)
    {
      boneIndex = index;
      boneNode = node;
    }

    aiNode* boneNode = nullptr;
    aiBone* bone = nullptr;
    unsigned int boneIndex = 0;
  };

  vector<string> g_usedFiles;
  bool IsUsed(string file)
  {
    return find
    (
      g_usedFiles.begin(),
      g_usedFiles.end(),
      file
    ) == g_usedFiles.end();
  }

  void AddToUsedFiles(string file)
  {
    // Add unique.
    if (IsUsed(file))
    {
      g_usedFiles.push_back(file);
    }
  }

  void ClearForbidden(std::string& str)
  {
    const std::string forbiddenChars = "\\/:?\"<>|";
    std::replace_if
    (
      str.begin(),
      str.end(),
      [&forbiddenChars] (char c)
      {
        return std::string::npos != forbiddenChars.find(c);
      },
      ' '
    );
  }

  unordered_map<string, BoneNode> g_skeletonMap;
  SkeletonPtr g_skeleton;
  const aiScene* g_scene = nullptr;
  vector<ULongID> g_entityIds;
  uint g_idListIterationIndex;  // Used to iterate over g_entityIds

  void Decompose(string fullPath, string& path, string& name)
  {
    NormalizePath(fullPath);
    path = "";

    size_t i = fullPath.find_last_of(GetPathSeparator());
    if (i != string::npos)
    {
      path = fullPath.substr(0, fullPath.find_last_of(GetPathSeparator()) + 1);
    }

    name = fullPath.substr(fullPath.find_last_of(GetPathSeparator()) + 1);
    name = name.substr(0, name.find_last_of('.'));
  }

  void DecomposeAssimpMatrix
  (
    aiMatrix4x4 transform,
    Vec3* t,
    Quaternion* r,
    Vec3* s
  )
  {
    aiVector3D aiT, aiS;
    aiQuaternion aiR;
    transform.Decompose(aiS, aiR, aiT);

    *t = Vec3(aiT.x, aiT.y, aiT.z);
    *r = Quaternion(aiR.w, aiR.x, aiR.y, aiR.z);
    *s = Vec3(aiS.x, aiS.y, aiS.z);
  }

  string GetEmbeddedTextureName(const aiTexture* texture)
  {
    string name = texture->mFilename.C_Str();
    NormalizePath(name);
    name = name + "." + texture->achFormatHint;

    return name;
  }

  string GetMaterialName(aiMaterial* material, unsigned int indx)
  {
    string name = material->GetName().C_Str();
    if (name.empty())
    {
      name = "emb" + to_string(indx);
    }

    return name;
  }

  string GetMaterialName(aiMesh* mesh)
  {
    aiString matName;
    g_scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, matName);
    return GetMaterialName
    (
      g_scene->mMaterials[mesh->mMaterialIndex],
      mesh->mMaterialIndex
    );
  }


  std::vector<MaterialPtr> tMaterials;
  template<typename T>
  void CreateFileAndSerializeObject
  (
    std::shared_ptr<T> objectToSerialize,
    const String& filePath
  )
  {
    std::ofstream file;
    file.open(filePath.c_str(), std::ios::out);
    assert(file.is_open() && "File creation failed!");


    XmlDocument doc;
    objectToSerialize->Serialize(&doc, nullptr);
    std::string xml;
    rapidxml::print(std::back_inserter(xml), doc, 0);

    file << xml;
    file.close();
    doc.clear();
  }
  template<typename T>
  void CreateFileAndSerializeObject
  (
    T* objectToSerialize,
    const String& filePath
  )
  {
    std::ofstream file;
    file.open(filePath.c_str(), std::ios::out);
    assert(file.is_open() && "File creation failed!");


    XmlDocument doc;
    objectToSerialize->Serialize(&doc, nullptr);
    std::string xml;
    rapidxml::print(std::back_inserter(xml), doc, 0);

    file << xml;
    file.close();
    doc.clear();
  }

  const float g_desiredFps = 30.0f;
  const float g_animEps = 0.001f;
  String g_currentExt;

  // Interpolator functions Begin
  // Range checks added by OTSoftware.
  // https://github.com/triplepointfive/ogldev/blob/master/tutorial39/mesh.cpp

  bool LessEqual(float a, float b, float eps)
  {
    float diff = a - b;
    return diff < - eps || diff == 0.0f;
  }

  bool IsEqual(float a, float b, float eps)
  {
    return abs(a - b) < eps;
  }

  bool IsZero(float a, float eps)
  {
    return abs(a) < eps;
  }

  int GetMax(int a, int b)
  {
    return a > b ? a : b;
  }

  int GetMax(int a, int b, int c)
  {
    return GetMax(a, GetMax(b, c));
  }

  uint FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim)
  {
    for (uint i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
    {
      if
      (
        LessEqual
        (
          AnimationTime,
          static_cast<float> (pNodeAnim->mPositionKeys[i + 1].mTime),
          g_animEps
        )
      )
      {
        return i;
      }
    }

    assert(0);

    return 0;
  }

  uint FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
  {
    assert(pNodeAnim->mNumRotationKeys > 0);

    for (uint i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
    {
      if
      (
        LessEqual
        (
          AnimationTime,
          static_cast<float> (pNodeAnim->mRotationKeys[i + 1].mTime),
          g_animEps
        )
      )
      {
        return i;
      }
    }

    assert(0);

    return 0;
  }

  uint FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim)
  {
    assert(pNodeAnim->mNumScalingKeys > 0);

    for (uint i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
    {
      if
      (
        LessEqual
        (
          AnimationTime,
          static_cast<float> (pNodeAnim->mScalingKeys[i + 1].mTime),
          g_animEps
        )
      )
      {
        return i;
      }
    }

    assert(0);

    return 0;
  }

  void CalcInterpolatedPosition
  (
    aiVector3D& Out,
    float AnimationTime,
    const aiNodeAnim* pNodeAnim
  )
  {
    if (pNodeAnim->mNumPositionKeys == 1)
    {
      Out = pNodeAnim->mPositionKeys[0].mValue;
      return;
    }

    uint PositionIndex = FindPosition(AnimationTime, pNodeAnim);
    uint NextPositionIndex = (PositionIndex + 1);
    assert(NextPositionIndex < pNodeAnim->mNumPositionKeys);
    float DeltaTime =
      static_cast<float>
      (
        pNodeAnim->mPositionKeys[NextPositionIndex].mTime -
        pNodeAnim->mPositionKeys[PositionIndex].mTime
      );

    float Factor =
      (
        AnimationTime -
        static_cast<float> (pNodeAnim->mPositionKeys[PositionIndex].mTime)
      ) / DeltaTime;

    if (IsZero(Factor, 0.001f))
    {
      Factor = 0.0f;
    }

    if (IsEqual(Factor, 1.0f, 0.001f))
    {
      Factor = 1.0f;
    }

    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
    const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;
    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
  }

  void CalcInterpolatedRotation
  (
    aiQuaternion& Out,
    float AnimationTime,
    const aiNodeAnim* pNodeAnim
  )
  {
    // we need at least two values to interpolate...
    if (pNodeAnim->mNumRotationKeys == 1)
    {
      Out = pNodeAnim->mRotationKeys[0].mValue;
      return;
    }

    uint RotationIndex = FindRotation(AnimationTime, pNodeAnim);
    uint NextRotationIndex = (RotationIndex + 1);
    assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);

    float DeltaTime =
      static_cast<float>
      (
        pNodeAnim->mRotationKeys[NextRotationIndex].mTime -
        pNodeAnim->mRotationKeys[RotationIndex].mTime
      );

    float Factor =
      (
        AnimationTime -
        static_cast<float> (pNodeAnim->mRotationKeys[RotationIndex].mTime)
      ) / DeltaTime;

    if (IsZero(Factor, g_animEps))
    {
      Factor = 0.0f;
    }

    if (IsEqual(Factor, 1.0f, g_animEps))
    {
      Factor = 1.0f;
    }

    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiQuaternion& StartRotationQ =
      pNodeAnim->mRotationKeys[RotationIndex].mValue;

    const aiQuaternion& EndRotationQ =
      pNodeAnim->mRotationKeys[NextRotationIndex].mValue;

    aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
    Out = Out.Normalize();
  }

  void CalcInterpolatedScaling
  (
    aiVector3D& Out,
    float AnimationTime,
    const aiNodeAnim* pNodeAnim
  )
  {
    if (pNodeAnim->mNumScalingKeys == 1)
    {
      Out = pNodeAnim->mScalingKeys[0].mValue;
      return;
    }

    uint ScalingIndex = FindScaling(AnimationTime, pNodeAnim);
    uint NextScalingIndex = (ScalingIndex + 1);
    assert(NextScalingIndex < pNodeAnim->mNumScalingKeys);

    float DeltaTime =
      static_cast<float>
      (
        pNodeAnim->mScalingKeys[NextScalingIndex].mTime -
        pNodeAnim->mScalingKeys[ScalingIndex].mTime
      );

    float Factor =
      (
        AnimationTime -
        static_cast<float> (pNodeAnim->mScalingKeys[ScalingIndex].mTime)
      ) / DeltaTime;

    if (IsZero(Factor, 0.001f))
    {
      Factor = 0.0f;
    }

    if (IsEqual(Factor, 1.0f, 0.001f))
    {
      Factor = 1.0f;
    }

    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
    const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
  }

  // Interpolator functions END

  void ImportAnimation(const aiScene* scene, string file)
  {
    if (!scene->HasAnimations())
    {
      return;
    }

    for (uint i = 0; i < scene->mNumAnimations; i++)
    {
      aiAnimation* anim = scene->mAnimations[i];
      std::string animName(anim->mName.C_Str());
      string animFilePath = file;
      replace(animName.begin(), animName.end(), '.', '_');
      replace(animName.begin(), animName.end(), '|', '_');
      animFilePath += animName + ".anim";
      AddToUsedFiles(animFilePath);
      AnimationPtr tAnim = std::make_shared<Animation>();

      double fps = anim->mTicksPerSecond == 0 ? g_desiredFps :
        anim->mTicksPerSecond;

      double duration = anim->mDuration / fps;
      uint frameCount = (uint)ceil(duration * g_desiredFps);

      // Used to normalize animation start time.
      int cr, ct, cs, cmax;
      cr = ct = cs = cmax = 0;

      for (unsigned int chIndx = 0; chIndx < anim->mNumChannels; chIndx++)
      {
        KeyArray keys;
        aiNodeAnim* nodeAnim = anim->mChannels[chIndx];
        for (uint frame = 1; frame < frameCount; frame++)
        {
          float timeInTicks = (frame / g_desiredFps) *
            static_cast<float> (anim->mTicksPerSecond);

          aiVector3D t;
          if
          (
            // Timer is not yet reach the animation begin. Skip frames.
            // Happens when there aren't keys at the beginning of the animation.
            LessEqual
            (
              timeInTicks,
              static_cast<float> (nodeAnim->mPositionKeys[0].mTime),
              0.001f
            )
          )
          {
            continue;
          }
          else
          {
            CalcInterpolatedPosition(t, timeInTicks, nodeAnim);
            ct++;
          }

          aiQuaternion r;
          if
          (
            LessEqual
            (
              timeInTicks,
              static_cast<float> (nodeAnim->mRotationKeys[0].mTime),
              0.001f
            )
          )
          {
            continue;
          }
          else
          {
            CalcInterpolatedRotation(r, timeInTicks, nodeAnim);
            cr++;
          }

          aiVector3D s;
          if
          (
            LessEqual
            (
              timeInTicks,
              static_cast<float> (nodeAnim->mScalingKeys[0].mTime),
              0.001f
            )
          )
          {
            continue;
          }
          else
          {
            CalcInterpolatedScaling(s, timeInTicks, nodeAnim);
            cs++;
          }

          Key tKey;
          tKey.m_frame = frame;
          tKey.m_position = Vec3(t.x, t.y, t.z);
          tKey.m_rotation = Quaternion(r.w, r.x, r.y, r.z);
          tKey.m_scale = Vec3(s.x, s.y, s.z);
          keys.push_back(tKey);
        }

        cmax = GetMax(cr, ct, cs);
        cr = ct = cs = 0;
        tAnim->m_keys.insert(std::make_pair(nodeAnim->mNodeName.C_Str(), keys));
      }

      // Recalculate duration. May be misleading dueto shifted animations.
      tAnim->m_duration = static_cast<float>(cmax / g_desiredFps);
      tAnim->m_fps = static_cast<float>(g_desiredFps);

      CreateFileAndSerializeObject(tAnim, animFilePath);
    }
  }

  void ImportMaterial(const aiScene* scene, string filePath, string origin)
  {
    fs::path pathOrg = fs::path(origin).parent_path();

    auto textureFindAndCreateFunc = [scene, filePath, pathOrg]
    (
      aiTextureType textureAssimpType,
      aiMaterial* material
    ) -> TexturePtr
    {
      int texCount = material->GetTextureCount(textureAssimpType);
      TexturePtr tTexture;
      if (texCount > 0)
      {
        aiString texture;
        material->GetTexture(textureAssimpType, 0, &texture);

        string tName = texture.C_Str();
        bool embedded = false;
        if (!tName.empty() && tName[0] == '*')  // Embedded texture.
        {
          embedded = true;
          string indxPart = tName.substr(1);
          unsigned int tIndx = atoi(indxPart.c_str());
          if (scene->mNumTextures > tIndx)
          {
            aiTexture* t = scene->mTextures[tIndx];
            tName = GetEmbeddedTextureName(t);
          }
        }

        string fileName = tName;
        TrunckToFileName(fileName);
        string textPath = fs::path(filePath + fileName).lexically_normal()
          .u8string();

        if (!embedded && !std::filesystem::exists(textPath))
        {
          // Try copying texture.
          fs::path fullPath = pathOrg;
          fullPath.append(tName);
          fullPath = fullPath.lexically_normal();

          ifstream isGoodFile;
          isGoodFile.open(fullPath, ios::binary | ios::in);
          if (isGoodFile.good())
          {
            fs::path target = fs::path(textPath);
            if (target.has_parent_path())
            {
              fs::path dir = target.parent_path();
              if (!fs::exists(dir))
              {
                fs::create_directories(dir);
              }
            }

            fs::copy(fullPath, target, fs::copy_options::overwrite_existing);
          }
          isGoodFile.close();
        }

        AddToUsedFiles(textPath);
        tTexture = std::make_shared<Texture>();
        tTexture->SetFile(textPath);
      }
      return tTexture;
    };

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
      aiMaterial* material = scene->mMaterials[i];
      string name = GetMaterialName(material, i);
      string writePath = filePath + name + MATERIAL;
      MaterialPtr tMaterial = std::make_shared<Material>();

      auto diffuse = textureFindAndCreateFunc(aiTextureType_DIFFUSE, material);
      if (diffuse)
      {
        tMaterial->m_diffuseTexture = diffuse;
      }

      tMaterial->SetFile(writePath);
      CreateFileAndSerializeObject(tMaterial, writePath);
      AddToUsedFiles(writePath);
      tMaterials.push_back(tMaterial);
    }
  }

  // Creates a ToolKit mesh by reading the aiMesh
  // @param mainMesh: Pointer of the mesh
  template<typename convertType>
  void ConvertMesh
  (
    aiMesh* mesh,
    convertType tMesh
  )
  {
    assert(mesh->mNumVertices && "Mesh has no vertices!");

    // Skin data
    unordered_map<int, vector<std::pair<int, float> > > skinData;
    if constexpr (std::is_same<convertType, SkinMeshPtr>::value)
    {
      for (unsigned int i = 0; i < mesh->mNumBones; i++)
      {
        aiBone* bone = mesh->mBones[i];
        assert(g_skeletonMap.find(bone->mName.C_Str()) != g_skeletonMap.end());
        BoneNode bn = g_skeletonMap[bone->mName.C_Str()];
        for (unsigned int j = 0; j < bone->mNumWeights; j++)
        {
          aiVertexWeight vw = bone->mWeights[j];
          skinData[vw.mVertexId].push_back
          (
            std::pair<int, float>(bn.boneIndex, vw.mWeight)
          );
        }
      }
      tMesh->m_skeleton = g_skeleton;
    }

    tMesh->m_clientSideVertices.resize(mesh->mNumVertices);
    for (unsigned int vIndex = 0; vIndex < mesh->mNumVertices; vIndex++)
    {
      auto& v = tMesh->m_clientSideVertices[vIndex];
      v.pos =
        Vec3
        (
          mesh->mVertices[vIndex].x,
          mesh->mVertices[vIndex].y,
          mesh->mVertices[vIndex].z
        );

      if (mesh->HasNormals())
      {
        v.norm =
          Vec3
          (
            mesh->mNormals[vIndex].x,
            mesh->mNormals[vIndex].y,
            mesh->mNormals[vIndex].z
          );
      }

      // Does the mesh contain texture coordinates?
      if (mesh->HasTextureCoords(0))
      {
        v.tex.x = mesh->mTextureCoords[0][vIndex].x;
        v.tex.y = mesh->mTextureCoords[0][vIndex].y;
      }


      if (mesh->HasTangentsAndBitangents())
      {
        v.btan =
          Vec3
          (
            mesh->mBitangents[vIndex].x,
            mesh->mBitangents[vIndex].y,
            mesh->mBitangents[vIndex].z
          );
      }

      if constexpr (std::is_same<convertType, SkinMeshPtr>::value)
      {
        if
        (
          !skinData.empty() &&
          skinData.find(vIndex) != skinData.end()
        )
        {
          for (int j = 0; j < 4; j++)
          {
            if (j >= static_cast<int>(skinData[vIndex].size()))
            {
              skinData[vIndex].push_back(std::pair<int, float>(0, 0.0f));
            }
          }

          for (ubyte i = 0; i < 4; i++)
          {
            v.bones[i] = skinData[vIndex][i].first;
            v.weights[i] = skinData[vIndex][i].second;
          }
        }
      }
    }

    tMesh->m_clientSideIndices.resize(mesh->mNumFaces * 3);
    for (unsigned int face_i = 0; face_i < mesh->mNumFaces; face_i++)
    {
      aiFace face = mesh->mFaces[face_i];
      assert(face.mNumIndices == 3);
      for (ubyte i = 0; i < 3; i++)
      {
        tMesh->m_clientSideIndices[(face_i * 3) + i] = face.mIndices[i];
      }
    }

    tMesh->m_loaded = true;
    tMesh->m_vertexCount = static_cast<int>(tMesh->m_clientSideVertices.size());
    tMesh->m_indexCount = static_cast<int>(tMesh->m_clientSideIndices.size());
    tMesh->m_material = tMaterials[mesh->mMaterialIndex];
    for (ubyte i = 0; i < 3; i++)
    {
      tMesh->m_aabb.min[i] = mesh->mAABB.mMin[i];
      tMesh->m_aabb.max[i] = mesh->mAABB.mMax[i];
    }
  }

  void SearchMesh
  (
    const aiScene* scene,
    Scene* tScene,
    string filePath,
    aiNode* node,
    ULongID parentId
  )
  {
    Entity* entity = nullptr;

    // Write meshes
    if (node->mNumMeshes > 0)
    {
      entity = new Entity;
      entity->AddComponent(new MeshComponent);

      MeshPtr parentMeshOfNode;
      if (node->mNumMeshes == 1)
      {
        if (scene->mMeshes[node->mMeshes[0]]->HasBones())
        {
          SkinMeshPtr skinMesh = std::make_shared<SkinMesh>();
          aiMesh* mesh = scene->mMeshes[node->mMeshes[0]];
          ConvertMesh(mesh, skinMesh);
          parentMeshOfNode = skinMesh;
        }
        else
        {
          parentMeshOfNode = std::make_shared<Mesh>();
          aiMesh* mesh = scene->mMeshes[node->mMeshes[0]];
          ConvertMesh(mesh, parentMeshOfNode);
        }
      }
      // Don't support multiple skeletal mesh in the same node
      else
      {
        parentMeshOfNode = std::make_shared<Mesh>();
        for (unsigned int i = 1; i < node->mNumMeshes; i++)
        {
          aiMesh* mesh = scene->mMeshes[node->mMeshes[0]];
          MeshPtr subMesh = std::make_shared<Mesh>();
          ConvertMesh(mesh, subMesh);
          parentMeshOfNode->m_subMeshes.push_back(subMesh);
        }
      }
      string path, name;
      Decompose(filePath, path, name);

      string tag = MESH;
      if (parentMeshOfNode->IsSkinned())
      {
        tag = SKINMESH;
      }

      string fileName = std::string(node->mName.C_Str());
      ClearForbidden(fileName);
      String meshPath = path + fileName + tag;
      AddToUsedFiles(meshPath);

      parentMeshOfNode->SetFile(meshPath);
      CreateFileAndSerializeObject(parentMeshOfNode, meshPath);

      entity->GetMeshComponent()->SetMeshVal(parentMeshOfNode);
    }
    else
    {
      entity = GetEntityFactory()->CreateByType(EntityType::Entity_Base);
    }

    ULongID thisId = entity->GetIdVal();
    g_entityIds.push_back(thisId);

    for (unsigned int j = 0; j < node->mNumChildren; j++)
    {
      SearchMesh(scene, tScene, filePath, node->mChildren[j], thisId);
    }

    // Add entity to the scene to serialize the entity
    tScene->AddEntity(entity);
  }

  void SearchEntity
  (
    const aiScene* scene,
    Scene* tScene,
    aiNode* node,
    ULongID parentId
  )
  {
    ULongID thisId = g_entityIds[g_idListIterationIndex++];
    Entity* entity = tScene->GetEntity(thisId);
    entity->m_node->m_inheritScale = true;

    if (parentId != thisId)
    {
      Entity* pEntity = tScene->GetEntity(parentId);
      if (pEntity)
      {
        pEntity->m_node->AddChild(entity->m_node, true);
      }
      else
      {
        printf("One of entities has parent but it's not found!");
      }
    }
    entity->_parentId = parentId;

    for (unsigned int j = 0; j < node->mNumChildren; j++)
    {
      SearchEntity(scene, tScene, node->mChildren[j], thisId);
    }
  }
  void SetEntityTransforms
  (
    const aiScene* scene,
    Scene* tScene,
    aiNode* node
  )
  {
    ULongID thisId = g_entityIds[g_idListIterationIndex++];
    Entity* entity = tScene->GetEntity(thisId);

    Quaternion rt;
    Vec3 ts, scl;
    DecomposeAssimpMatrix(node->mTransformation, &ts, &rt, &scl);

    // Set child transforms first
    for (unsigned int j = 0; j < node->mNumChildren; j++)
    {
      SetEntityTransforms(scene, tScene, node->mChildren[j]);
    }

    // When all childs are set, set parent's transform
    entity->m_node->Translate(ts);
    entity->m_node->Rotate(rt);
    entity->m_node->Scale(scl);
  }

  void ImportSceneAndMeshes(const aiScene* scene, string filePath)
  {
    // Print Scene.
    string path, name;
    Decompose(filePath, path, name);

    string fullPath = path + name + ".scene";
    AddToUsedFiles(fullPath);
    Scene* tScene = new Scene;

    // Add Meshes.
    SearchMesh(scene, tScene, filePath, scene->mRootNode, -1);

    g_idListIterationIndex = 0;
    SearchEntity
    (
      scene,
      tScene,
      scene->mRootNode,
      g_entityIds[g_idListIterationIndex]
    );
    g_idListIterationIndex = 0;
    SetEntityTransforms(scene, tScene, scene->mRootNode);


    CreateFileAndSerializeObject(tScene, fullPath);
  }

  void PrintSkeleton_(const aiScene* scene, string filePath)
  {
    auto addBoneNodeFn = [](aiNode* node, aiBone* bone) -> void
    {
      BoneNode bn(node, 0);
      if (node->mName == bone->mName)
      {
        bn.bone = bone;
      }
      g_skeletonMap[node->mName.C_Str()] = bn;
    };

    // Collect skeleton parts
    vector<aiBone*> bones;
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
      aiMesh* mesh = scene->mMeshes[i];
      aiNode* meshNode = scene->mRootNode->FindNode(mesh->mName);
      for (unsigned int j = 0; j < mesh->mNumBones; j++)
      {
        aiBone* bone = mesh->mBones[j];
        bones.push_back(bone);
        aiNode* node = scene->mRootNode->FindNode(bone->mName);
        while (node)  // Go Up
        {
          if (node == meshNode)
          {
            break;
          }

          if (meshNode != nullptr)
          {
            if (node == meshNode->mParent)
            {
              break;
            }
          }

          addBoneNodeFn(node, bone);
          node = node->mParent;
        }

        node = scene->mRootNode->FindNode(bone->mName);

        // Go Down
        std::function<void(aiNode*)> checkDownFn =
          [&checkDownFn, &bone, &addBoneNodeFn] (aiNode* node) -> void
        {
          if (node == nullptr)
          {
            return;
          }

          addBoneNodeFn(node, bone);

          for (unsigned int i = 0; i < node->mNumChildren; i++)
          {
            checkDownFn(node->mChildren[i]);
          }
        };
        checkDownFn(node);
      }
    }

    for (auto& bone : bones)
    {
      if (g_skeletonMap.find(bone->mName.C_Str()) != g_skeletonMap.end())
      {
        g_skeletonMap[bone->mName.C_Str()].bone = bone;
      }
    }

    // Assign indices
    std::function<void(aiNode*, unsigned int&)> assignBoneIndexFn =
      [&assignBoneIndexFn] (aiNode* node, unsigned int& index) -> void
    {
      if (g_skeletonMap.find(node->mName.C_Str()) != g_skeletonMap.end())
      {
        g_skeletonMap[node->mName.C_Str()].boneIndex = index++;
      }

      for (unsigned int i = 0; i < node->mNumChildren; i++)
      {
        assignBoneIndexFn(node->mChildren[i], index);
      }
    };

    unsigned int boneIndex = 0;
    assignBoneIndexFn(scene->mRootNode, boneIndex);

    string name, path;
    Decompose(filePath, path, name);
    string fullPath = path + name + SKELETON;

    g_skeleton = std::make_shared<Skeleton>();
    g_skeleton->SetFile(fullPath);

    // Print
    std::function<void(aiNode* node, Bone*)> setBoneHierarchyFn =
      [&setBoneHierarchyFn]
    (
      aiNode* node,
      Bone* parentBone
    ) -> void
    {
      Bone* bone = parentBone;
      if (g_skeletonMap.find(node->mName.C_Str()) != g_skeletonMap.end())
      {
        assert(node->mName.length);

        bone = new Bone(node->mName.C_Str());
        g_skeleton->AddBone(bone, parentBone);
      }
      for (unsigned int i = 0; i < node->mNumChildren; i++)
      {
        setBoneHierarchyFn(node->mChildren[i], bone);
      }
    };

    std::function<void(aiNode* node)> setTransformationsFn =
      [&setTransformationsFn]
      (
        aiNode* node
      ) -> void
    {
      if (g_skeletonMap.find(node->mName.C_Str()) != g_skeletonMap.end())
      {
        Bone* tBone = g_skeleton->GetBone(node->mName.C_Str());

        // Set bone node transformation
        {
          Vec3 t, s;
          Quaternion r;
          DecomposeAssimpMatrix(node->mTransformation, &t, &r, &s);

          tBone->m_node->Translate(t);
          tBone->m_node->Rotate(r);
          tBone->m_node->Scale(s);
        }

        // Set bind pose transformation
        {
          aiBone* bone = g_skeletonMap[node->mName.C_Str()].bone;

          if (bone)
          {
            Vec3 t, s;
            Quaternion r;
            DecomposeAssimpMatrix(bone->mOffsetMatrix, &t, &r, &s);

            Mat4 tMat, rMat, sMat;
            tMat = glm::translate(tMat, t);
            rMat = glm::toMat4(r);
            sMat = glm::scale(sMat, s);
            tBone->m_inverseWorldMatrix = tMat * rMat * sMat;
          }
        }
      }

      for (unsigned int i = 0; i < node->mNumChildren; i++)
      {
        setTransformationsFn(node->mChildren[i]);
      }
    };

    setBoneHierarchyFn(scene->mRootNode, nullptr);
    setTransformationsFn(scene->mRootNode);

    CreateFileAndSerializeObject(g_skeleton, fullPath);
    AddToUsedFiles(fullPath);
  }

  void ImportTextures(const aiScene* scene, string filePath)
  {
    // Embedded textures.
    if (scene->HasTextures())
    {
      for (unsigned int i = 0; i < scene->mNumTextures; i++)
      {
        TexturePtr tTexture = std::make_shared<Texture>();
        aiTexture* texture = scene->mTextures[i];
        string embId = GetEmbeddedTextureName(texture);

        // Compressed.
        if (texture->mHeight == 0)
        {
          ofstream file(filePath + embId, fstream::out | std::fstream::binary);
          assert(file.good());

          file.write
          (
            (const char*)scene->mTextures[i]->pcData,
            scene->mTextures[i]->mWidth
          );
        }
        else
        {
          unsigned char* buffer = (unsigned char*)texture->pcData;
          stbi_write_png
          (
            filePath.c_str(),
            texture->mWidth,
            texture->mHeight,
            4,
            buffer,
            texture->mWidth * 4
          );
        }
      }
    }
  }

  int ToolKitMain(int argc, char* argv[])
  {
    try
    {
      if (argc < 2)
      {
        cout <<
          "usage: Import 'fileToImport.format' <op> -t 'importTo' <op> -s 1.0";
        throw (-1);
      }

      Assimp::Importer importer;
      importer.SetPropertyInteger
      (
        AI_CONFIG_PP_SBP_REMOVE,
        aiPrimitiveType_LINE | aiPrimitiveType_POINT
      );

      string dest, file = argv[1];
      Assimp::DefaultLogger::create("Assimplog.txt", Assimp::Logger::VERBOSE);
      for (int i = 0; i < argc; i++)
      {
        string arg = argv[i];
        Assimp::DefaultLogger::get()->info(arg);

        if (arg == "-t")
        {
          dest = fs::path(argv[i + 1]).append("").u8string();
        }

        if (arg == "-s")
        {
          float scale = static_cast<float>(std::atof(argv[i + 1]));
          importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, scale);
        }
      }

      dest = fs::path(dest).lexically_normal().u8string();
      if (!dest.empty())
      {
        fs::create_directories(dest);
      }

      string ext = file.substr(file.find_last_of("."));
      std::vector<string> files;
      if (ext == ".txt")
      {
        fstream fList;
        fList.open(file, ios::in);
        if (fList.is_open())
        {
          string fileStr;
          while (getline(fList, fileStr))
          {
            files.push_back(fileStr);
          }
          fList.close();
        }
      }
      else
      {
        files.push_back(file);
      }

      // Initialize ToolKit to serialize resources
      Main* g_proxy = new Main();
      Main::SetProxy(g_proxy);

      for (int i = 0; i < static_cast<int>(files.size()); i++)
      {
        file = files[i];
        // Clear global materials for each scene to prevent wrong referencing
        tMaterials.clear();

        const aiScene* scene = importer.ReadFile
        (
          file,
          aiProcess_Triangulate
          | aiProcess_SortByPType
          | aiProcess_FindDegenerates
          | aiProcess_CalcTangentSpace
          | aiProcess_FlipUVs
          | aiProcess_LimitBoneWeights
          | aiProcess_GenSmoothNormals
          | aiProcess_GlobalScale
          | aiProcess_FindInvalidData
          | aiProcess_GenBoundingBoxes
        );

        if (scene == nullptr)
        {
          throw (-1);
        }
        g_scene = scene;
        g_idListIterationIndex = 0;
        g_entityIds.clear();

        String fileName;
        DecomposePath(file, nullptr, &fileName, &g_currentExt);
        string destFile = dest + fileName;

        // DON'T BREAK THE CALLING ORDER!

        ImportAnimation(scene, dest);
        // Create Textures to reference in Materials
        ImportTextures(scene, dest);
        // Create Materials to reference in Meshes
        ImportMaterial(scene, dest, file);
        // Create a Skeleton to reference in Meshes
        PrintSkeleton_(scene, destFile);
        // Create Meshes & Scene
        ImportSceneAndMeshes(scene, destFile);
      }

      // Report all in use files.
      fstream inUse("out.txt", ios::out);
      for (string fs : g_usedFiles)
      {
        inUse << fs << endl;
      }
      inUse.close();


      delete g_proxy;
    }
    catch (int code)
    {
      Assimp::DefaultLogger::get()->error("Import failed");
      Assimp::DefaultLogger::kill();
      return code;
    }

    Assimp::DefaultLogger::get()->info("Import success");
    Assimp::DefaultLogger::kill();

    return 0;
  }
}   // namespace ToolKit
int main(int argc, char* argv[])
{
  return ToolKit::ToolKitMain(argc, argv);
}

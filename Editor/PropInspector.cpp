#include "PropInspector.h"
#include <memory>
#include <utility>
#include "App.h"
#include "Util.h"
#include "ConsoleWindow.h"
#include "TransformMod.h"
#include "MaterialInspector.h"
#include "ImGui/imgui_stdlib.h"
#include "DebugNew.h"

namespace ToolKit
{
  namespace Editor
  {

    // View
    //////////////////////////////////////////////////////////////////////////

    void View::ShowVariant(ParameterVariant* var, ComponentPtr comp)
    {
      if (!var->m_exposed)
      {
        return;
      }

      ImGui::BeginDisabled(!var->m_editable);

      switch (var->GetType())
      {
        case ParameterVariant::VariantType::Bool:
        {
          bool val = var->GetVar<bool>();
          if (ImGui::Checkbox(var->m_name.c_str(), &val))
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::Float:
        {
          float val = var->GetVar<float>();
          if (ImGui::DragFloat(var->m_name.c_str(), &val, 0.1f))
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::Int:
        {
          int val = var->GetVar<int>();
          if(ImGui::InputInt(var->m_name.c_str(), &val))
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::Vec2:
        {
          Vec2 val = var->GetVar<Vec2>();
          if (ImGui::DragFloat2(var->m_name.c_str(), &val[0], 0.1f))
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::Vec3:
        {
          Vec3 val = var->GetVar<Vec3>();
          if (var->m_hint.isColor)
          {
            if
            (
              ImGui::ColorEdit3
              (
                var->m_name.c_str(),
                &val[0],
                ImGuiColorEditFlags_NoLabel
              )
            )
            {
              *var = val;
            }
          }
          else if (var->m_hint.isRangeLimited)
          {
            if
            (
              ImGui::DragFloat3
              (
                var->m_name.c_str(),
                &val[0],
                var->m_hint.increment,
                var->m_hint.rangeMin,
                var->m_hint.rangeMax
              )
            )
            {
              *var = val;
            }
          }
          else
          {
            if (ImGui::DragFloat3(var->m_name.c_str(), &val[0], 0.1f))
            {
              *var = val;
            }
          }
        }
        break;
        case ParameterVariant::VariantType::Vec4:
        {
          Vec4 val = var->GetVar<Vec4>();
          if (var->m_hint.isColor)
          {
            if
            (
              ImGui::ColorEdit4
              (
                var->m_name.c_str(),
                &val[0],
                ImGuiColorEditFlags_NoLabel
              )
            )
            {
              *var = val;
            }
          }
          else if (var->m_hint.isRangeLimited)
          {
            if
            (
              ImGui::DragFloat4
              (
                var->m_name.c_str(),
                &val[0],
                var->m_hint.increment,
                var->m_hint.rangeMin,
                var->m_hint.rangeMax
              )
            )
            {
              *var = val;
            }
          }
          else
          {
            if (ImGui::DragFloat4(var->m_name.c_str(), &val[0], 0.1f))
            {
              *var = val;
            }
          }
        }
        break;
        case ParameterVariant::VariantType::String:
        {
          String val = var->GetVar<String>();
          if
          (
            ImGui::InputText(var->m_name.c_str(), &val)
            && ImGuiEnterPressed()
          )
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::ULongID:
        {
          ULongID val = var->GetVar<ULongID>();
          if
          (
            ImGui::InputScalar
            (
              var->m_name.c_str(),
              ImGuiDataType_U32,
              var->GetVarPtr<ULongID>()
            )
            && ImGuiEnterPressed()
          )
          {
            *var = val;
          }
        }
        break;
        case ParameterVariant::VariantType::MaterialPtr:
        {
          MaterialPtr& mref = var->GetVar<MaterialPtr>();
          String file, id;
          if (mref)
          {
            id = std::to_string(mref->m_id);
            file = mref->GetFile();
          }

          String uniqueName = var->m_name + "##" + id;
          DropSubZone
          (
            uniqueName,
            static_cast<uint> (UI::m_materialIcon->m_textureId),
            file,
            [&var](const DirectoryEntry& entry) -> void
            {
              if (GetResourceType(entry.m_ext) == ResourceType::Material)
              {
                *var = GetMaterialManager()->Create<Material>
                (
                  entry.GetFullPath()
                );
              }
              else
              {
                GetLogger()->WriteConsole
                (
                  LogType::Error,
                  "Only Material Types are accepted."
                );
              }
            }
          );
        }
        break;
        case ParameterVariant::VariantType::MeshPtr:
        {
          MeshPtr mref = var->GetVar<MeshPtr>();
          DropSubZone
          (
            "Mesh##" + std::to_string(mref->m_id),
            static_cast<uint> (UI::m_meshIcon->m_textureId),
            mref->GetFile(),
            [](const DirectoryEntry& entry) -> void
            {
              assert(false && "Not implemented !");
            }
          );
        }
        break;
        case ParameterVariant::VariantType::HdriPtr:
        {
        HdriPtr mref = var->GetVar<HdriPtr>();
        String file, id;
        if (mref)
        {
          id = std::to_string(mref->m_id);
          file = mref->GetFile();
        }

        DropSubZone
        (
          "Hdri##" + id,
          static_cast<uint> (UI::m_imageIcon->m_textureId),
          file,
          [&var](const DirectoryEntry& entry) -> void
          {
            if (GetResourceType(entry.m_ext) == ResourceType::Hdri)
            {
              *var = GetTextureManager()->Create<Hdri>
              (
                entry.GetFullPath()
              );
            }
            else
            {
              GetLogger()->WriteConsole
              (
                LogType::Error,
                "Only hdri's are accepted."
              );
            }
          }
        );
        break;
      }
        case ParameterVariant::VariantType::AnimRecordPtrMap:
        {
          ShowAnimControllerComponent(var, comp);
        }
        break;
        default:
         break;
      }

      ImGui::EndDisabled();
    }

    void View::ShowAnimControllerComponent
    (
      ParameterVariant* var,
      ComponentPtr comp
    )
    {
      AnimRecordPtrMap& mref = var->GetVar<AnimRecordPtrMap>();
      String file, id;

      AnimControllerComponent* animPlayerComp =
        reinterpret_cast<AnimControllerComponent*>(comp.get());

      // If component isn't AnimationPlayerComponent, don't show variant
      if
      (
        !comp ||
        comp->GetType() != ComponentType::AnimControllerComponent
      )
      {
        GetLogger()->WriteConsole
        (
          LogType::Error,
          "AnimRecordPtrMap is for AnimationControllerComponent"
        );
        return;
      }


      if (animPlayerComp->GetActiveRecord())
      {
        String file;
        DecomposePath
        (
          animPlayerComp->GetActiveRecord()->m_animation->GetFile(),
          nullptr,
          &file,
          nullptr
        );

        String text = Format
        (
          "Animation: %s, Duration: %f, T: %f",
          file.c_str(),
          animPlayerComp->GetActiveRecord()->m_animation->m_duration,
          animPlayerComp->GetActiveRecord()->m_currentTime
        );

        ImGui::Text(text.c_str());
      }

      if
      (
        ImGui::BeginTable
        (
          "Animation Records and Signals",
          4,
          ImGuiTableFlags_RowBg
          | ImGuiTableFlags_Borders
          | ImGuiTableFlags_Resizable
          | ImGuiTableFlags_Reorderable
          | ImGuiTableFlags_ScrollY
        )
      )
      {
        float tableWdth = ImGui::GetItemRectSize().x;
        ImGui::TableSetupColumn
        (
          "Animation",
          ImGuiTableColumnFlags_WidthStretch,
          tableWdth / 5.0f
        );

        ImGui::TableSetupColumn
        (
          "Name",
          ImGuiTableColumnFlags_WidthStretch,
          tableWdth / 2.5f
        );

        ImGui::TableSetupColumn
        (
          "Preview",
          ImGuiTableColumnFlags_WidthStretch,
          tableWdth / 4.0f
        );

        ImGui::TableSetupColumn
        (
          "",
          ImGuiTableColumnFlags_WidthStretch,
          tableWdth / 20.0f
        );

        ImGui::TableHeadersRow();

        uint rowIndx = 0;
        String removedSignalName = "";
        String nameUpdated = "";
        std::pair<String, AnimRecordPtr> nameUpdatedPair = {};

        static std::pair<String, AnimRecordPtr> extraTrack =
          std::make_pair("", std::make_shared<AnimRecord>());

        // Animation DropZone
        auto showAnimationDropzone =
        [this, tableWdth, file]
        (uint& columnIndx, const std::pair<String, AnimRecordPtr>& pair)
        {
          ImGui::TableSetColumnIndex(columnIndx++);
          ImGui::SetCursorPosX(tableWdth / 25.0f);
          DropZone
          (
            static_cast<uint> (UI::m_clipIcon->m_textureId),
            file,
            [&pair](const DirectoryEntry& entry) -> void
            {
              if (GetResourceType(entry.m_ext) == ResourceType::Animation)
              {
                pair.second->m_animation =
                  GetAnimationManager()->Create<Animation>
                  (
                    entry.GetFullPath()
                  );
                if (pair.first.empty())
                {
                  extraTrack.first = entry.m_fileName;
                }
              }
              else
              {
                GetLogger()->WriteConsole
                (
                  LogType::Error,
                  "Only animations are accepted."
                );
              }
            }
          );
        };

        auto showSignalName =
        [this, &nameUpdated, &nameUpdatedPair, tableWdth]
        (uint& columnIndx, const std::pair<String, AnimRecordPtr>& pair)
        {
          ImGui::TableSetColumnIndex(columnIndx++);
          ImGui::SetCursorPosY
          (
            ImGui::GetCursorPos().y + (ImGui::GetItemRectSize().y / 4.0f)
          );
          ImGui::PushItemWidth((tableWdth / 2.5f) - 5.0f);
          String readOnly = pair.first;
          if
          (
            ImGui::InputText
            (
              "##",
              &readOnly,
              ImGuiInputTextFlags_EnterReturnsTrue
            )
            && readOnly.length()
          )
          {
            nameUpdated = readOnly;
            nameUpdatedPair = pair;
          }
          ImGui::PopItemWidth();
        };
        for (auto it = mref.begin(); it != mref.end(); ++it, rowIndx++)
        {
          uint columnIndx = 0;
          ImGui::TableNextRow();
          ImGui::PushID(rowIndx);

          showAnimationDropzone(columnIndx, *it);

          // Signal Name
          showSignalName(columnIndx, *it);

          // Play, Pause & Stop Buttons
          ImGui::TableSetColumnIndex(columnIndx++);
          if (it->second->m_animation)
          {
            ImGui::SetCursorPosX
            (
              ImGui::GetCursorPos().x
              + (ImGui::GetItemRectSize().x / 10.0f)
            );

            ImGui::SetCursorPosY
            (
              ImGui::GetCursorPos().y
              + (ImGui::GetItemRectSize().y / 5.0f)
            );

            AnimRecordPtr activeRecord = animPlayerComp->GetActiveRecord();

            // Alternate between Play - Pause buttons.
            if
            (
              activeRecord == it->second
              && activeRecord->m_state == AnimRecord::State::Play
            )
            {
              if
              (
                UI::ImageButtonDecorless
                (
                  UI::m_pauseIcon->m_textureId,
                  Vec2(24, 24),
                  false
                )
              )
              {
                animPlayerComp->Pause();
              }
            }
            else if
            (
              UI::ImageButtonDecorless
              (
                UI::m_playIcon->m_textureId,
                Vec2(24, 24),
                false
              )
            )
            {
              animPlayerComp->Play(it->first.c_str());
            }

            // Draw stop button always.
            ImGui::SameLine();
            if
            (
              UI::ImageButtonDecorless
              (
                UI::m_stopIcon->m_textureId,
                Vec2(24, 24),
                false
              )
            )
            {
              animPlayerComp->Stop();
            }
          }

          // Remove Button
          {
            ImGui::TableSetColumnIndex(columnIndx++);
            ImGui::SetCursorPosY
            (
              ImGui::GetCursorPos().y + (ImGui::GetItemRectSize().y / 4.0f)
            );

            if
            (
              UI::ImageButtonDecorless
              (
                UI::m_closeIcon->m_textureId,
                Vec2(15, 15),
                false
              )
            )
            {
              removedSignalName = it->first;
            }
          }

          ImGui::PopID();
        }

        // Show last extra track.
        uint columnIndx = 0;
        ImGui::TableNextRow();
        ImGui::PushID(rowIndx);

        showAnimationDropzone(columnIndx, extraTrack);

        // Signal Name
        showSignalName(columnIndx, extraTrack);
        ImGui::PopID();

        if (removedSignalName.length())
        {
          animPlayerComp->RemoveSignal(removedSignalName);
        }

        if (nameUpdated.length() && nameUpdatedPair.first != nameUpdated)
        {
          if (mref.find(nameUpdated) != mref.end())
          {
            GetLogger()->WriteConsole(LogType::Error, "SignalName exists");
          }
          else if (nameUpdatedPair.first == extraTrack.first)
          {
            extraTrack.first = nameUpdated;
          }
          else
          {
            auto node = mref.extract(nameUpdatedPair.first);
            node.key() = nameUpdated;
            mref.insert(std::move(node));

            nameUpdated = "";
            nameUpdatedPair = {};
          }
        }

        // If extra track is filled properly, add it to the list
        if (extraTrack.first != "" && extraTrack.second->m_animation != nullptr)
        {
          mref.insert(extraTrack);
          extraTrack.first = "";
          extraTrack.second = std::make_shared<AnimRecord>();
        }

        ImGui::EndTable();
      }
    }

    void View::DropZone
    (
      uint fallbackIcon,
      const String& file,
      std::function<void(const DirectoryEntry& entry)> dropAction,
      const String& dropName
    )
    {
      DirectoryEntry dirEnt;
      bool fileExist = g_app->GetAssetBrowser()->GetFileEntry(file, dirEnt);
      uint iconId = fallbackIcon;

      ImVec2 texCoords = ImVec2(1.0f, 1.0f);
      if (RenderTargetPtr thumb = dirEnt.GetThumbnail())
      {
        texCoords = ImVec2(1.0f, -1.0f);
        iconId = thumb->m_textureId;
      }
      else if (fileExist)
      {
        dirEnt.GenerateThumbnail();

        if (RenderTargetPtr thumb = dirEnt.GetThumbnail())
        {
          iconId = thumb->m_textureId;
        }
      }

      if (!dropName.empty())
      {
        ImGui::Text(dropName.c_str());
      }

      bool clicked = ImGui::ImageButton
      (
        reinterpret_cast<void*>((intptr_t)iconId),
        ImVec2(48.0f, 48.0f),
        ImVec2(0.0f, 0.0f),
        texCoords
      );

      if (ImGui::BeginDragDropTarget())
      {
        if
        (
          const ImGuiPayload* payload =
          ImGui::AcceptDragDropPayload("BrowserDragZone")
        )
        {
          IM_ASSERT(payload->DataSize == sizeof(DirectoryEntry));
          DirectoryEntry entry = *(const DirectoryEntry*)payload->Data;
          dropAction(entry);
        }

        ImGui::EndDragDropTarget();
      }

      // Show file info.
      String info = "Drop zone";
      if (!file.empty() && !dirEnt.m_fileName.empty())
      {
        info = "";
        if (ResourceManager* man = dirEnt.GetManager())
        {
          auto textureRepFn = [&info, file](const TexturePtr& t) -> void
          {
            if (t)
            {
              String file, ext;
              DecomposePath(t->GetFile(), nullptr, &file, &ext);

              info += "Texture: " + file + ext + "\n";
              info += "Width: " + std::to_string(t->m_width) + "\n";
              info += "Height: " + std::to_string(t->m_height);
            }
          };

          if (man->m_type == ResourceType::Material)
          {
            MaterialPtr mr = man->Create<Material>(file);
            if (clicked)
            {
              g_app->GetMaterialInspector()->m_material = mr;
            }

            info += "File: " + dirEnt.m_fileName + dirEnt.m_ext + "\n";
            textureRepFn(mr->m_diffuseTexture);
          }

          if (man->m_type == ResourceType::Texture)
          {
            TexturePtr t = man->Create<Texture>(file);
            textureRepFn(t);
          }
        }
      }

      UI::HelpMarker(TKLoc + file, info.c_str(), 0.1f);
    }

    void View::DropSubZone
    (
      const String& title,
      uint fallbackIcon,
      const String& file,
      std::function<void(const DirectoryEntry& entry)> dropAction
    )
    {
      if (ImGui::TreeNode(title.c_str()))
      {
        DropZone(fallbackIcon, file, dropAction);
        ImGui::TreePop();
      }
    }

    bool View::ImGuiEnterPressed()
    {
      return
      (
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)
        || ImGui::IsKeyPressed(ImGuiKey_Enter)
      );
    }

    // EntityView
    //////////////////////////////////////////////////////////////////////////

    void EntityView::Show()
    {
      if (m_entity == nullptr)
      {
        return;
      }

      ShowParameterBlock(m_entity->m_localData, m_entity->GetIdVal());

      // Missing data reporter.
      if (m_entity->IsDrawable())
      {
        Drawable* dw = static_cast<Drawable*> (m_entity);
        MeshPtr mesh = dw->GetMesh();

        StringArray missingData;
        MeshRawCPtrArray meshes;
        mesh->GetAllMeshes(meshes);

        for (const Mesh* subMesh : meshes)
        {
          if (!subMesh->_missingFile.empty())
          {
            missingData.push_back(subMesh->_missingFile);
          }

          if (MaterialPtr mat = subMesh->m_material)
          {
            if (!mat->_missingFile.empty())
            {
              missingData.push_back(mat->_missingFile);
            }

            if (TexturePtr tex = mat->m_diffuseTexture)
            {
              if (!tex->_missingFile.empty())
              {
                missingData.push_back(tex->_missingFile);
              }
            }
          }
        }

        if (!missingData.empty())
        {
          ImGui::Text("Missing Data: ");
          ImGui::Separator();
          ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
          for (const String& data : missingData)
          {
            ImGui::Text(data.c_str());
          }
          ImGui::PopStyleColor();
        }
      }

      if
      (
        ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_DefaultOpen)
      )
      {
        Mat3 rotate;
        Vec3 scale, shear;
        Mat4 ts = m_entity->m_node->GetTransform(g_app->m_transformSpace);
        QDUDecomposition(ts, rotate, scale, shear);

        // Continuous edit utils.
        static TransformAction* dragMem = nullptr;
        const auto saveDragMemFn = [this]() -> void
        {
          if (dragMem == nullptr)
          {
            dragMem = new TransformAction(m_entity);
          }
        };

        const auto saveTransformActionFn = [this]() -> void
        {
          if (ImGui::IsItemDeactivatedAfterEdit())
          {
            ActionManager::GetInstance()->AddAction(dragMem);
            dragMem = nullptr;
          }
        };

        TransformationSpace space = g_app->m_transformSpace;
        Vec3 translate = glm::column(ts, 3);
        Vec3 newTranslate = translate;
        if (ImGui::DragFloat3("Translate", &newTranslate[0], 0.25f))
        {
          saveDragMemFn();

          bool isDrag = ImGui::IsMouseDragging(0, 0.25f);
          if (isDrag)
          {
            m_entity->m_node->Translate(newTranslate - translate, space);
          }
          else if (!isDrag && ImGuiEnterPressed())
          {
            m_entity->m_node->SetTranslation(newTranslate, space);
          }
        }

        saveTransformActionFn();

        Quaternion q0 = glm::toQuat(rotate);
        Vec3 eularXYZ = glm::eulerAngles(q0);
        Vec3 degrees = glm::degrees(eularXYZ);
        if (ImGui::DragFloat3("Rotate", &degrees[0], 0.25f))
        {
          saveDragMemFn();

          Vec3 eular = glm::radians(degrees);
          Vec3 change = eular - eularXYZ;

          bool isDrag = ImGui::IsMouseDragging(0, 0.25f);
          if (!isDrag)
          {
            change = eular;
          }

          Quaternion qx = glm::angleAxis(change.x, X_AXIS);
          Quaternion qy = glm::angleAxis(change.y, Y_AXIS);
          Quaternion qz = glm::angleAxis(change.z, Z_AXIS);
          Quaternion q = qz * qy * qx;

          if (isDrag)
          {
            m_entity->m_node->Rotate(q, space);
          }
          else if (ImGuiEnterPressed())
          {
            m_entity->m_node->SetOrientation(q, space);
          }
        }

        saveTransformActionFn();

        if (ImGui::DragFloat3("Scale", &scale[0], 0.1f))
        {
          bool exceed = false;
          if (scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f)
          {
            exceed = true;
          }

          if (!exceed)
          {
            saveDragMemFn();

            bool isDrag = ImGui::IsMouseDragging(0, 0.25f);
            if (isDrag || ImGuiEnterPressed())
            {
              m_entity->m_node->SetScale(scale);
            }
          }
        }

        saveTransformActionFn();

        if
        (
          ImGui::Checkbox("Inherit Scale", &m_entity->m_node->m_inheritScale)
        )
        {
          m_entity->m_node->SetInheritScaleDeep
          (
            m_entity->m_node->m_inheritScale
          );
        }

        ImGui::Separator();

        BoundingBox bb = m_entity->GetAABB(true);
        Vec3 dim = bb.max - bb.min;
        ImGui::Text("Bounding box dimensions:");
        ImGui::Text("x: %.2f", dim.x);
        ImGui::SameLine();
        ImGui::Text("\ty: %.2f", dim.y);
        ImGui::SameLine();
        ImGui::Text("\tz: %.2f", dim.z);
      }

      ShowCustomData();

      if(ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, g_indentSpacing);

        std::vector<ULongID> compRemove;
        for (ComponentPtr& com : m_entity->m_components)
        {
          if (ShowComponentBlock(com))
          {
            compRemove.push_back(com->m_id);
          }
        }

        for (ULongID id : compRemove)
        {
          ActionManager::GetInstance()->AddAction
          (
            new DeleteComponentAction(m_entity->GetComponent(id))
          );
        }

        // Remove billboards if necessary
        std::static_pointer_cast<EditorScene>
        (
          GetSceneManager()->GetCurrentScene()
        )->InitEntityBillboard(m_entity);

        ImGui::PushItemWidth(150);
        static bool addInAction = false;
        if (addInAction)
        {
          int dataType = 0;
          if
          (
            ImGui::Combo
            (
              "##NewComponent",
              &dataType,
              "..."
                "\0Mesh Component"
                "\0Material Component"
                "\0Environment Component"
                "\0Animation Controller Component"
            )
          )
          {
            Component* newComponent = nullptr;
            switch (dataType)
            {
            case 1:
              newComponent = new MeshComponent();
              break;
            case 2:
              newComponent = new MaterialComponent;
              break;
            case 3:
              newComponent = new EnvironmentComponent;
              break;
            case 4:
              newComponent = new AnimControllerComponent;
            break;
            default:
              break;
            }

            if (newComponent)
            {
              m_entity->AddComponent(newComponent);
              addInAction = false;

              // Add gizmo if needed
              std::static_pointer_cast<EditorScene>
              (
                GetSceneManager()->GetCurrentScene()
              )->AddBillboardToEntity(m_entity);
            }
          }
        }
        ImGui::PopItemWidth();

        ImGui::Separator();
        if (UI::BeginCenteredTextButton("Add Component"))
        {
          addInAction = true;
        }
        UI::EndCenteredTextButton();

        ImGui::PopStyleVar();
      }
    }

    void EntityView::ShowParameterBlock(ParameterBlock& params, ULongID id)
    {
      VariantCategoryArray categories;
      params.GetCategories(categories, true, true);

      for (VariantCategory& category : categories)
      {
        if (category.Name == CustomDataCategory.Name)
        {
          continue;
        }

        String varName = category.Name + "##" + std::to_string(id);
        if
        (
          ImGui::CollapsingHeader
          (
            varName.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen
          )
        )
        {
          ParameterVariantRawPtrArray vars;
          params.GetByCategory(category.Name, vars);

          for (ParameterVariant* var : vars)
          {
            ShowVariant(var, nullptr);
          }
        }
      }
    }

    bool EntityView::ShowComponentBlock(ComponentPtr& comp)
    {
      VariantCategoryArray categories;
      comp->m_localData.GetCategories(categories, true, true);

      bool removeComp = false;
      for (VariantCategory& category : categories)
      {
        String varName = category.Name + "##" + std::to_string(comp->m_id);
        bool isOpen = ImGui::TreeNodeEx
        (
          varName.c_str(),
          ImGuiTreeNodeFlags_DefaultOpen | g_treeNodeFlags
        );

        float offset = ImGui::GetContentRegionAvail().x - 10.0f;
        ImGui::SameLine(offset);
        ImGui::PushID(static_cast<int> (comp->m_id));
        if
        (
          UI::ImageButtonDecorless
          (
            UI::m_closeIcon->m_textureId,
            ImVec2(15.0f, 15.0f),
            false
          ) &&
          !removeComp
        )
        {
          g_app->m_statusMsg = "Component " + category.Name + " removed.";
          removeComp = true;
        }
        ImGui::PopID();

        if (isOpen)
        {
          ParameterVariantRawPtrArray vars;
          comp->m_localData.GetByCategory(category.Name, vars);

          for (ParameterVariant* var : vars)
          {
            ShowVariant(var, comp);
          }

          ImGui::TreePop();
        }
      }

      return removeComp;
    }

    void EntityView::ShowCustomData()
    {
      if
      (
        ImGui::CollapsingHeader("Custom Data", ImGuiTreeNodeFlags_DefaultOpen)
      )
      {
        if
        (
          ImGui::BeginTable
          (
            "##CustomData",
            3,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedSame
          )
        )
        {
          Vec2 xSize = ImGui::CalcTextSize("Name");
          xSize *= 3.0f;
          ImGui::TableSetupColumn
          (
            "Name",
            ImGuiTableColumnFlags_WidthFixed,
            xSize.x
          );
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          xSize = ImGui::CalcTextSize("X");
          xSize *= 2.5f;
          ImGui::TableSetupColumn
          (
            "##Remove",
            ImGuiTableColumnFlags_WidthFixed,
            xSize.x
          );

          ImGui::TableHeadersRow();

          ImGui::TableSetColumnIndex(0);
          ImGui::PushItemWidth(-FLT_MIN);

          ImGui::TableSetColumnIndex(1);
          ImGui::PushItemWidth(-FLT_MIN);

          ParameterVariantRawPtrArray customParams;
          m_entity->m_localData.GetByCategory
          (
            CustomDataCategory.Name,
            customParams
          );

          ParameterVariant* remove = nullptr;
          for (size_t i = 0; i < customParams.size(); i++)
          {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::PushID(static_cast<int>(i));
            ParameterVariant* var = customParams[i];
            static char buff[1024];
            strcpy_s(buff, sizeof(buff), var->m_name.c_str());

            String pNameId = "##Name" + std::to_string(i);
            ImGui::InputText(pNameId.c_str(), buff, sizeof(buff));
            var->m_name = buff;

            ImGui::TableSetColumnIndex(1);

            String pId = "##" + std::to_string(i);
            switch (var->GetType())
            {
            case ParameterVariant::VariantType::String:
            {
              ImGui::InputText(pId.c_str(), var->GetVarPtr<String>());
            }
            break;
            case ParameterVariant::VariantType::Bool:
            {
              ImGui::Checkbox(pId.c_str(), var->GetVarPtr<bool>());
            }
            break;
            case ParameterVariant::VariantType::Int:
            {
              ImGui::InputInt(pId.c_str(), var->GetVarPtr<int>());
            }
            break;
            case ParameterVariant::VariantType::Float:
            {
              ImGui::DragFloat(pId.c_str(), var->GetVarPtr<float>(), 0.1f);
            }
            break;
            case ParameterVariant::VariantType::Vec3:
            {
              ImGui::DragFloat3(pId.c_str(), &var->GetVar<Vec3>()[0], 0.1f);
            }
            break;
            case ParameterVariant::VariantType::Vec4:
            {
              ImGui::DragFloat4(pId.c_str(), &var->GetVar<Vec4>()[0], 0.1f);
            }
            break;
            case ParameterVariant::VariantType::Mat3:
            {
              Vec3 vec;
              Mat3 val = var->GetVar<Mat3>();
              for (int j = 0; j < 3; j++)
              {
                pId += std::to_string(j);
                vec = glm::row(val, j);
                ImGui::InputFloat3(pId.c_str(), &vec[0]);
                val = glm::row(val, j, vec);
                *var = val;
              }
            }
            break;
            case ParameterVariant::VariantType::Mat4:
            {
              Vec4 vec;
              Mat4 val = var->GetVar<Mat4>();
              for (int j = 0; j < 4; j++)
              {
                pId += std::to_string(j);
                vec = glm::row(val, j);
                ImGui::InputFloat4(pId.c_str(), &vec[0]);
                val = glm::row(val, j, vec);
                *var = val;
              }
            }
            break;
            }

            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button("X"))
            {
              remove = customParams[i];
              g_app->m_statusMsg =
              Format("Parameter %d: %s removed.", i + 1, var->m_name.c_str());
            }

            ImGui::PopID();
          }

          if (remove != nullptr)
          {
            m_entity->m_localData.Remove(remove->m_id);
          }

          ImGui::EndTable();
          ImGui::Separator();

          ImGui::PushItemWidth(150);
          static bool addInAction = false;
          if (addInAction)
          {
            int dataType = 0;
            if
            (
              ImGui::Combo
              (
                "##NewCustData",
                &dataType,
                "...\0String\0Boolean\0Int\0Float\0Vec3\0Vec4\0Mat3\0Mat4"
              )
            )
            {
              ParameterVariant customVar;
              // This makes them only visible in Custom Data dropdown.
              customVar.m_exposed = false;
              customVar.m_editable = true;
              customVar.m_category = CustomDataCategory;

              bool added = true;
              switch (dataType)
              {
              case 1:
                customVar = "";
                break;
              case 2:
                customVar = false;
                break;
              case 3:
                customVar = 0;
                break;
              case 4:
                customVar = 0.0f;
                break;
              case 5:
                customVar = ZERO;
                break;
              case 6:
                customVar = Vec4();
                break;
              case 7:
                customVar = Mat3();
                break;
              case 8:
                customVar = Mat4();
                break;
              default:
                added = false;
                break;
              }

              if (added)
              {
                m_entity->m_localData.Add(customVar);
                addInAction = false;
              }
            }
          }
          ImGui::PopItemWidth();

          if (UI::BeginCenteredTextButton("Add Custom Data"))
          {
            addInAction = true;
          }
          UI::EndCenteredTextButton();
        }
      }
    }

    // PropInspector
    //////////////////////////////////////////////////////////////////////////

    PropInspector::PropInspector(XmlNode* node)
      : PropInspector()
    {
      DeSerialize(nullptr, node);
    }

    PropInspector::PropInspector()
    {
      m_view = new EntityView();
    }

    PropInspector::~PropInspector()
    {
      SafeDel(m_view);
    }

    void PropInspector::Show()
    {
      if (ImGui::Begin(m_name.c_str(), &m_visible))
      {
        HandleStates();

        Entity* curr = g_app->GetCurrentScene()->GetCurrentSelection();
        if (curr == nullptr)
        {
          ImGui::Text("Select an entity");
        }
        else
        {
          m_view->m_entity = curr;
          m_view->Show();
        }
      }
      ImGui::End();
    }

    Window::Type PropInspector::GetType() const
    {
      return Window::Type::Inspector;
    }

    void PropInspector::DispatchSignals() const
    {
      ModShortCutSignals
      (
        {
          SDL_SCANCODE_DELETE,
          SDL_SCANCODE_D,
          SDL_SCANCODE_F,
          SDL_SCANCODE_R,
          SDL_SCANCODE_G,
          SDL_SCANCODE_B,
          SDL_SCANCODE_S
        }
      );
    }

  }  // namespace Editor
}  // namespace ToolKit


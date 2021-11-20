#include "stdafx.h"

#include "EditorViewport2d.h"
#include "Directional.h"
#include "Renderer.h"
#include "App.h"
#include "GlobalDef.h"
#include "SDL.h"
#include "OverlayUI.h"
#include "Node.h"
#include "Primative.h"
#include "Grid.h"
#include "FolderWindow.h"
#include "ConsoleWindow.h"
#include "Gizmo.h"
#include "Mod.h"
#include "Util.h"
#include "DebugNew.h"

namespace ToolKit
{
  namespace Editor
  {

    EditorViewport2d::EditorViewport2d(XmlNode* node)
      : EditorViewport(node)
    {
    }

    EditorViewport2d::EditorViewport2d(float width, float height)
      : EditorViewport(width, height)
    {
    }

    EditorViewport2d::~EditorViewport2d()
    {
    }

    void EditorViewport2d::Show()
    {
      m_mouseOverOverlay = false;

      Vec2 size =
      {
        g_app->m_playWidth,
        g_app->m_playHeight
      };

      ImGui::SetNextWindowSize(size);
      if 
      (
        ImGui::Begin(m_name.c_str(), 
        &m_visible, 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking)
      )
      {
        HandleStates();

        // Content area size
        ImVec2 vMin = ImGui::GetWindowContentRegionMin();
        ImVec2 vMax = ImGui::GetWindowContentRegionMax();

        vMin.x += ImGui::GetWindowPos().x;
        vMin.y += ImGui::GetWindowPos().y;
        vMax.x += ImGui::GetWindowPos().x;
        vMax.y += ImGui::GetWindowPos().y;

        m_wndPos.x = vMin.x;
        m_wndPos.y = vMin.y;

        m_wndContentAreaSize = Vec2(glm::abs(vMax.x - vMin.x), glm::abs(vMax.y - vMin.y));

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 absMousePos = io.MousePos;
        m_mouseOverContentArea = false;
        if (vMin.x < absMousePos.x && vMax.x > absMousePos.x)
        {
          if (vMin.y < absMousePos.y && vMax.y > absMousePos.y)
          {
            m_mouseOverContentArea = true;
          }
        }

        m_lastMousePosRelContentArea.x = (int)(absMousePos.x - vMin.x);
        m_lastMousePosRelContentArea.y = (int)(absMousePos.y - vMin.y);

        if (!ImGui::IsWindowCollapsed())
        {
          if (m_wndContentAreaSize.x > 0 && m_wndContentAreaSize.y > 0)
          {
            ImGui::Image((void*)(intptr_t)m_viewportImage->m_textureId, ImVec2(m_width, m_height), ImVec2(0.0f, 0.0f), ImVec2(1.0f, -1.0f));

            if (m_wndContentAreaSize.x != m_width || m_wndContentAreaSize.y != m_height)
            {
              OnResize(m_wndContentAreaSize.x, m_wndContentAreaSize.y);
            }

            if (IsActive())
            {
              ImGui::GetWindowDrawList()->AddRect(vMin, vMax, IM_COL32(255, 255, 0, 255));
            }
            else
            {
              ImGui::GetWindowDrawList()->AddRect(vMin, vMax, IM_COL32(128, 128, 128, 255));
            }
          }
        }

        m_mouseHover = ImGui::IsWindowHovered();

        // Process draw commands.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (auto command : m_drawCommands)
        {
          command(drawList);
        }
        m_drawCommands.clear();

        // AssetBrowser drop handling.
        if (ImGui::BeginDragDropTarget())
        {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BrowserDragZone"))
          {
            IM_ASSERT(payload->DataSize == sizeof(DirectoryEntry));
            DirectoryEntry entry = *(const DirectoryEntry*)payload->Data;

            if (entry.m_ext == MESH)
            {
              String path = ConcatPaths({ entry.m_rootPath, entry.m_fileName + entry.m_ext });
              
              Drawable* dwMesh = new Drawable();
              if (io.KeyShift)
              {
                MeshPtr mesh = GetMeshManager()->Create<Mesh>(path);
                dwMesh->m_mesh = mesh->Copy<Mesh>();
              }
              else
              {
                dwMesh->m_mesh = GetMeshManager()->Create<Mesh>(path);
              }
              
              dwMesh->m_mesh->Init(false);
              Ray ray = RayFromMousePosition();
              Vec3 pos = PointOnRay(ray, 5.0f);
              g_app->m_grid->HitTest(ray, pos);
              dwMesh->m_node->SetTranslation(pos);
              g_app->m_scene->AddEntity(dwMesh);
              g_app->m_scene->AddToSelection(dwMesh->m_id, false);
              SetActive();
            }
            else if (entry.m_ext == SCENE)
            {
              YesNoWindow* importOptionWnd = new YesNoWindow("Open Scene", "Open", "Merge", "Open or merge the scene ?", true);
              importOptionWnd->m_yesCallback = [entry]() ->void
              {
                String fullPath = entry.GetFullPath();
                g_app->OpenScene(fullPath);
              };

              importOptionWnd->m_noCallback = [entry]() -> void
              {
                String fullPath = entry.GetFullPath();
                g_app->MergeScene(fullPath);
              };

              UI::m_volatileWindows.push_back(importOptionWnd);
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (g_app->m_showOverlayUI)
        {
          if (IsActive() || g_app->m_showOverlayUIAlways)
          {
            bool onPlugin = false;
            if (m_name == "Perspective" && g_app->m_gameMod != App::GameMod::Stop)
            {
              if (!g_app->m_runWindowed)
              {
                // Game is being drawn on perspective. Hide overlays.
                onPlugin = true;
              }
            }

            if (m_name == "PlayWindow")
            {
              onPlugin = true;
            }

            if (!onPlugin)
            {
              for (OverlayUI* overlay : m_overlays)
              {
                if (overlay)
                {
                  overlay->m_owner = this;
                  overlay->Show();
                }
              }
            }

          }
        }

      }
      ImGui::End();
    }

    Window::Type EditorViewport2d::GetType() const
    {
      return Type::Viewport2d;
    }

    void EditorViewport2d::Update(float deltaTime)
    {
      if (!IsActive())
      {
        SDL_GetGlobalMouseState(&m_mousePosBegin.x, &m_mousePosBegin.y);
        return;
      }

      OrbitPanMod(deltaTime);
    }

    void EditorViewport2d::OnResize(float width, float height)
    {
      Viewport::OnResize(width, height);
      if (m_camera->IsOrtographic())
      {
        Camera::CamData dat = m_camera->GetData();
        float distToCenter = glm::distance(Vec3(), dat.pos);

        float hDist = distToCenter * 10.0f / 500.0f;
        m_camera->SetLens(dat.aspect, -hDist, hDist, -hDist, hDist, 0.01f, 1000.0f);
      }
    }

    void EditorViewport2d::OrbitPanMod(float deltaTime)
    {
      if (m_camera)
      {
        // Adjust zoom always.
        if (m_mouseOverContentArea)
        {
          float zoom = ImGui::GetIO().MouseWheel;
          if (m_camera->IsOrtographic())
          {
            m_camera->Translate(Vec3(0.0f, 0.0f, -zoom * 10.0f));

            Camera::CamData dat = m_camera->GetData();
            float distToCenter = glm::distance(Vec3(), dat.pos);

            float hDist = distToCenter * 10.0f / 500.0f;
            m_camera->SetLens(dat.aspect, -hDist, hDist, -hDist, hDist, 0.01f, 1000.0f);
          }
          else
          {
            m_camera->Translate(Vec3(0.0f, 0.0f, -zoom));
          }
        }

        static Vec3 orbitPnt;
        static bool hitFound = false;
        static float dist = 0.0f;
        Camera::CamData dat = m_camera->GetData();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
          // Figure out orbiting point.
          Entity* currEntity = g_app->m_scene->GetCurrentSelection();
          if (currEntity == nullptr)
          {
            if (!hitFound)
            {
              Ray orbitRay = RayFromMousePosition();
              EditorScene::PickData pd = g_app->m_scene->PickObject(orbitRay);

              if (pd.entity == nullptr)
              {
                if (!g_app->m_grid->HitTest(orbitRay, orbitPnt))
                {
                  orbitPnt = PointOnRay(orbitRay, 5.0f);
                }
              }
              else
              {
                orbitPnt = pd.pickPos;
              }
              hitFound = true;
              dist = glm::distance(orbitPnt, dat.pos);
            }
          }
          else
          {
            hitFound = true;
            orbitPnt = currEntity->m_node->GetTranslation(TransformationSpace::TS_WORLD);
            dist = glm::distance(orbitPnt, dat.pos);
          }

          // Orbit around it.
          ImGuiIO& io = ImGui::GetIO();
          float x = io.MouseDelta.x;
          float y = io.MouseDelta.y;
          Vec3 r = m_camera->GetRight();
          Vec3 u = m_camera->GetUp();

          if (io.KeyShift)
          {
            // Reflect window space mouse delta to image plane. 
            Vec3 deltaOnImagePlane = glm::unProject
            (
              // Here, mouse delta is transformed to viewport center.
              Vec3(x + m_width * 0.5f, y + m_height * 0.5f, 0.0f),
              Mat4(),
              dat.projection,
              Vec4(0.0f, 0.0f, m_width, m_height)
            );

            // Thales ! Reflect imageplane displacement to world space.
            Vec3 deltaOnWorld = deltaOnImagePlane * dist / dat.nearDist;
            if (m_camera->IsOrtographic())
            {
              deltaOnWorld = deltaOnImagePlane;
            }

            Vec3 displace = r * -deltaOnWorld.x + u * deltaOnWorld.y;
            m_camera->m_node->Translate(displace, TransformationSpace::TS_WORLD);
          }
          else
          {
            if (m_cameraAlignment != 0)
            {
              if (m_cameraAlignment == 1)
              {
                orbitPnt.y = 0.0f;
              }
              else if (m_cameraAlignment == 2)
              {
                orbitPnt.z = 0.0f;
              }
              else if (m_cameraAlignment == 3)
              {
                orbitPnt.x = 0.0f;
              }
            }

            Mat4 camTs = m_camera->m_node->GetTransform(TransformationSpace::TS_WORLD);
            Mat4 ts = glm::translate(Mat4(), orbitPnt);
            Mat4 its = glm::translate(Mat4(), -orbitPnt);
            Quaternion qx = glm::angleAxis(-glm::radians(y * g_app->m_mouseSensitivity), r);
            Quaternion qy = glm::angleAxis(-glm::radians(x * g_app->m_mouseSensitivity), Y_AXIS);

            camTs = ts * glm::toMat4(qy * qx) * its * camTs;
            m_camera->m_node->SetTransform(camTs, TransformationSpace::TS_WORLD);
            m_cameraAlignment = 0;
          }
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
          hitFound = false;
          dist = 0.0f;
        }
      }
    }

  }
}

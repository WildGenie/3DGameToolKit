#pragma once

#include "ToolKit.h"
#include "Drawable.h"
#include "MathUtil.h"
#include "Resource.h"
#include "Events.h"
#include "Types.h"
#include <vector>
#include <functional>

namespace ToolKit
{

  class TK_API Surface : public Drawable
  {
  public:
    Surface();
    Surface(TexturePtr texture, const Vec2& pivotOffset);
    Surface(TexturePtr texture, const SpriteEntry& entry);
    Surface(const String& textureFile, const Vec2& pivotOffset);
    Surface(const Vec2& size, const Vec2& offset = { 0.5f, 0.5f });
    virtual ~Surface();

    virtual EntityType GetType() const override;
    virtual void Serialize(XmlDocument* doc, XmlNode* parent) const override;
    virtual void DeSerialize(XmlDocument* doc, XmlNode* parent) override;

    virtual void ResetCallbacks();
    void UpdateGeometry(bool byTexture); // To reflect the size & pivot changes, this function regenerates the geometry.

  protected:
    virtual Entity* CopyTo(Entity* copyTo) const override;

  private:
    void CreateQuat();
    void CreateQuat(const SpriteEntry& val);
    void SetSizeFromTexture();

  public:
    Vec2 m_size;
    Vec2 m_pivotOffset;

    // UI states.
    bool m_mouseOver = false;
    bool m_mouseClicked = false;

    // Event Callbacks.
    SurfaceEventCallback m_onMouseEnter = nullptr;
    SurfaceEventCallback m_onMouseExit = nullptr;
    SurfaceEventCallback m_onMouseOver = nullptr;
    SurfaceEventCallback m_onMouseClick = nullptr;
  };

  class TK_API Button : public Surface
  {
  public:
    Button();
    Button(const Vec2& size);
    Button(const TexturePtr& buttonImage, const TexturePtr& mouseOverImage);
    virtual ~Button();
    virtual EntityType GetType() const override;
    virtual void Serialize(XmlDocument* doc, XmlNode* parent) const override;
    virtual void DeSerialize(XmlDocument* doc, XmlNode* parent) override;
    virtual void ResetCallbacks() override;

  public:
    TexturePtr m_mouseOverImage;
    TexturePtr m_buttonImage;

    // Local events.
    SurfaceEventCallback m_onMouseEnterLocal;
    SurfaceEventCallback m_onMouseExitLocal;
  };

  class Viewport;


}
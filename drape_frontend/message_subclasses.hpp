#pragma once

#include "drape_frontend/gui/country_status_helper.hpp"
#include "drape_frontend/gui/layer_render.hpp"
#include "drape_frontend/gui/skin.hpp"

#include "drape_frontend/route_builder.hpp"
#include "drape_frontend/my_position.hpp"
#include "drape_frontend/selection_shape.hpp"
#include "drape_frontend/message.hpp"
#include "drape_frontend/viewport.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/user_marks_provider.hpp"

#include "geometry/polyline2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/screenbase.hpp"

#include "drape/glstate.hpp"
#include "drape/pointers.hpp"
#include "drape/render_bucket.hpp"

#include "platform/location.hpp"

#include "std/condition_variable.hpp"
#include "std/shared_ptr.hpp"
#include "std/set.hpp"
#include "std/function.hpp"
#include "std/utility.hpp"

namespace df
{

class BaseBlockingMessage : public Message
{
public:
  struct Blocker
  {
    void Wait()
    {
      unique_lock<mutex> lock(m_lock);
      m_signal.wait(lock, [this]{return !m_blocked;} );
    }

  private:
    friend class BaseBlockingMessage;

    void Signal()
    {
      lock_guard<mutex> lock(m_lock);
      m_blocked = false;
      m_signal.notify_one();
    }

  private:
    mutex m_lock;
    condition_variable m_signal;
    bool m_blocked = true;
  };

  BaseBlockingMessage(Blocker & blocker)
    : m_blocker(blocker)
  {
  }

  ~BaseBlockingMessage()
  {
    m_blocker.Signal();
  }

private:
  Blocker & m_blocker;
};

class BaseTileMessage : public Message
{
public:
  BaseTileMessage(TileKey const & key)
    : m_tileKey(key) {}

  TileKey const & GetKey() const { return m_tileKey; }

private:
  TileKey m_tileKey;
};

class TileReadStartMessage : public BaseTileMessage
{
public:
  TileReadStartMessage(TileKey const & key)
    : BaseTileMessage(key) {}

  Type GetType() const override { return Message::TileReadStarted; }
};

class TileReadEndMessage : public BaseTileMessage
{
public:
  TileReadEndMessage(TileKey const & key)
    : BaseTileMessage(key) {}

  Type GetType() const override { return Message::TileReadEnded; }
};

class FinishReadingMessage : public Message
{
public:
  template<typename T> FinishReadingMessage(T && tiles)
    : m_tiles(forward<T>(tiles))
  {}

  Type GetType() const override { return Message::FinishReading; }

  TTilesCollection const & GetTiles() { return m_tiles; }
  TTilesCollection && MoveTiles() { return move(m_tiles); }

private:
  TTilesCollection m_tiles;
};

class FlushRenderBucketMessage : public BaseTileMessage
{
public:
  FlushRenderBucketMessage(TileKey const & key, dp::GLState const & state, drape_ptr<dp::RenderBucket> && buffer)
    : BaseTileMessage(key)
    , m_state(state)
    , m_buffer(move(buffer))
  {}

  Type GetType() const override { return Message::FlushTile; }

  dp::GLState const & GetState() const { return m_state; }
  drape_ptr<dp::RenderBucket> && AcceptBuffer() { return move(m_buffer); }

private:
  dp::GLState m_state;
  drape_ptr<dp::RenderBucket> m_buffer;
};

class InvalidateRectMessage : public Message
{
public:
  InvalidateRectMessage(m2::RectD const & rect)
    : m_rect(rect) {}

  Type GetType() const override { return Message::InvalidateRect; }

  m2::RectD const & GetRect() const { return m_rect; }

private:
  m2::RectD m_rect;
};

class UpdateReadManagerMessage : public Message
{
public:
  UpdateReadManagerMessage(){}
  Type GetType() const override { return Message::UpdateReadManager; }
};

class InvalidateReadManagerRectMessage : public BaseBlockingMessage
{
public:
  InvalidateReadManagerRectMessage(Blocker & blocker, TTilesCollection const & tiles)
    : BaseBlockingMessage(blocker)
    , m_tiles(tiles)
  {}

  Type GetType() const override { return Message::InvalidateReadManagerRect; }

  TTilesCollection const & GetTilesForInvalidate() const { return m_tiles; }

private:
  TTilesCollection m_tiles;
};

class ClearUserMarkLayerMessage : public BaseTileMessage
{
public:
  ClearUserMarkLayerMessage(TileKey const & tileKey)
    : BaseTileMessage(tileKey) {}

  Type GetType() const override { return Message::ClearUserMarkLayer; }
};

class ChangeUserMarkLayerVisibilityMessage : public BaseTileMessage
{
public:
  ChangeUserMarkLayerVisibilityMessage(TileKey const & tileKey, bool isVisible)
    : BaseTileMessage(tileKey)
    , m_isVisible(isVisible) {}

  Type GetType() const override { return Message::ChangeUserMarkLayerVisibility; }

  bool IsVisible() const { return m_isVisible; }

private:
  bool m_isVisible;
};

class UpdateUserMarkLayerMessage : public BaseTileMessage
{
public:
  UpdateUserMarkLayerMessage(TileKey const & tileKey, UserMarksProvider * provider)
    : BaseTileMessage(tileKey)
    , m_provider(provider)
  {
    m_provider->IncrementCounter();
  }

  ~UpdateUserMarkLayerMessage()
  {
    ASSERT(m_inProcess == false, ());
    m_provider->DecrementCounter();
    if (m_provider->IsPendingOnDelete() && m_provider->CanBeDeleted())
      delete m_provider;
  }

  Type GetType() const override { return Message::UpdateUserMarkLayer; }

  UserMarksProvider const * StartProcess()
  {
    m_provider->BeginRead();
#ifdef DEBUG
    m_inProcess = true;
#endif
    return m_provider;
  }

  void EndProcess()
  {
#ifdef DEBUG
    m_inProcess = false;
#endif
    m_provider->EndRead();
  }

private:
  UserMarksProvider * m_provider;
#ifdef DEBUG
  bool m_inProcess;
#endif
};

class GuiLayerRecachedMessage : public Message
{
public:
  GuiLayerRecachedMessage(drape_ptr<gui::LayerRenderer> && renderer)
    : m_renderer(move(renderer)) {}

  Type GetType() const override { return Message::GuiLayerRecached; }

  drape_ptr<gui::LayerRenderer> && AcceptRenderer() { return move(m_renderer); }

private:
  drape_ptr<gui::LayerRenderer> m_renderer;
};

class GuiRecacheMessage : public BaseBlockingMessage
{
public:
  GuiRecacheMessage(Blocker & blocker, gui::TWidgetsInitInfo const & initInfo, gui::TWidgetsSizeInfo & resultInfo)
    : BaseBlockingMessage(blocker)
    , m_initInfo(initInfo)
    , m_sizeInfo(resultInfo)
  {
  }

  Type GetType() const override { return Message::GuiRecache;}
  gui::TWidgetsInitInfo const & GetInitInfo() const { return m_initInfo; }
  gui::TWidgetsSizeInfo & GetSizeInfoMap() const { return m_sizeInfo; }

private:
  gui::TWidgetsInitInfo m_initInfo;
  gui::TWidgetsSizeInfo & m_sizeInfo;
};

class GuiLayerLayoutMessage : public Message
{
public:
  GuiLayerLayoutMessage(gui::TWidgetsLayoutInfo const & info)
    : m_layoutInfo(info)
  {}

  Type GetType() const override { return GuiLayerLayout; }

  gui::TWidgetsLayoutInfo const & GetLayoutInfo() const { return m_layoutInfo; }
  gui::TWidgetsLayoutInfo AcceptLayoutInfo() { return move(m_layoutInfo); }

private:
  gui::TWidgetsLayoutInfo m_layoutInfo;
};

class CountryInfoUpdateMessage : public Message
{
public:
  CountryInfoUpdateMessage()
    : m_needShow(false)
  {}

  CountryInfoUpdateMessage(gui::CountryInfo const & info, bool isCurrentCountry)
    : m_countryInfo(info)
    , m_isCurrentCountry(isCurrentCountry)
    , m_needShow(true)
  {}

  Type GetType() const override { return Message::CountryInfoUpdate;}
  gui::CountryInfo const & GetCountryInfo() const { return m_countryInfo; }
  bool IsCurrentCountry() const { return m_isCurrentCountry; }
  bool NeedShow() const { return m_needShow; }

private:
  gui::CountryInfo m_countryInfo;
  bool m_isCurrentCountry;
  bool m_needShow;
};

class CountryStatusRecacheMessage : public Message
{
public:
  CountryStatusRecacheMessage() {}
  Type GetType() const override { return Message::CountryStatusRecache ;}
};

class MyPositionShapeMessage : public Message
{
public:
  MyPositionShapeMessage(drape_ptr<MyPosition> && shape, drape_ptr<SelectionShape> && selection)
    : m_shape(move(shape))
    , m_selection(move(selection))
  {}

  Type GetType() const override { return Message::MyPositionShape; }

  drape_ptr<MyPosition> && AcceptShape() { return move(m_shape); }
  drape_ptr<SelectionShape> AcceptSelection() { return move(m_selection); }

private:
  drape_ptr<MyPosition> m_shape;
  drape_ptr<SelectionShape> m_selection;
};

class StopRenderingMessage : public Message
{
public:
  StopRenderingMessage(){}
  Type GetType() const override { return Message::StopRendering; }
};

class ChangeMyPositionModeMessage : public Message
{
public:
  enum EChangeType
  {
    TYPE_NEXT,
    TYPE_CANCEL,
    TYPE_STOP_FOLLOW,
    TYPE_INVALIDATE
  };

  explicit ChangeMyPositionModeMessage(EChangeType changeType)
    : m_changeType(changeType)
    , m_preferredZoomLevel(-1)
  {}

  explicit ChangeMyPositionModeMessage(EChangeType changeType, int zoomLevel)
    : m_changeType(changeType)
    , m_preferredZoomLevel(zoomLevel)
  {}

  EChangeType GetChangeType() const { return m_changeType; }
  Type GetType() const override { return Message::ChangeMyPostitionMode; }

  int GetPreferredZoomLevel() const { return m_preferredZoomLevel; }

private:
  EChangeType const m_changeType;
  int m_preferredZoomLevel;
};

class CompassInfoMessage : public Message
{
public:
  CompassInfoMessage(location::CompassInfo const & info)
    : m_info(info)
  {}

  Type GetType() const override { return Message::CompassInfo; }
  location::CompassInfo const & GetInfo() const { return m_info; }

private:
  location::CompassInfo const m_info;
};

class GpsInfoMessage : public Message
{
public:
  GpsInfoMessage(location::GpsInfo const & info, bool isNavigable,
                 location::RouteMatchingInfo const & routeInfo)
    : m_info(info)
    , m_isNavigable(isNavigable)
    , m_routeInfo(routeInfo)
  {}

  Type GetType() const override { return Message::GpsInfo; }
  location::GpsInfo const & GetInfo() const { return m_info; }
  bool IsNavigable() const { return m_isNavigable; }
  location::RouteMatchingInfo const & GetRouteInfo() const { return m_routeInfo; }

private:
  location::GpsInfo const m_info;
  bool const m_isNavigable;
  location::RouteMatchingInfo const m_routeInfo;
};

class FindVisiblePOIMessage : public BaseBlockingMessage
{
public:
  FindVisiblePOIMessage(Blocker & blocker, m2::PointD const & glbPt, FeatureID & featureID)
    : BaseBlockingMessage(blocker)
    , m_pt(glbPt)
    , m_featureID(featureID)
  {}

  Type GetType() const override { return FindVisiblePOI; }

  m2::PointD const & GetPoint() const { return m_pt; }
  void SetFeatureID(FeatureID const & id)
  {
    m_featureID = id;
  }

private:
  m2::PointD m_pt;
  FeatureID & m_featureID;
};

class SelectObjectMessage : public Message
{
public:
  struct DismissTag {};
  SelectObjectMessage(DismissTag)
    : SelectObjectMessage(SelectionShape::OBJECT_EMPTY, m2::PointD::Zero(), false, true)
  {}

  SelectObjectMessage(SelectionShape::ESelectedObject selectedObject, m2::PointD const & glbPoint, bool isAnim)
    : SelectObjectMessage(selectedObject, glbPoint, isAnim, false)
  {}

  Type GetType() const override { return SelectObject; }
  m2::PointD const & GetPosition() const { return m_glbPoint; }
  SelectionShape::ESelectedObject GetSelectedObject() const { return m_selected; }
  bool IsAnim() const { return m_isAnim; }
  bool IsDismiss() const { return m_isDismiss; }

private:
  SelectObjectMessage(SelectionShape::ESelectedObject obj, m2::PointD const & pt, bool isAnim, bool isDismiss)
    : m_selected(obj)
    , m_glbPoint(pt)
    , m_isAnim(isAnim)
    , m_isDismiss(isDismiss)
  {}

private:
  SelectionShape::ESelectedObject m_selected;
  m2::PointD m_glbPoint;
  bool m_isAnim;
  bool m_isDismiss;
};

class GetSelectedObjectMessage : public BaseBlockingMessage
{
public:
  GetSelectedObjectMessage(Blocker & blocker, SelectionShape::ESelectedObject & object)
    : BaseBlockingMessage(blocker)
    , m_object(object)
  {}

  Type GetType() const override { return GetSelectedObject; }

  void SetSelectedObject(SelectionShape::ESelectedObject const & object)
  {
    m_object = object;
  }

private:
  SelectionShape::ESelectedObject & m_object;
};

class GetMyPositionMessage : public BaseBlockingMessage
{
public:
  GetMyPositionMessage(Blocker & blocker, bool & hasPosition, m2::PointD & myPosition)
    : BaseBlockingMessage(blocker)
    , m_myPosition(myPosition)
    , m_hasPosition(hasPosition)
  {}

  Type GetType() const override { return GetMyPosition; }

  void SetMyPosition(bool hasPosition, m2::PointD const & myPosition)
  {
    m_hasPosition = hasPosition;
    m_myPosition = myPosition;
  }

private:
  m2::PointD & m_myPosition;
  bool & m_hasPosition;
};

class AddRouteMessage : public Message
{
public:
  AddRouteMessage(m2::PolylineD const & routePolyline, vector<double> const & turns, dp::Color const & color)
    : m_routePolyline(routePolyline)
    , m_color(color)
    , m_turns(turns)
  {}

  Type GetType() const override { return Message::AddRoute; }

  m2::PolylineD const & GetRoutePolyline() { return m_routePolyline; }
  dp::Color const & GetColor() const { return m_color; }
  vector<double> const & GetTurns() const { return m_turns; }

private:
  m2::PolylineD m_routePolyline;
  dp::Color m_color;
  vector<double> m_turns;
};

class CacheRouteSignMessage : public Message
{
public:
  CacheRouteSignMessage(m2::PointD const & pos, bool isStart, bool isValid)
    : m_position(pos)
    , m_isStart(isStart)
    , m_isValid(isValid)
  {}

  Type GetType() const override { return Message::CacheRouteSign; }

  m2::PointD const & GetPosition() const { return m_position; }
  bool IsStart() const { return m_isStart; }
  bool IsValid() const { return m_isValid; }

private:
  m2::PointD const m_position;
  bool const m_isStart;
  bool const m_isValid;
};

class RemoveRouteMessage : public Message
{
public:
  RemoveRouteMessage(bool deactivateFollowing)
    : m_deactivateFollowing(deactivateFollowing)
  {}

  Type GetType() const override { return Message::RemoveRoute; }

  bool NeedDeactivateFollowing() const { return m_deactivateFollowing; }

private:
  bool m_deactivateFollowing;
};

class FlushRouteMessage : public Message
{
public:
  FlushRouteMessage(drape_ptr<RouteData> && routeData)
    : m_routeData(move(routeData))
  {}

  Type GetType() const override { return Message::FlushRoute; }
  drape_ptr<RouteData> && AcceptRouteData() { return move(m_routeData); }

private:
  drape_ptr<RouteData> m_routeData;
};

class FlushRouteSignMessage : public Message
{
public:
  FlushRouteSignMessage(drape_ptr<RouteSignData> && routeSignData)
    : m_routeSignData(move(routeSignData))
  {}

  Type GetType() const override { return Message::FlushRouteSign; }
  drape_ptr<RouteSignData> && AcceptRouteSignData() { return move(m_routeSignData); }

private:
  drape_ptr<RouteSignData> m_routeSignData;
};

class UpdateMapStyleMessage : public BaseBlockingMessage
{
public:
  UpdateMapStyleMessage(Blocker & blocker)
    : BaseBlockingMessage(blocker)
  {}

  Type GetType() const override { return Message::UpdateMapStyle; }
};

class InvalidateTexturesMessage : public BaseBlockingMessage
{
public:
  InvalidateTexturesMessage(Blocker & blocker)
    : BaseBlockingMessage(blocker)
  {}

  Type GetType() const override { return Message::InvalidateTextures; }
};

class InvalidateMessage : public Message
{
public:
  InvalidateMessage(){}

  Type GetType() const override { return Message::Invalidate; }
};

class DeactivateRouteFollowingMessage : public Message
{
public:
  DeactivateRouteFollowingMessage(){}

  Type GetType() const override { return Message::DeactivateRouteFollowing; }
};

} // namespace df

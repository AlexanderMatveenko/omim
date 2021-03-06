#pragma once

#include "skin.hpp"
#include "compass.hpp"
#include "country_status.hpp"

#include "storage/index.hpp"
#include "storage/storage_defines.hpp"

#include "drape/pointers.hpp"

#include "std/function.hpp"
#include "std/unique_ptr.hpp"
#include "std/mutex.hpp"

class ScreenBase;

namespace gui
{

class RulerHelper;
class CountryStatusHelper;

class DrapeGui
{
public:
  using TRecacheCountryStatusSlot = function<void ()>;
  using TLocalizeStringFn = function<string (string const &)>;

  static DrapeGui & Instance();
  static RulerHelper & GetRulerHelper();
  static CountryStatusHelper & GetCountryStatusHelper();

  static dp::FontDecl const & GetGuiTextFont();

  void SetLocalizator(TLocalizeStringFn const & fn);
  void Destroy();
  void SetSurfaceSize(m2::PointF const & size);
  m2::PointF GetSurfaceSize() const;

  string GetLocalizedString(string const & stringID) const;

  void SetRecacheCountryStatusSlot(TRecacheCountryStatusSlot const & fn);
  void EmitRecacheCountryStatusSignal();
  void ClearRecacheCountryStatusSlot();

  bool IsInUserAction() const { return m_inUserAction; }
  void SetInUserAction(bool isInUserAction) { m_inUserAction = isInUserAction; }

  bool IsCopyrightActive() const { return m_isCopyrightActive; }
  void DeactivateCopyright() { m_isCopyrightActive = false; }

  void ConnectOnCompassTappedHandler(Shape::TTapHandler const & handler);
  void ConnectOnButtonPressedHandler(CountryStatusHelper::EButtonType buttonType,
                                     Shape::TTapHandler const & handler);
  void CallOnCompassTappedHandler();
  void CallOnButtonPressedHandler(CountryStatusHelper::EButtonType buttonType);

private:
  DrapeGui();

  RulerHelper & GetRulerHelperImpl();
  CountryStatusHelper & GetCountryStatusHelperImpl();

private:
  struct Impl;
  unique_ptr<Impl> m_impl;
  bool m_isCopyrightActive = true;

  Shape::TTapHandler m_onCompassTappedHandler;
  CountryStatus::TButtonHandlers m_buttonHandlers;
  m2::PointF m_surfaceSize;
  mutable mutex m_surfaceSizeMutex;
  bool m_inUserAction = false;
};

}

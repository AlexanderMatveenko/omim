#include "drape_frontend/rule_drawer.hpp"
#include "drape_frontend/stylist.hpp"
#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/apply_feature_functors.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"

#include "base/assert.hpp"
#include "std/bind.hpp"

namespace df
{

RuleDrawer::RuleDrawer(TDrawerCallback const & fn, EngineContext & context)
  : m_callback(fn)
  , m_context(context)
{
  m_globalRect = context.GetTileKey().GetGlobalRect();

  int32_t tileSize = df::VisualParams::Instance().GetTileSize();
  m_geometryConvertor.OnSize(0, 0, tileSize, tileSize);
  m_geometryConvertor.SetFromRect(m2::AnyRectD(m_globalRect));
  m_currentScaleGtoP = 1.0f / m_geometryConvertor.GetScale();
}

void RuleDrawer::operator()(FeatureType const & f)
{
  Stylist s;
  m_callback(f, s);

  if (s.IsEmpty())
    return;

  if (s.IsCoastLine() && (!m_coastlines.insert(s.GetCaptionDescription().GetMainText()).second))
    return;

#ifdef DEBUG
  // Validate on feature styles
  if (s.AreaStyleExists() == false)
  {
    int checkFlag = s.PointStyleExists() ? 1 : 0;
    checkFlag += s.LineStyleExists() ? 1 : 0;
    ASSERT(checkFlag == 1, ());
  }
#endif

  int zoomLevel = m_context.GetTileKey().m_zoomLevel;

  if (s.AreaStyleExists())
  {
    ApplyAreaFeature apply(m_context, f.GetID(), s.GetCaptionDescription());
    f.ForEachTriangleRef(apply, zoomLevel);

    if (s.PointStyleExists())
      apply(feature::GetCenter(f, zoomLevel));

    s.ForEachRule(bind(&ApplyAreaFeature::ProcessRule, &apply, _1));
    apply.Finish();
  }
  else if (s.LineStyleExists())
  {
    ApplyLineFeature apply(m_context, f.GetID(), s.GetCaptionDescription(),
                           m_currentScaleGtoP);
    f.ForEachPointRef(apply, zoomLevel);

    if (apply.HasGeometry())
      s.ForEachRule(bind(&ApplyLineFeature::ProcessRule, &apply, _1));
    apply.Finish();
  }
  else
  {
    ASSERT(s.PointStyleExists(), ());
    ApplyPointFeature apply(m_context, f.GetID(), s.GetCaptionDescription());
    f.ForEachPointRef(apply, zoomLevel);

    s.ForEachRule(bind(&ApplyPointFeature::ProcessRule, &apply, _1));
    apply.Finish();
  }

  m_context.Flush();
}

} // namespace df

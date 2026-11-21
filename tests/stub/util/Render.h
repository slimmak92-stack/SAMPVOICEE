#pragma once
namespace Render {
  inline bool ConvertBaseXValueToScreenXValue(float v, float& o){ o=v; return true; }
  inline bool ConvertBaseYValueToScreenYValue(float v, float& o){ o=v; return true; }
  inline bool ConvertScreenXValueToBaseXValue(float v, float& o){ o=v; return true; }
  inline bool ConvertScreenYValueToBaseYValue(float v, float& o){ o=v; return true; }
}

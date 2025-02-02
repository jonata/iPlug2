/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IGraphicsCanvas.h"
#include <string>
#include <utility>
#include <stdio.h>
#include <type_traits>
#include <emscripten.h>

#include "wdl_base64.h"

using namespace emscripten;

extern IGraphicsWeb* gGraphics;

extern val GetPreloadedImages();
extern val GetCanvas();

struct CanvasFont
{
  using FontDesc = std::remove_pointer<FontDescriptor>::type;
  
  CanvasFont(FontDesc descriptor, double ascenderRatio, double EMRatio)
  : mDescriptor(descriptor), mAscenderRatio(ascenderRatio), mEMRatio(EMRatio) {}
    
  FontDesc mDescriptor;
  double mAscenderRatio;
  double mEMRatio;
};

std::string GetFontString(const char* fontName, const char* styleName, double size)
{
  WDL_String fontString;
  fontString.SetFormatted(FONT_LEN + 64, "%s %lfpx %s", styleName, size, fontName);
  return std::string(fontString.Get());
}

StaticStorage<CanvasFont> sFontCache;

static std::string CanvasColor(const IColor& color, float alpha = 1.0)
{
  WDL_String str;
  str.SetFormatted(64, "rgba(%d, %d, %d, %lf)", color.R, color.G, color.B, alpha * color.A / 255.0);
  return str.Get();
}

CanvasBitmap::CanvasBitmap(val imageCanvas, const char* name, int scale)
{
  SetBitmap(new val(imageCanvas), imageCanvas["width"].as<int>(), imageCanvas["height"].as<int>(), scale, 1.f);
}

CanvasBitmap::CanvasBitmap(int width, int height, int scale, float drawScale)
{
  val canvas = val::global("document").call<val>("createElement", std::string("canvas"));
  canvas.set("width", width);
  canvas.set("height", height);

  SetBitmap(new val(canvas), width, height, scale, drawScale);
}

CanvasBitmap::~CanvasBitmap()
{
  delete GetBitmap();
}

IGraphicsCanvas::IGraphicsCanvas(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
: IGraphicsPathBase(dlg, w, h, fps, scale)
{
  StaticStorage<CanvasFont>::Accessor storage(sFontCache);
  storage.Retain();
}

IGraphicsCanvas::~IGraphicsCanvas()
{
  StaticStorage<CanvasFont>::Accessor storage(sFontCache);
  storage.Release();
}

void IGraphicsCanvas::DrawBitmap(const IBitmap& bitmap, const IRECT& bounds, int srcX, int srcY, const IBlend* pBlend)
{
  val context = GetContext();
  val img = *bitmap.GetAPIBitmap()->GetBitmap();
  context.call<void>("save");
  SetCanvasBlendMode(context, pBlend);
  context.set("globalAlpha", BlendWeight(pBlend));
    
  const float bs = bitmap.GetScale();
  IRECT sr = bounds;
  sr.Scale(bs * bitmap.GetDrawScale());

  PathRect(bounds);
  context.call<void>("clip");
  context.call<void>("drawImage", img, srcX * bs, srcY * bs, sr.W(), sr.H(), bounds.L, bounds.T, bounds.W(), bounds.H());
  GetContext().call<void>("restore");
}

void IGraphicsCanvas::PathClear()
{
  GetContext().call<void>("beginPath");
}

void IGraphicsCanvas::PathClose()
{
  GetContext().call<void>("closePath");
}

void IGraphicsCanvas::PathArc(float cx, float cy, float r, float a1, float a2, EWinding winding)
{
  GetContext().call<void>("arc", cx, cy, r, DegToRad(a1 - 90.f), DegToRad(a2 - 90.f), winding == EWinding::CCW);
}

void IGraphicsCanvas::PathMoveTo(float x, float y)
{
  GetContext().call<void>("moveTo", x, y);
}

void IGraphicsCanvas::PathLineTo(float x, float y)
{
  GetContext().call<void>("lineTo", x, y);
}

void IGraphicsCanvas::PathCubicBezierTo(float c1x, float c1y, float c2x, float c2y, float x2, float y2)
{
  GetContext().call<void>("bezierCurveTo", c1x, c1y, c2x, c2y, x2, y2);
}

void IGraphicsCanvas::PathQuadraticBezierTo(float cx, float cy, float x2, float y2)
{
  GetContext().call<void>("quadraticCurveTo", cx, cy, x2, y2);
}

void IGraphicsCanvas::PathStroke(const IPattern& pattern, float thickness, const IStrokeOptions& options, const IBlend* pBlend)
{
  val context = GetContext();
  
  switch (options.mCapOption)
  {
    case ELineCap::Butt: context.set("lineCap", "butt"); break;
    case ELineCap::Round: context.set("lineCap", "round"); break;
    case ELineCap::Square: context.set("lineCap", "square"); break;
  }
  
  switch (options.mJoinOption)
  {
    case ELineJoin::Miter: context.set("lineJoin", "miter"); break;
    case ELineJoin::Round: context.set("lineJoin", "round"); break;
    case ELineJoin::Bevel: context.set("lineJoin", "bevel"); break;
  }
  
  context.set("miterLimit", options.mMiterLimit);
    
  val dashArray = val::array();
  
  for (int i = 0; i < options.mDash.GetCount(); i++)
    dashArray.call<void>("push", val(*(options.mDash.GetArray() + i)));
  
  context.call<void>("setLineDash", dashArray);
  context.set("lineDashOffset", options.mDash.GetOffset());
  context.set("lineWidth", thickness);
  
  SetCanvasSourcePattern(context, pattern, pBlend);

  context.call<void>("stroke");
  
  if (!options.mPreserve)
    PathClear();
}

void IGraphicsCanvas::PathFill(const IPattern& pattern, const IFillOptions& options, const IBlend* pBlend)
{
  val context = GetContext();
  std::string fillRule(options.mFillRule == EFillRule::Winding ? "nonzero" : "evenodd");
  
  SetCanvasSourcePattern(context, pattern, pBlend);

  context.call<void>("fill", fillRule);

  if (!options.mPreserve)
    PathClear();
}

void IGraphicsCanvas::SetCanvasSourcePattern(val& context, const IPattern& pattern, const IBlend* pBlend)
{
  SetCanvasBlendMode(context, pBlend);
  
  switch (pattern.mType)
  {
    case EPatternType::Solid:
    {
      const IColor color = pattern.GetStop(0).mColor;
      std::string colorString = CanvasColor(color, BlendWeight(pBlend));

      context.set("fillStyle", colorString);
      context.set("strokeStyle", colorString);
    }
    break;
      
    case EPatternType::Linear:
    case EPatternType::Radial:
    {
      double x, y;
      IMatrix m = IMatrix(pattern.mTransform).Invert();
      m.TransformPoint(x, y, 0.0, 1.0);
        
      val gradient = (pattern.mType == EPatternType::Linear) ?
        context.call<val>("createLinearGradient", m.mTX, m.mTY, x, y) :
        context.call<val>("createRadialGradient", m.mTX, m.mTY, 0.0, m.mTX, m.mTY, m.mXX);
      
      for (int i = 0; i < pattern.NStops(); i++)
      {
        const IColorStop& stop = pattern.GetStop(i);
        gradient.call<void>("addColorStop", stop.mOffset, CanvasColor(stop.mColor));
      }
      
      context.set("fillStyle", gradient);
      context.set("strokeStyle", gradient);
    }
    break;
  }
}

void IGraphicsCanvas::SetCanvasBlendMode(val& context, const IBlend* pBlend)
{
  if (!pBlend)
    context.set("globalCompositeOperation", "source-over");
  
  switch (pBlend->mMethod)
  {
    case EBlend::Default:       // fall through
    case EBlend::Clobber:       // fall through
    case EBlend::SourceOver:    context.set("globalCompositeOperation", "source-over");        break;
    case EBlend::SourceIn:      context.set("globalCompositeOperation", "source-in");          break;
    case EBlend::SourceOut:     context.set("globalCompositeOperation", "source-out");         break;
    case EBlend::SourceAtop:    context.set("globalCompositeOperation", "source-atop");        break;
    case EBlend::DestOver:      context.set("globalCompositeOperation", "destination-over");   break;
    case EBlend::DestIn:        context.set("globalCompositeOperation", "destination-in");     break;
    case EBlend::DestOut:       context.set("globalCompositeOperation", "destination-out");    break;
    case EBlend::DestAtop:      context.set("globalCompositeOperation", "destination-atop");   break;
    case EBlend::Add:           context.set("globalCompositeOperation", "lighter");            break;
    case EBlend::XOR:           context.set("globalCompositeOperation", "xor");                break;
  }
}

void IGraphicsCanvas::PrepareAndMeasureText(const IText& text, const char* str, IRECT& r, double& x, double & y) const
{
  StaticStorage<CanvasFont>::Accessor storage(sFontCache);
  CanvasFont* pFont = storage.Find(text.mFont);
    
  assert(pFont && "No font found - did you forget to load it?");
  
  FontDescriptor descriptor = &pFont->mDescriptor;
  val context = GetContext();
  std::string fontString = GetFontString(descriptor->first.Get(), descriptor->second.Get(), text.mSize * pFont->mEMRatio);
  
  context.set("font", fontString);
  
  const double textWidth = context.call<val>("measureText", std::string(str))["width"].as<double>();
  const double textHeight = text.mSize;
  const double ascender = pFont->mAscenderRatio * textHeight;
  const double descender = -(1.0 - pFont->mAscenderRatio) * textHeight;
  
  switch (text.mAlign)
  {
    case EAlign::Near:     x = r.L;                          break;
    case EAlign::Center:   x = r.MW() - (textWidth / 2.0);   break;
    case EAlign::Far:      x = r.R - textWidth;              break;
  }
  
  switch (text.mVAlign)
  {
    case EVAlign::Top:      y = r.T + ascender;                            break;
    case EVAlign::Middle:   y = r.MH() + descender + (textHeight / 2.0);   break;
    case EVAlign::Bottom:   y = r.B + descender;                           break;
  }
  
  r = IRECT((float) x, (float) (y - ascender), (float) (x + textWidth), (float) (y + textHeight - ascender));
}

void IGraphicsCanvas::DoMeasureText(const IText& text, const char* str, IRECT& bounds) const
{
  IRECT r = bounds;
  double x, y;
  PrepareAndMeasureText(text, str, bounds, x, y);
  DoMeasureTextRotation(text, r, bounds);
}

void IGraphicsCanvas::DoDrawText(const IText& text, const char* str, const IRECT& bounds, const IBlend* pBlend)
{
  IRECT measured = bounds;
  val context = GetContext();
  double x, y;
  
  PrepareAndMeasureText(text, str, measured, x, y);
  PathTransformSave();
  DoTextRotation(text, bounds, measured);
  context.set("textBaseline", std::string("alphabetic"));
  SetCanvasSourcePattern(context, text.mFGColor, pBlend);
  context.call<void>("fillText", std::string(str), x, y);
  PathTransformRestore();
}

void IGraphicsCanvas::PathTransformSetMatrix(const IMatrix& m)
{
  const double scale = GetBackingPixelScale();
  IMatrix t = IMatrix().Scale(scale, scale).Translate(XTranslate(), YTranslate()).Transform(m);

  GetContext().call<void>("setTransform", t.mXX, t.mYX, t.mXY, t.mYY, t.mTX, t.mTY);
}

void IGraphicsCanvas::SetClipRegion(const IRECT& r)
{
  val context = GetContext();
  context.call<void>("restore");
  context.call<void>("save");
  if (!r.Empty())
  {
    context.call<void>("beginPath");
    context.call<void>("rect", r.L, r.T, r.W(), r.H());
    context.call<void>("clip");
    context.call<void>("beginPath");
  }
}

bool IGraphicsCanvas::BitmapExtSupported(const char* ext)
{
  char extLower[32];
  ToLower(extLower, ext);
  return (strstr(extLower, "png") != nullptr) || (strstr(extLower, "jpg") != nullptr) || (strstr(extLower, "jpeg") != nullptr);
}

APIBitmap* IGraphicsCanvas::LoadAPIBitmap(const char* fileNameOrResID, int scale, EResourceLocation location, const char* ext)
{
  return new CanvasBitmap(GetPreloadedImages()[fileNameOrResID], fileNameOrResID + 1, scale);
}

APIBitmap* IGraphicsCanvas::CreateAPIBitmap(int width, int height, int scale, double drawScale)
{
  return new CanvasBitmap(width, height, scale, drawScale);
}

void IGraphicsCanvas::GetFontMetrics(const char* font, const char* style, double& ascenderRatio, double& EMRatio)
{
  // Provides approximate font metrics for a system font (until text metrics are properly supported)
  int size = 1000;
  std::string fontString = GetFontString(font, style, size);
  
  val document = val::global("document");
  val textSpan = document.call<val>("createElement", std::string("span"));
  textSpan.set("innerHTML", std::string("M"));
  textSpan["style"].set("font", fontString);
  
  val block = document.call<val>("createElement", std::string("div"));
  block["style"].set("display", std::string("inline-block"));
  block["style"].set("width", std::string("1px"));
  block["style"].set("height", std::string("0px"));
  
  val div = document.call<val>("createElement", std::string("div"));
  div.call<void>("appendChild", textSpan);
  div.call<void>("appendChild", block);
  document["body"].call<void>("appendChild", div);
  
  block["style"].set("vertical-align", std::string("baseline"));
  double ascent = block["offsetTop"].as<double>() - textSpan["offsetTop"].as<double>();
  double height = textSpan.call<val>("getBoundingClientRect")["height"].as<double>();
  document["body"].call<void>("removeChild", div);
  
  EMRatio = size / height;
  ascenderRatio = ascent / height;
}

bool IGraphicsCanvas::CompareFontMetrics(const char* style, const char* font1, const char* font2)
{
  WDL_String fontCombination;
  fontCombination.SetFormatted(FONT_LEN * 2 + 2, "%s, %s", font1, font2);
  val context = GetContext();
  std::string textString("@BmwdWMoqPYyzZr1234567890.+-=_~'");
  const int size = 72;
    
  context.set("font", GetFontString(font2, style, size));
  val metrics1 = context.call<val>("measureText", textString);

  context.set("font", GetFontString(fontCombination.Get(), style, size));
  val metrics2 = context.call<val>("measureText", textString);
  
  return metrics1["width"].as<double>() == metrics2["width"].as<double>();
}

bool IGraphicsCanvas::FontExists(const char* font, const char* style)
{
    return !CompareFontMetrics(style, font, "monospace") ||
    !CompareFontMetrics(style, font, "sans-serif") ||
    !CompareFontMetrics(style, font, "serif");
}

bool IGraphicsCanvas::LoadAPIFont(const char* fontID, const PlatformFontPtr& font)
{
  StaticStorage<CanvasFont>::Accessor storage(sFontCache);

  if (storage.Find(fontID))
  {
    if (!font->IsSystem())
      mCustomFonts.push_back(*font->GetDescriptor());
    return true;
  }

  if (!font->IsSystem())
  {
    IFontDataPtr data = font->GetFontData();
    
    if (data->IsValid())
    {
      // Embed the font data in base64 format as CSS in the head of the html
      WDL_TypedBuf<char> base64Encoded;
      
      if (!base64Encoded.ResizeOK(((data->GetSize() * 4) + 3) / 3 + 1))
        return false;
      
      wdl_base64encode(data->Get(), base64Encoded.Get(), data->GetSize());
      std::string htmlText("@font-face { font-family: '");
      htmlText.append(fontID);
      htmlText.append("'; src: url(data:font/ttf;base64,");
      htmlText.append(base64Encoded.Get());
      htmlText.append(") format('truetype'); }");
      val document = val::global("document");
      val documentHead = document["head"];
      val css = document.call<val>("createElement", std::string("style"));
      css.set("type", std::string("text/css"));
      css.set("innerHTML", htmlText);
      document["head"].call<void>("appendChild", css);
      
      FontDescriptor descriptor = font->GetDescriptor();
      const double ascenderRatio = data->GetAscender() / static_cast<double>(data->GetAscender() - data->GetDescender());
      const double EMRatio = data->GetHeightEMRatio();
      storage.Add(new CanvasFont({descriptor->first, descriptor->second}, ascenderRatio, EMRatio), fontID);
      
      // Add to store and encourage to load by using the font immediately
      
      mCustomFonts.push_back(*descriptor);
      CompareFontMetrics(descriptor->second.Get(), descriptor->first.Get(), "monospace");
        
      return true;
    }
  }
  else
  {
    FontDescriptor descriptor = font->GetDescriptor();
    const char* fontName = descriptor->first.Get();
    const char* styleName = descriptor->second.Get();
    
    if (FontExists(fontName, styleName))
    {
      double ascenderRatio, EMRatio;
      
      GetFontMetrics(descriptor->first.Get(), descriptor->second.Get(), ascenderRatio, EMRatio);
      storage.Add(new CanvasFont({descriptor->first, descriptor->second}, ascenderRatio, EMRatio), fontID);
      return true;
    }
  }
  
  return false;
}

bool IGraphicsCanvas::AssetsLoaded()
{
  for (auto it = mCustomFonts.begin(); it != mCustomFonts.end(); it++)
  {
    if (!FontExists(it->first.Get(), it->second.Get()))
      return false;
  }
  
  mCustomFonts.clear();
    
  return true;
}

void IGraphicsCanvas::GetLayerBitmapData(const ILayerPtr& layer, RawBitmapData& data)
{
  const APIBitmap* pBitmap = layer->GetAPIBitmap();
  int size = pBitmap->GetWidth() * pBitmap->GetHeight() * 4;
  val context = pBitmap->GetBitmap()->call<val>("getContext", std::string("2d"));
  val imageData = context.call<val>("getImageData", 0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
  val pixelData = imageData["data"];
  data.Resize(size);
  
  // Copy pixels from context
  if (data.GetSize() >= size)
  {
    unsigned char* out = data.Get();
    
    for (auto i = 0; i < size; i++)
      out[i] = pixelData[i].as<unsigned char>();
  }
}

void IGraphicsCanvas::ApplyShadowMask(ILayerPtr& layer, RawBitmapData& mask, const IShadow& shadow)
{
  const APIBitmap* pBitmap = layer->GetAPIBitmap();
  int size = pBitmap->GetWidth() * pBitmap->GetHeight() * 4;
  
  if (mask.GetSize() >= size)
  {
    int width = pBitmap->GetWidth();
    int height = pBitmap->GetHeight();
    double scale = pBitmap->GetScale() * pBitmap->GetDrawScale();
    double x = shadow.mXOffset * scale;
    double y = shadow.mYOffset * scale;
    val layerCanvas = *pBitmap->GetBitmap();
    val layerContext = layerCanvas.call<val>("getContext", std::string("2d"));
    layerContext.call<void>("setTransform");
    
    if (!shadow.mDrawForeground)
    {
      layerContext.call<void>("clearRect", 0, 0, width, height);
    }
    
    CanvasBitmap localBitmap(width, height, pBitmap->GetScale(), pBitmap->GetDrawScale());
    val localCanvas = *localBitmap.GetBitmap();
    val localContext = localCanvas.call<val>("getContext", std::string("2d"));
    val imageData = localContext.call<val>("createImageData", width, height);
    val pixelData = imageData["data"];
    unsigned char* in = mask.Get();
    
    for (auto i = 0; i < size; i++)
      pixelData.set(i, in[i]);
    
    localContext.call<void>("putImageData", imageData, 0, 0);
    IBlend blend(EBlend::SourceIn, shadow.mOpacity);
    localContext.call<void>("rect", 0, 0, width, height);
    localContext.call<void>("scale", scale, scale);
    localContext.call<void>("translate", -(layer->Bounds().L + shadow.mXOffset), -(layer->Bounds().T + shadow.mYOffset));
    SetCanvasSourcePattern(localContext, shadow.mPattern, &blend);
    localContext.call<void>("fill");
    
    layerContext.set("globalCompositeOperation", "destination-over");
    layerContext.call<void>("drawImage", localCanvas, 0, 0, width, height, x, y, width, height);
  }
}

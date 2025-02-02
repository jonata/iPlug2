#pragma once

#include "IPlugPlatform.h"
#include "IGraphicsPathBase.h"

#if defined IGRAPHICS_METAL
#define SK_METAL
#endif

#include "SkSurface.h"
#include "SkPath.h"
#include "SkCanvas.h"
#include "SkImage.h"

class SkiaBitmap : public APIBitmap
{
public:
  SkiaBitmap(GrContext* context, int width, int height, int scale, float drawScale);
  SkiaBitmap(const char* path, double sourceScale);
  SkiaBitmap(const void* pData, int size, double sourceScale);

private:
 SkiaDrawable mDrawable;
};

/** IGraphics draw class using Skia
*   @ingroup DrawClasses */
class IGraphicsSkia : public IGraphicsPathBase
{
public:
  const char* GetDrawingAPIStr() override ;

  IGraphicsSkia(IGEditorDelegate& dlg, int w, int h, int fps, float scale);
  ~IGraphicsSkia();

  void BeginFrame() override;
  void EndFrame() override;
  void OnViewInitialized(void* pContext) override;
  void OnViewDestroyed() override;
  void DrawResize() override;

  void DrawBitmap(const IBitmap& bitmap, const IRECT& dest, int srcX, int srcY, const IBlend* pBlend) override;

  void PathClear() override { mMainPath.reset(); }
  void PathClose() override { mMainPath.close(); }

  void PathArc(float cx, float cy, float r, float a1, float a2, EWinding winding) override;

  void PathMoveTo(float x, float y) override { mMainPath.moveTo(x, y); }
  void PathLineTo(float x, float y) override { mMainPath.lineTo(x, y); }
  
  void PathCubicBezierTo(float x1, float y1, float x2, float y2, float x3, float y3) override
  {
    mMainPath.cubicTo({x1, y1}, {x2, y2}, {x3, y3});
  }
    
  void PathQuadraticBezierTo(float cx, float cy, float x2, float y2) override
  {
    mMainPath.quadTo({cx, cy}, {x2, y2});
  }
    
  void PathStroke(const IPattern& pattern, float thickness, const IStrokeOptions& options, const IBlend* pBlend) override;
  void PathFill(const IPattern& pattern, const IFillOptions& options, const IBlend* pBlend) override;
  
  IColor GetPoint(int x, int y) override;
  void* GetDrawContext() override { return (void*) mCanvas; }

  bool BitmapExtSupported(const char* ext) override;
  int AlphaChannel() const override { return 3; }
  bool FlippedBitmap() const override { return false; }

  void ReleaseBitmap(const IBitmap& bitmap) override { } // NO-OP
  void RetainBitmap(const IBitmap& bitmap, const char * cacheName) override { } // NO-OP
  APIBitmap* CreateAPIBitmap(int width, int height, int scale, double drawScale) override;

  void GetLayerBitmapData(const ILayerPtr& layer, RawBitmapData& data) override;
  void ApplyShadowMask(ILayerPtr& layer, RawBitmapData& mask, const IShadow& shadow) override;

  void UpdateLayer() override;
    
protected:
    
  void DoMeasureText(const IText& text, const char* str, IRECT& bounds) const override;
  void DoDrawText(const IText& text, const char* str, const IRECT& bounds, const IBlend* pBlend) override;

  bool LoadAPIFont(const char* fontID, const PlatformFontPtr& font) override;

  APIBitmap* LoadAPIBitmap(const char* fileNameOrResID, int scale, EResourceLocation location, const char* ext) override;
private:
    
  void PrepareAndMeasureText(const IText& text, const char* str, IRECT& r, double& x, double & y, SkFont& font) const;

  void PathTransformSetMatrix(const IMatrix& m) override;
  void SetClipRegion(const IRECT& r) override;
  sk_sp<SkSurface> mSurface;
  sk_sp<SkSurface> mScreenSurface;
  SkCanvas* mCanvas = nullptr;
  sk_sp<GrContext> mGrContext;
  std::unique_ptr<GrBackendRenderTarget> mBackendRenderTarget;
  SkPath mMainPath;

#if defined OS_WIN && defined IGRAPHICS_CPU
  WDL_TypedBuf<uint8_t> mSurfaceMemory;
#endif
  
#ifdef IGRAPHICS_METAL
  void* mMTLDevice;
  void* mMTLCommandQueue;
  void* mMTLDrawable;
  void* mMTLLayer;
#endif
};

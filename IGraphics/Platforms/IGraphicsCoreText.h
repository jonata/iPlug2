/*
 ==============================================================================
 
 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.
 
 See LICENSE.txt for  more info.
 
 ==============================================================================
 */

#pragma once

#include <CoreText/CoreText.h>
#include "IGraphicsStructs.h"

class CoreTextFont : public PlatformFont
{
public:
  CoreTextFont(CTFontDescriptorRef descriptor, CGDataProviderRef provider, bool system)
  : PlatformFont(system)
  , mDescriptor(descriptor)
  , mProvider(provider)
  {}
  
  ~CoreTextFont();
  
  FontDescriptor GetDescriptor() override { return mDescriptor; }
  IFontDataPtr GetFontData() override;
  
private:
  CTFontDescriptorRef mDescriptor;
  CGDataProviderRef mProvider;
};

template <class T>
class CFLocal
{
public:
  CFLocal(T obj)
  : mObject(obj)
  {}
  
  ~CFLocal()
  {
    if (mObject)
      CFRelease(mObject);
  }
      
  CFLocal(const CFLocal&) = delete;
  CFLocal& operator=(const CFLocal&) = delete;
    
  T Get() { return mObject;  }
  
  T Release()
  {
    T prev = mObject;
    mObject = nullptr;
    return prev;
  }
  
private:
  T mObject;
};

class CoreTextFontDescriptor
{
public:
  CoreTextFontDescriptor(CTFontDescriptorRef descriptor, double EMRatio)
  : mDescriptor(descriptor)
  , mEMRatio(EMRatio)
  {
    CFRetain(mDescriptor);
  }
  
  ~CoreTextFontDescriptor()
  {
    CFRelease(mDescriptor);
  }
  
  CoreTextFontDescriptor(const CoreTextFontDescriptor&) = delete;
  CoreTextFontDescriptor& operator=(const CoreTextFontDescriptor&) = delete;
    
  CTFontDescriptorRef GetDescriptor() const { return mDescriptor; }
  double GetEMRatio() const { return mEMRatio; }
    
private:
  CTFontDescriptorRef mDescriptor;
  double mEMRatio;
};

namespace CoreTextHelpers
{
  extern PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fileNameOrResID, const char* bundleID);

  extern PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fontName, ETextStyle style);

  extern void CachePlatformFont(const char* fontID, const PlatformFontPtr& font, StaticStorage<CoreTextFontDescriptor>& cache);
  
  CoreTextFontDescriptor* GetCTFontDescriptor(const IText& text, StaticStorage<CoreTextFontDescriptor>& cache);
}

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <cstdlib>
extern "C" unsigned char* ccpFontAtlas(int* width,int* height,float* advances){
 *width=16*48;*height=6*56;auto* data=static_cast<unsigned char*>(std::calloc(*width**height,4));
 auto space=CGColorSpaceCreateDeviceRGB();auto ctx=CGBitmapContextCreate(data,*width,*height,8,*width*4,space,static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast)|static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));CGColorSpaceRelease(space);
 auto font=CTFontCreateWithName(CFSTR("Helvetica"),32,nullptr);CGContextSetAllowsAntialiasing(ctx,true);CGContextSetShouldAntialias(ctx,true);CGContextSetRGBFillColor(ctx,1,1,1,1);
 for(int i=0;i<96;i++){UniChar c=static_cast<UniChar>(32+i);CGGlyph glyph;CTFontGetGlyphsForCharacters(font,&c,&glyph,1);CGSize size;CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,&glyph,&size,1);advances[i]=size.width;CGPoint p{static_cast<CGFloat>((i%16)*48+2),static_cast<CGFloat>((i/16)*56+12)};CTFontDrawGlyphs(font,&glyph,&p,1,ctx);}
 CFRelease(font);CGContextRelease(ctx);return data;
}

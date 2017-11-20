/**
Copyright (c) 2016-2017 Roman Katuntsev <sbkarr@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef RICH_TEXT_COMMON_RTDRAWER_H_
#define RICH_TEXT_COMMON_RTDRAWER_H_

#include "RTCommon.h"
#include "SLDraw.h"
#include "SPThread.h"
#include "SPDrawGLCacheNode.h"

NS_RT_BEGIN

class Drawer : public Ref, public draw::GLCacheNode {
public:
	using ObjectVec = std::vector<Object>;
	using Callback = std::function<void(cocos2d::Texture2D *)>;
	using Font = font::FontSource;

	static Thread &thread();

	~Drawer() { }

	bool init();
	void free();

	// draw normal texture
	bool draw(CommonSource *, Result *, const Rect &, const Callback &, cocos2d::Ref *);

	// draw thumbnail texture, where scale < 1.0f - resample coef
	bool thumbnail(CommonSource *, Result *, const Rect &, float, const Callback &, cocos2d::Ref *);

	void update();
	void clearCache();

public:
	cocos2d::Texture2D *getBitmap(const String &);
	cocos2d::Texture2D *getBitmap(const String &, const Size &);
	void addBitmap(const String &str, cocos2d::Texture2D *bmp);
	void addBitmap(const String &str, cocos2d::Texture2D *bmp, const Size &);
	void performUpdate();

	bool begin(cocos2d::Texture2D *, const Color4B &);
	void end();

	void setColor(const Color4B &);
	void setLineWidth(float);

	void drawRectangle(const Rect &, layout::DrawStyle);
	void drawRectangleFill(const Rect &);
	void drawRectangleOutline(const Rect &);
	void drawRectangleOutline(const Rect &, bool top, bool right, bool bottom, bool left);

	void drawTexture(const Rect &bbox, cocos2d::Texture2D *, const Rect &texRect);

	void drawCharRects(Font *, const font::FormatSpec &, const Rect & bbox, float scale);
	void drawChars(Font *, const font::FormatSpec &, const Rect & bbox);
	void drawRects(const Rect &, const Vector<Rect> &);

	void drawCharsQuads(const Vector<Rc<cocos2d::Texture2D>> &, Vector<Rc<DynamicQuadArray>> &&, const Rect & bbox);

protected:
	void drawCharsEffects(Font *, const font::FormatSpec &, const Rect & bbox);
	void drawCharsQuads(cocos2d::Texture2D *, DynamicQuadArray *, const Rect & bbox);

	void drawResizeBuffer(size_t count);

	Mat4 _projection;

	GLuint _fbo = 0;
	uint32_t _width = 0;
	uint32_t _height = 0;

	GLuint _drawBufferVBO[2] = { 0, 0 };
	size_t _drawBufferSize = 0;

	Color4B _color = Color4B(0, 0, 0, 0);
	float _lineWidth = 1.0f;

	Time _updated;
	bool _cacheUpdated = false;
	Map<String, Pair<Rc<cocos2d::Texture2D>, Time>> _cache;
};

NS_RT_END

#endif /* RICH_TEXT_COMMON_RTDRAWER_H_ */

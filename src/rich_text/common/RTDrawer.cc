// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
Copyright (c) 2016 Roman Katuntsev <sbkarr@stappler.org>

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

#include "SPDefine.h"
#include "SPDynamicLabel.h"

#include "renderer/CCTexture2D.h"
#include "renderer/ccGLStateCache.h"
#include "RTDrawer.h"
#include "RTCommonSource.h"

#include "SPBitmap.h"
#include "SPString.h"
#include "SPTextureCache.h"
#include "SPDrawCanvas.h"
#include "SLImage.h"

constexpr uint16_t ImageFillerWidth = 312;
constexpr uint16_t ImageFillerHeight = 272;

constexpr auto ImageFillerData = R"ImageFillerSvg(<svg xmlns="http://www.w3.org/2000/svg" height="272" width="312" version="1.1">
	<rect x="0" y="0" width="312" height="272" opacity="0.5"/>
	<g transform="translate(0 -780.4)">
		<path d="m104 948.4h104l-32-56-24 32-16-12zm-32-96v128h168v-128h-168zm16 16h136v96h-136v-96zm38 20a10 10 0 0 1 -10 10 10 10 0 0 1 -10 -10 10 10 0 0 1 10 -10 10 10 0 0 1 10 10z" fill-rule="evenodd" fill="#000000"/>
	</g>
</svg>)ImageFillerSvg";

constexpr uint16_t ImageVideoWidth = 128;
constexpr uint16_t ImageVideoHeight = 128;

constexpr auto ImageVideoData = R"ImageVideoData(<svg xmlns="http://www.w3.org/2000/svg" height="128" width="128" version="1.1">
	<circle cx="64" cy="64" r="64"/>
	<path fill="#fff" d="m96.76 64-51.96 30v-60z"/>
</svg>)ImageVideoData";

constexpr float ImageVideoSize = 72.0f;

NS_SP_EXT_BEGIN(rich_text)

class Request : public cocos2d::Ref {
public:
	using Callback = std::function<void(cocos2d::Texture2D *)>;

	// draw normal texture
	bool init(Drawer *, CommonSource *, Result *, const Rect &, const Callback &, cocos2d::Ref *);

	// draw thumbnail texture, where scale < 1.0f - resample coef
	bool init(Drawer *, CommonSource *, Result *, const Rect &, float, const Callback &, cocos2d::Ref *);

protected:
	void onAssetCaptured();

	void prepareBackgroundImage(const Rect &bbox, const Background &bg);

	void draw(cocos2d::Texture2D *);
	void onDrawed(cocos2d::Texture2D *);

	Rect getRect(const Rect &) const;

	void drawRef(const Rect &bbox, const Link &);
	void drawOutline(const Rect &bbox, const Outline &);
	void drawBitmap(const Rect &bbox, cocos2d::Texture2D *bmp, const Background &bg, const Size &box);
	void drawFillerImage(const Rect &bbox, const Background &bg);
	void drawBackgroundImage(const Rect &bbox, const Background &bg);
	void drawBackgroundColor(const Rect &bbox, const Background &bg);
	void drawBackground(const Rect &bbox, const Background &bg);
	void drawLabel(const Rect &bbox, const Label &l);
	void drawObject(const Object &obj);

	Rect _rect;
	float _scale = 1.0f;
	float _density = 1.0f;
	bool _isThumbnail = false;
	bool _highlightRefs = false;

	uint16_t _width = 0;
	uint16_t _height = 0;
	uint16_t _stride = 0;

	Rc<Drawer> _drawer;
	Rc<Result> _result;
	Rc<CommonSource> _source;
	Rc<FontSource> _font;

	Map<String, CommonSource::AssetData> _networkAssets;

	Callback _callback = nullptr;
	Rc<cocos2d::Ref> _ref = nullptr;
};

// draw normal texture
bool Request::init(Drawer *drawer, CommonSource *source, Result *result, const Rect &rect, const Callback &cb, cocos2d::Ref *ref) {
	if (!cb) {
		return false;
	}

	_rect = rect;
	_density = result->getMedia().density;
	_drawer = drawer;
	_source = source;
	_result = result;
	_ref = ref;
	_callback = cb;

	_width = (uint16_t)ceilf(_rect.size.width * _result->getMedia().density);
	_height = (uint16_t)ceilf(_rect.size.height * _result->getMedia().density);
	_source->retainReadLock(this, std::bind(&Request::onAssetCaptured, this));
	return true;
}

// draw thumbnail texture, where scale < 1.0f - resample coef
bool Request::init(Drawer *drawer, CommonSource *source, Result *result, const Rect &rect, float scale, const Callback &cb, cocos2d::Ref *ref) {
	if (!cb) {
		return false;
	}

	_rect = rect;
	_scale = scale;
	_density = result->getMedia().density * scale;
	_isThumbnail = true;
	_drawer = drawer;
	_source = source;
	_result = result;
	_ref = ref;
	_callback = cb;

	_width = (uint16_t)ceilf(_rect.size.width * _result->getMedia().density * _scale);
	_height = (uint16_t)ceilf(_rect.size.height * _result->getMedia().density * _scale);
	_source->retainReadLock(this, std::bind(&Request::onAssetCaptured, this));
	return true;
}

void Request::onAssetCaptured() {
	if (!_source->isActual()) {
		_callback(nullptr);
		_source->getAsset()->releaseReadLock(this);
		return;
	}

	_font = _source->getSource();
	_font->unschedule();

	_networkAssets = _source->getExternalAssets();

	Rc<cocos2d::Texture2D> *ptr = new Rc<cocos2d::Texture2D>(nullptr);

	TextureCache::thread().perform([this, ptr] (const Task &) -> bool {
		TextureCache::getInstance()->performWithGL([&] {
			auto tex = Rc<cocos2d::Texture2D>::create(cocos2d::Texture2D::PixelFormat::RGBA8888, _width, _height, cocos2d::Texture2D::RenderTarget);
			draw( tex );
			*ptr = tex;
		});
		return true;
	}, [this, ptr] (const Task &, bool) {
		onDrawed(*ptr);
		_source->releaseReadLock(this);
		delete ptr;
	}, this);
}

static Size Request_getBitmapSize(const Rect &bbox, const Background &bg, uint32_t w, uint32_t h) {
	Size coverSize, containSize;

	const float coverRatio = std::max(bbox.size.width / w, bbox.size.height / h);
	const float containRatio = std::min(bbox.size.width / w, bbox.size.height / h);

	coverSize = Size(w * coverRatio, h * coverRatio);
	containSize = Size(w * containRatio, h * containRatio);

	float boxWidth = 0.0f, boxHeight = 0.0f;
	switch (bg.backgroundSizeWidth.metric) {
	case layout::style::Metric::Units::Contain: boxWidth = containSize.width; break;
	case layout::style::Metric::Units::Cover: boxWidth = coverSize.width; break;
	case layout::style::Metric::Units::Percent: boxWidth = bbox.size.width * bg.backgroundSizeWidth.value; break;
	case layout::style::Metric::Units::Px: boxWidth = bg.backgroundSizeWidth.value; break;
	default: boxWidth = bbox.size.width; break;
	}

	switch (bg.backgroundSizeHeight.metric) {
	case layout::style::Metric::Units::Contain: boxHeight = containSize.height; break;
	case layout::style::Metric::Units::Cover: boxHeight = coverSize.height; break;
	case layout::style::Metric::Units::Percent: boxHeight = bbox.size.height * bg.backgroundSizeHeight.value; break;
	case layout::style::Metric::Units::Px: boxHeight = bg.backgroundSizeHeight.value; break;
	default: boxHeight = bbox.size.height; break;
	}

	if (bg.backgroundSizeWidth.metric == layout::style::Metric::Units::Auto
			&& bg.backgroundSizeHeight.metric == layout::style::Metric::Units::Auto) {
		boxWidth = w;
		boxHeight = h;
	} else if (bg.backgroundSizeWidth.metric == layout::style::Metric::Units::Auto) {
		boxWidth = boxHeight * ((float)w / (float)h);
	} else if (bg.backgroundSizeHeight.metric == layout::style::Metric::Units::Auto) {
		boxHeight = boxWidth * ((float)h / (float)w);
	}

	return Size(boxWidth, boxHeight);
}

void Request::prepareBackgroundImage(const Rect &bbox, const Background &bg) {
	auto &src = bg.backgroundImage;
	if (_source->isFileExists(src)) {
		auto size = _source->getImageSize(src);
		Size bmpSize = Request_getBitmapSize(bbox, bg, size.first, size.second);
		if (!bmpSize.equals(Size::ZERO)) {
			auto bmp = _drawer->getBitmap(src, bmpSize);
			if (!bmp) {
				Bytes bmpSource = _source->getImageData(src);
				if (!bmpSource.empty()) {
					auto tex = TextureCache::uploadTexture(bmpSource, bmpSize, _density);
					_drawer->addBitmap(src, tex, bmpSize);
				}
			}
		}
	} else {
		Size bmpSize = Request_getBitmapSize(bbox, bg, ImageFillerWidth, ImageFillerHeight);
		auto bmp = _drawer->getBitmap("system://filler.svg", bmpSize);
		if (!bmp && !bmpSize.equals(Size::ZERO)) {
			Bytes bmpSource = Bytes((uint8_t *)ImageFillerData, (uint8_t *)(ImageFillerData + strlen(ImageFillerData)));
			auto tex = TextureCache::uploadTexture(bmpSource, bmpSize, _density);
			_drawer->addBitmap("system://filler.svg", tex, bmpSize);
		}
	}
}

void Request::draw(cocos2d::Texture2D *data) {
	Vector<const Object *> drawObjects;
	auto &objs = _result->getObjects();
	for (auto &obj : objs) {
		if (obj.bbox.intersectsRect(_rect)) {
			drawObjects.push_back(&obj);
			if (obj.type == Object::Type::Label) {
				const Label &l = obj.value.label;
				for (auto &it : l.format.ranges) {
					_font->addTextureChars(it.layout->getName(), l.format.chars, it.start, it.count);
				}
			} else if (obj.type == Object::Type::Background) {
				const Background &bg = obj.value.background;
				if (!bg.backgroundImage.empty() && !_isThumbnail) {
					prepareBackgroundImage(obj.bbox, bg);
				}
			}
		}
	}

	for (auto &obj : _result->getRefs()) {
		if (obj.bbox.intersectsRect(_rect) && obj.type == Object::Type::Ref) {
			const Link &link = obj.value.ref;
			if (link.mode == "video" && !_isThumbnail) {
				drawObjects.push_back(&obj);
				float size = (obj.bbox.size.width > ImageVideoSize && obj.bbox.size.height > ImageVideoSize)
						? ImageVideoSize : (std::min(obj.bbox.size.width, obj.bbox.size.height));

				Size texSize(size, size);
				Bytes bmpSource = Bytes((uint8_t *)ImageVideoData, (uint8_t *)(ImageVideoData + strlen(ImageVideoData)));
				auto tex = TextureCache::uploadTexture(bmpSource, texSize, _density);
				_drawer->addBitmap("system://video.svg", tex, texSize);
			}
		}
	}

	if (_font->isDirty()) {
		_font->update(0.0f);
	}

	if (!_isThumbnail) {
		auto bg = _result->getBackgroundColor();
		if (bg.a == 0) {
			bg.r = 255;
			bg.g = 255;
			bg.b = 255;
		}
		if (!_drawer->begin(data, bg)) {
			return;
		}
	} else {
		if (!_drawer->begin(data, Color4B(0, 0, 0, 0))) {
			return;
		}
	}

	for (auto &obj : drawObjects) {
		drawObject(*obj);
	}

	_drawer->end();
}

Rect Request::getRect(const Rect &rect) const {
	return Rect(
			(rect.origin.x - _rect.origin.x) * _density,
			(rect.origin.y - _rect.origin.y) * _density,
			rect.size.width * _density,
			rect.size.height * _density);
}

void Request::drawRef(const Rect &bbox, const Link &l) {
	if (_isThumbnail) {
		return;
	}
	if (l.mode == "video") {
		float size = (bbox.size.width > ImageVideoSize && bbox.size.height > ImageVideoSize)
				? ImageVideoSize : (std::min(bbox.size.width, bbox.size.height));

		auto bmp = _drawer->getBitmap("system://video.svg", Size(size, size));
		if (bmp) {
			auto targetBbox = getRect(bbox);
			Rect drawBbox;
			if (size == ImageVideoSize) {
				drawBbox = Rect(targetBbox.origin.x, targetBbox.origin.y, ImageVideoSize * _density, ImageVideoSize * _density);
				drawBbox.origin.x += (targetBbox.size.width - drawBbox.size.width) / 2.0f;
				drawBbox.origin.y += (targetBbox.size.height - drawBbox.size.height) / 2.0f;
			}
			_drawer->setColor(Color4B(255, 255, 255, 160));
			_drawer->drawTexture(drawBbox, bmp, Rect(0, 0, bmp->getPixelsWide(), bmp->getPixelsHigh()));
		}
	}

	if (_highlightRefs) {
		_drawer->setColor(Color4B(127, 255, 0, 64));
		_drawer->drawRectangle(getRect(bbox), layout::DrawStyle::Fill);
	}
}

static void DrawerCanvas_prepareOutline(Drawer *canvas, const Outline::Params &outline, float density) {
	canvas->setColor(outline.color);
	canvas->setLineWidth(std::max(outline.width * density, 1.0f));
}

void Request::drawOutline(const Rect &bbox, const Outline &outline) {
	if (_isThumbnail) {
		return;
	}
	if (outline.isMono()) {
		if (outline.top.style != layout::style::BorderStyle::None && outline.top.width > 0.0f) {
			DrawerCanvas_prepareOutline(_drawer, outline.top, _density);
		} else if (outline.right.style != layout::style::BorderStyle::None && outline.right.width > 0.0f) {
			DrawerCanvas_prepareOutline(_drawer, outline.right, _density);
		} else if (outline.bottom.style != layout::style::BorderStyle::None && outline.bottom.width > 0.0f) {
			DrawerCanvas_prepareOutline(_drawer, outline.bottom, _density);
		} else if (outline.left.style != layout::style::BorderStyle::None && outline.left.width > 0.0f) {
			DrawerCanvas_prepareOutline(_drawer, outline.left, _density);
		}
		_drawer->drawRectangleOutline(getRect(bbox),
				outline.hasTopLine(), outline.hasRightLine(), outline.hasBottomLine(), outline.hasLeftLine());
	} else {
		if (outline.hasTopLine()) {
			DrawerCanvas_prepareOutline(_drawer, outline.top, _density);
			_drawer->drawRectangleOutline(getRect(bbox), true, false, false, false);
		}
		if (outline.hasRightLine()) {
			DrawerCanvas_prepareOutline(_drawer, outline.right, _density);
			_drawer->drawRectangleOutline(getRect(bbox), false, true, false, false);
		}
		if (outline.hasBottomLine()) {
			DrawerCanvas_prepareOutline(_drawer, outline.bottom, _density);
			_drawer->drawRectangleOutline(getRect(bbox), false, false, true, false);
		}
		if (outline.hasLeftLine()) {
			DrawerCanvas_prepareOutline(_drawer, outline.left, _density);
			_drawer->drawRectangleOutline(getRect(bbox), false, false, false, true);
		}
	}
}

void Request::drawBitmap(const Rect &origBbox, cocos2d::Texture2D *bmp, const Background &bg, const Size &box) {
	Rect bbox = origBbox;

	const auto w = bmp->getPixelsWide();
	const auto h = bmp->getPixelsHigh();

	float availableWidth = bbox.size.width - box.width, availableHeight = bbox.size.height - box.height;
	float xOffset = 0.0f, yOffset = 0.0f;

	switch (bg.backgroundPositionX.metric) {
	case layout::style::Metric::Units::Percent: xOffset = availableWidth * bg.backgroundPositionX.value; break;
	case layout::style::Metric::Units::Px: xOffset = bg.backgroundPositionX.value; break;
	default: xOffset = availableWidth / 2.0f; break;
	}

	switch (bg.backgroundPositionY.metric) {
	case layout::style::Metric::Units::Percent: yOffset = availableHeight * bg.backgroundPositionY.value; break;
	case layout::style::Metric::Units::Px: yOffset = bg.backgroundPositionY.value; break;
	default: yOffset = availableHeight / 2.0f; break;
	}

	Rect contentBox(0, 0, w, h);

	if (box.width < bbox.size.width) {
		bbox.size.width = box.width;
		bbox.origin.x += xOffset;
	} else if (box.width > bbox.size.width) {
		contentBox.size.width = bbox.size.width * w / box.width;
		contentBox.origin.x -= xOffset * (w / box.width);
	}

	if (box.height < bbox.size.height) {
		bbox.size.height = box.height;
		bbox.origin.y += yOffset;
	} else if (box.height > bbox.size.height) {
		contentBox.size.height = bbox.size.height * h / box.height;
		contentBox.origin.y -= yOffset * (h / box.height);
	}

	bbox = getRect(bbox);
	_drawer->drawTexture(bbox, bmp, contentBox);
}

void Request::drawFillerImage(const Rect &bbox, const Background &bg) {
	auto box = Request_getBitmapSize(bbox, bg, ImageFillerWidth, ImageFillerHeight);
	auto bmp = _drawer->getBitmap("system://filler.svg", box);
	if (bmp) {
		drawBitmap(bbox, bmp, bg, box);
	}
}

void Request::drawBackgroundImage(const Rect &bbox, const Background &bg) {
	auto &src = bg.backgroundImage;
	if (!_source->isFileExists(src)) {
		if (!src.empty() && bbox.size.width > 0.0f && bbox.size.height > 0.0f) {
			_drawer->setColor(Color4B(168, 168, 168, 255));
			drawFillerImage(bbox, bg);
		}
		return;
	}

	auto size = _source->getImageSize(src);
	Size box = Request_getBitmapSize(bbox, bg, size.first, size.second);

	auto bmp = _drawer->getBitmap(src, box);
	if (bmp) {
		_drawer->setColor(Color4B(layout::Color(_result->getBackgroundColor()).text(), 255));
		drawBitmap(bbox, bmp, bg, box);
	}
}

void Request::drawBackgroundColor(const Rect &bbox, const Background &bg) {
	auto &color = bg.backgroundColor;
	_drawer->setColor(color);
	_drawer->drawRectangle(getRect(bbox), layout::DrawStyle::Fill);
}

void Request::drawBackground(const Rect &bbox, const Background &bg) {
	if (_isThumbnail) {
		_drawer->setColor(Color4B(127, 127, 127, 127));
		_drawer->drawRectangle(getRect(bbox), layout::DrawStyle::Fill);
		return;
	}
	if (bg.backgroundColor.a != 0) {
		drawBackgroundColor(bbox, bg);
	}
	if (!bg.backgroundImage.empty()) {
		drawBackgroundImage(bbox, bg);
	}
}

void Request::drawLabel(const cocos2d::Rect &bbox, const Label &l) {
	if (l.format.chars.empty()) {
		return;
	}

	if (_isThumbnail) {
		_drawer->setColor(Color4B(127, 127, 127, 127));
		_drawer->drawCharRects(_font, l.format, getRect(bbox), _scale);
	} else {
		_drawer->drawChars(_font, l.format, getRect(bbox));
	}
}

void Request::drawObject(const Object &obj) {
	switch (obj.type) {
	case Object::Type::Background: drawBackground(obj.bbox, obj.value.background); break;
	case Object::Type::Label: drawLabel(obj.bbox, obj.value.label); break;
	case Object::Type::Outline: drawOutline(obj.bbox, obj.value.outline); break;
	case Object::Type::Ref: drawRef(obj.bbox, obj.value.ref); break;
	default: break;
	}
}

void Request::onDrawed(cocos2d::Texture2D *data) {
	if (data) {
		_callback(data);
	}
}

bool Drawer::init() {
	TextureCache::thread().perform([this] (const Task &) -> bool {
		TextureCache::getInstance()->performWithGL([&] {
			glGenFramebuffers(1, &_fbo);
			glGenBuffers(2, _drawBufferVBO);
		});
		return true;
	}, [this] (const Task &, bool) {
		_cacheUpdated = true;
	}, this);
	return true;
}

void Drawer::free() {
	TextureCache::thread().perform([this] (const Task &) -> bool {
		TextureCache::getInstance()->performWithGL([&] {
			cleanup();
			if (_drawBufferVBO[0] != 0) {
				glDeleteBuffers(1, &_drawBufferVBO[0]);
			}
			if (_drawBufferVBO[1] != 0) {
				glDeleteBuffers(1, &_drawBufferVBO[1]);
			}
			if (_fbo != 0) {
			    glDeleteFramebuffers(1, &_fbo);
			}
		});
		return true;
	}, nullptr, this);
}

// draw normal texture
bool Drawer::draw(CommonSource *s, Result *res, const Rect &r, const Callback &cb, cocos2d::Ref *ref) {
	return Rc<Request>::create(this, s, res, r, cb, ref);
}

// draw thumbnail texture, where scale < 1.0f - resample coef
bool Drawer::thumbnail(CommonSource *s, Result *res, const Rect &r, float scale, const Callback &cb, cocos2d::Ref *ref) {
	return Rc<Request>::create(this, s, res, r, scale, cb, ref);
}

bool Drawer::begin(cocos2d::Texture2D * tex, const Color4B &clearColor) {
	if (_fbo == 0) {
		return false;
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	load();
	_width = tex->getPixelsWide();
	_height = tex->getPixelsHigh();

	tex->setAliasTexParameters();

	int32_t gcd = sp_gcd(_width, _height);
	int32_t dw = (int32_t)_width / gcd;
	int32_t dh = (int32_t)_height / gcd;
	int32_t dwh = gcd * dw * dh;

	float mod = 1.0f;
	while (dwh * mod > 16384) {
		mod /= 2.0f;
	}

	_projection = Mat4::IDENTITY;
	_projection.scale(dh * mod, dw * mod, -1.0);
	_projection.m[12] = -dwh * mod / 2.0f;
	_projection.m[13] = -dwh * mod / 2.0f;
	_projection.m[14] = dwh * mod / 2.0f - 1;
	_projection.m[15] = dwh * mod / 2.0f + 1;
	_projection.m[11] = -1.0f;

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->getName(), 0);
	glViewport(0, 0, (GLsizei)_width, (GLsizei)_height);

	glClearColor(clearColor.r / 255.0f, clearColor.g / 255.0f, clearColor.b / 255.0f, clearColor.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	return true;
}

void Drawer::end() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	cleanup();
}

void Drawer::drawResizeBuffer(size_t count) {
	if (count <= _drawBufferSize) {
		return;
	}

    Vector<GLushort> indices;
    indices.reserve(count * 6);

    for (size_t i= 0; i < count; i++) {
        indices.push_back( i*4 + 0 );
        indices.push_back( i*4 + 1 );
        indices.push_back( i*4 + 2 );

        indices.push_back( i*4 + 3 );
        indices.push_back( i*4 + 2 );
        indices.push_back( i*4 + 1 );
    }

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _drawBufferVBO[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (sizeof(GLushort) * count * 6), (const GLvoid *) indices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Drawer::setLineWidth(float value) {
	if (value != _lineWidth) {
		_lineWidth = value;
	}
}

void Drawer::setColor(const Color4B &color) {
	if (color != _color) {
		_color = color;
	}
}

void Drawer::drawRectangle(const Rect &bbox, layout::DrawStyle style) {
	switch (style) {
	case layout::DrawStyle::Fill:
		drawRectangleFill(bbox);
		break;
	case layout::DrawStyle::Stroke:
		drawRectangleOutline(bbox);
		break;
	case layout::DrawStyle::FillAndStroke:
		drawRectangleFill(bbox);
		drawRectangleOutline(bbox);
		break;
	default:
		break;
	}
}

void Drawer::drawRectangleFill(const Rect &bbox) {
	auto programs = TextureCache::getInstance()->getPrograms();
	auto p = programs->getProgram(GLProgramDesc::Default::RawRect);

	bindTexture(0);
	blendFunc(cocos2d::BlendFunc::ALPHA_NON_PREMULTIPLIED);

	Mat4 transform;
	transform.m[0] = 1.0f;
	transform.m[5] = 1.0f;
	transform.m[10] = 1.0f;
	transform.m[12] = bbox.origin.x;
	transform.m[13] = _height - bbox.origin.y - bbox.size.height;

	transform = _projection * transform;

	useProgram(p->getProgram());
	p->setUniformLocationWith4f(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_AMBIENT_COLOR),
			_color.r / 255.0f, _color.g / 255.0f, _color.b / 255.0f, _color.a / 255.0f);
	p->setUniformLocationWithMatrix4fv(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_MVP_MATRIX),
			transform.m, 1);
	p->setUniformLocationWith2f(p->getUniformLocationForName("u_size"), GLfloat(bbox.size.width / 2.0f + 0.5f), GLfloat(bbox.size.height / 2.0f + 0.5f));
	p->setUniformLocationWith2f(p->getUniformLocationForName("u_position"), GLfloat(bbox.origin.x - 0.5f), GLfloat(_height - bbox.origin.y - bbox.size.height - 0.5f));

	GLfloat vertices[] = { -1.5f, -1.5f, bbox.size.width + 1.5f, -1.5f, -1.5f, bbox.size.height + 1.5f, bbox.size.width + 1.5f, bbox.size.height + 1.5f};

	enableVertexAttribs(cocos2d::GL::VERTEX_ATTRIB_FLAG_POSITION);
	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CHECK_GL_ERROR_DEBUG();
}

void Drawer::drawRectangleOutline(const Rect &bbox) {
	drawRectangleOutline(bbox, true, true, true, true);
}

static void Drawer_makeRect(Vector<GLfloat> &vertices, float x, float y, float width, float height) {
	vertices.push_back(x);			vertices.push_back(y);
	vertices.push_back(x + width);	vertices.push_back(y);
	vertices.push_back(x);			vertices.push_back(y + height);
	vertices.push_back(x + width);	vertices.push_back(y + height);
}

static void Drawer_makeRectFilterVert(Vector<GLfloat> &vertices, float x, float height, float inc, bool top, bool bottom) {
	if (!bottom && !top) {
		Drawer_makeRect(vertices, x - inc, 0, inc * 2.0f, height);
	} else if (bottom && !top) {
		Drawer_makeRect(vertices, x - inc, inc, inc * 2.0f, height - inc);
	} else if (!bottom && top) {
		Drawer_makeRect(vertices, x - inc, 0, inc * 2.0f, height - inc);
	} else {
		Drawer_makeRect(vertices, x - inc, inc, inc * 2.0f, height - inc * 2.0f);
	}
}

static void Drawer_makeRectFilterHorz(Vector<GLfloat> &vertices, float y, float width, float inc, bool left, bool right) {
	if (!left && !right) {
		Drawer_makeRect(vertices, 0, y - inc, width, inc * 2.0f);
	} else if (left && !right) {
		Drawer_makeRect(vertices, inc, y - inc, width - inc, inc * 2.0f);
	} else if (!left && right) {
		Drawer_makeRect(vertices, 0, y - inc, width - inc, inc * 2.0f);
	} else {
		Drawer_makeRect(vertices, inc, y - inc, width - inc * 2.0f, inc * 2.0f);
	}
}

void Drawer::drawRectangleOutline(const Rect &bbox, bool top, bool right, bool bottom, bool left) {
	Vector<GLfloat> vertices; vertices.reserve(6 * 8 * 2);

	auto programs = TextureCache::getInstance()->getPrograms();
	auto p = programs->getProgram(GLProgramDesc::Default::RawRectBorder);

	bindTexture(0);
	blendFunc(cocos2d::BlendFunc::ALPHA_NON_PREMULTIPLIED);

	Mat4 transform;
	transform.m[0] = 1.0f;
	transform.m[5] = 1.0f;
	transform.m[10] = 1.0f;
	transform.m[12] = bbox.origin.x + 1.0f;
	transform.m[13] = _height - bbox.origin.y - bbox.size.height + 1.0f;

	transform = _projection * transform;

	useProgram(p->getProgram());
	p->setUniformLocationWith4f(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_AMBIENT_COLOR),
			_color.r / 255.0f, _color.g / 255.0f, _color.b / 255.0f, _color.a / 255.0f);
	p->setUniformLocationWithMatrix4fv(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_MVP_MATRIX),
			transform.m, 1);
	p->setUniformLocationWith1f(p->getUniformLocationForName("u_border"), GLfloat(_lineWidth / 2.0f));

	const float inc = _lineWidth/2.0f + 2.0f;

	size_t count = 0;

	Rect shaderBox(bbox);
	if (!left) {
		shaderBox.size.width += inc;
		shaderBox.origin.x -= inc;
	}

	if (!right) {
		shaderBox.size.width += inc * 2;
	}

	if (!top) {
		shaderBox.size.height += inc * 2.0f;
		shaderBox.origin.y -= inc * 2.0f;
	}

	if (!bottom) {
		shaderBox.size.height += inc * 2;
	}

	Size shaderSize(shaderBox.size.width / 2.0f + 0.5f, shaderBox.size.height / 2.0f + 0.5f);
	Vec2 shaderPos(shaderBox.origin.x - 0.5f, _height - shaderBox.origin.y - shaderBox.size.height - 0.5f);

	p->setUniformLocationWith2f(p->getUniformLocationForName("u_size"), GLfloat(shaderSize.width), GLfloat(shaderSize.height));
	p->setUniformLocationWith2f(p->getUniformLocationForName("u_position"), GLfloat(shaderPos.x), GLfloat(shaderPos.y));

	if (left) {
		Drawer_makeRectFilterVert(vertices, 0, bbox.size.height, inc, top, bottom);
		++ count;
	}

	if (right) {
		Drawer_makeRectFilterVert(vertices, bbox.size.width, bbox.size.height, inc, top, bottom);
		++ count;
	}

	if (top) {
		Drawer_makeRectFilterHorz(vertices, bbox.size.height, bbox.size.width, inc, left, right);
		++ count;
	}

	if (bottom) {
		Drawer_makeRectFilterHorz(vertices, 0, bbox.size.width, inc, left, right);
		++ count;
	}

    if (left && bottom) {
    	Drawer_makeRect(vertices, -inc, -inc, inc * 2.0f, inc * 2.0f);
		++ count;
    }

    if (left && top) {
    	Drawer_makeRect(vertices, -inc, bbox.size.height - inc, inc * 2.0f, inc * 2.0f);
		++ count;
    }

    if (top && right) {
    	Drawer_makeRect(vertices, bbox.size.width - inc, bbox.size.height - inc, inc * 2.0f, inc * 2.0f);
		++ count;
    }

    if (right && bottom) {
    	Drawer_makeRect(vertices, bbox.size.width - inc, -inc, inc * 2.0f, inc * 2.0f);
		++ count;
    }

	drawResizeBuffer(count);

	glBindBuffer(GL_ARRAY_BUFFER, _drawBufferVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

	enableVertexAttribs(cocos2d::GL::VERTEX_ATTRIB_FLAG_POSITION);
	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _drawBufferVBO[1]);
	glDrawElements(GL_TRIANGLES, (GLsizei) count * 6, GL_UNSIGNED_SHORT, (GLvoid *) (0));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	CHECK_GL_ERROR_DEBUG();
}

void Drawer::drawTexture(const Rect &bbox, cocos2d::Texture2D *tex, const Rect &texRect) {
	auto programs = TextureCache::getInstance()->getPrograms();
	cocos2d::GLProgram * p = nullptr;

	bool colorize = tex->getReferenceFormat() == cocos2d::Texture2D::PixelFormat::A8
			|| tex->getReferenceFormat() == cocos2d::Texture2D::PixelFormat::AI88;

	if (colorize) {
		p = programs->getProgram(GLProgramDesc(GLProgramDesc::Attr::MatrixMVP | GLProgramDesc::Attr::AmbientColor
				| GLProgramDesc::Attr::TexCoords | GLProgramDesc::Attr::MediumP,
				tex->getPixelFormat(), tex->getReferenceFormat()));
	} else {
		p = programs->getProgram(GLProgramDesc(GLProgramDesc::Attr::MatrixMVP
				| GLProgramDesc::Attr::TexCoords | GLProgramDesc::Attr::MediumP,
				tex->getPixelFormat(), tex->getReferenceFormat()));
	}


	auto w = tex->getPixelsWide();
	auto h = tex->getPixelsHigh();

	Mat4 transform;
	transform.m[0] = 1.0f;
	transform.m[5] = 1.0f;
	transform.m[10] = 1.0f;
	transform.m[12] = bbox.origin.x;
	transform.m[13] = _height - bbox.origin.y - bbox.size.height;

	transform = _projection * transform;

	bindTexture(tex->getName());
	useProgram(p->getProgram());
	if (colorize) {
		p->setUniformLocationWith4f(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_AMBIENT_COLOR),
			_color.r / 255.0f, _color.g / 255.0f, _color.b / 255.0f, _color.a / 255.0f);
	}
    p->setUniformLocationWithMatrix4fv(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_MVP_MATRIX), transform.m, 1);

    GLfloat coordinates[] = {
    		(texRect.origin.x / w) , ((texRect.origin.y + texRect.size.height) / h),
			((texRect.origin.x + texRect.size.width) / w), ((texRect.origin.y + texRect.size.height) / h),
			(texRect.origin.x / w), (texRect.origin.y / h),
			((texRect.origin.x + texRect.size.width) / w), (texRect.origin.y / h) };

    GLfloat vertices[] = {
		0.0f, 0.0f,
		bbox.size.width, 0.0f,
		0.0f, bbox.size.height,
		bbox.size.width, bbox.size.height,
    };
	if (!tex->hasPremultipliedAlpha()) {
		blendFunc(cocos2d::BlendFunc::ALPHA_NON_PREMULTIPLIED);
	} else {
		blendFunc(cocos2d::BlendFunc::ALPHA_PREMULTIPLIED);
	}

    enableVertexAttribs( cocos2d::GL::VERTEX_ATTRIB_FLAG_POSITION | cocos2d::GL::VERTEX_ATTRIB_FLAG_TEX_COORD );
    glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE, 0, coordinates);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CHECK_GL_ERROR_DEBUG();
}

void Drawer::drawCharRects(Font *f, const font::FormatSpec &format, const Rect & bbox, float scale) {
	DynamicLabel::makeLabelRects(f, &format, scale, [&] (const Vector<Rect> &rects) {
		drawRects(bbox, rects);
	});
}

void Drawer::drawRects(const Rect &bbox, const Vector<Rect> &rects) {
	Vector<GLfloat> vertices; vertices.reserve(rects.size() * 12);

	auto programs = TextureCache::getInstance()->getPrograms();
	auto p = programs->getProgram(GLProgramDesc::Default::RawRects);

	bindTexture(0);
	blendFunc(cocos2d::BlendFunc::ALPHA_NON_PREMULTIPLIED);

	Mat4 transform;
	transform.m[0] = 1.0f;
	transform.m[5] = 1.0f;
	transform.m[10] = 1.0f;
	transform.m[12] = bbox.origin.x;
	transform.m[13] = _height - bbox.origin.y - bbox.size.height;

	transform = _projection * transform;

	useProgram(p->getProgram());
	p->setUniformLocationWith4f(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_AMBIENT_COLOR),
			_color.r / 255.0f, _color.g / 255.0f, _color.b / 255.0f, _color.a / 255.0f);
	p->setUniformLocationWithMatrix4fv(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_MVP_MATRIX),
			transform.m, 1);

	for (auto &it : rects) {
		Drawer_makeRect(vertices, it.origin.x, it.origin.y, it.size.width, it.size.height);
	}

	drawResizeBuffer(rects.size());

	glBindBuffer(GL_ARRAY_BUFFER, _drawBufferVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

	enableVertexAttribs(cocos2d::GL::VERTEX_ATTRIB_FLAG_POSITION);
	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _drawBufferVBO[1]);
	glDrawElements(GL_TRIANGLES, (GLsizei) rects.size() * 6, GL_UNSIGNED_SHORT, (GLvoid *) (0));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	CHECK_GL_ERROR_DEBUG();
}

void Drawer::drawChars(Font *f, const font::FormatSpec &format, const Rect & bbox) {
	DynamicLabel::makeLabelQuads(f, &format,
			[&] (const DynamicLabel::TextureVec &newTex, DynamicLabel::QuadVec &&newQuads, DynamicLabel::ColorMapVec &&cMap) {
		drawCharsQuads(newTex, std::move(newQuads), bbox);
	});

	drawCharsEffects(f, format, bbox);
}

void Drawer::drawCharsQuads(const Vector<Rc<cocos2d::Texture2D>> &tex, Vector<Rc<DynamicQuadArray>> &&quads, const Rect & bbox) {
	bindTexture(0);

	for (size_t i = 0; i < quads.size(); ++ i) {
		drawCharsQuads(tex[i], quads[i], bbox);
	}
}

void Drawer::drawCharsQuads(cocos2d::Texture2D *tex, DynamicQuadArray *quads, const Rect & bbox) {
	auto programs = TextureCache::getInstance()->getPrograms();
	auto p = programs->getProgram(GLProgramDesc::Default::DynamicBatchA8Highp);
	useProgram(p->getProgram());

	Mat4 transform;
	transform.m[0] = 1.0f;
	transform.m[5] = 1.0f;
	transform.m[10] = 1.0f;
	transform.m[12] = bbox.origin.x;
	transform.m[13] = _height - bbox.origin.y - bbox.size.height;

	transform = _projection * transform;

    p->setUniformLocationWithMatrix4fv(p->getUniformLocationForName(cocos2d::GLProgram::UNIFORM_MVP_MATRIX), transform.m, 1);

	bindTexture(tex->getName());

	auto count = quads->size();
	size_t bufferSize = sizeof(cocos2d::V3F_C4B_T2F_Quad) * count;

	drawResizeBuffer(count);

	glBindBuffer(GL_ARRAY_BUFFER, _drawBufferVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, bufferSize, quads->getData(), GL_DYNAMIC_DRAW);

    blendFunc(cocos2d::BlendFunc::ALPHA_NON_PREMULTIPLIED);
	enableVertexAttribs(cocos2d::GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);

	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE,
			sizeof(cocos2d::V3F_C4B_T2F), (GLvoid*) offsetof(cocos2d::V3F_C4B_T2F, vertices));

	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			sizeof(cocos2d::V3F_C4B_T2F), (GLvoid*) offsetof(cocos2d::V3F_C4B_T2F, colors));

	glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE,
			sizeof(cocos2d::V3F_C4B_T2F), (GLvoid*) offsetof(cocos2d::V3F_C4B_T2F, texCoords));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _drawBufferVBO[1]);

	glDrawElements(GL_TRIANGLES, (GLsizei) count * 6, GL_UNSIGNED_SHORT, (GLvoid *) (0));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	CHECK_GL_ERROR_DEBUG();
}

void Drawer::drawCharsEffects(Font *font, const font::FormatSpec &format, const Rect &bbox) {
	for (auto it = format.begin(); it != format.end(); ++ it) {
		if (it.count() > 0 && it.range->underline > 0) {
			const font::CharSpec &firstChar = format.chars[it.start()];
			const font::CharSpec &lastChar = format.chars[it.start() + it.count() - 1];

			auto color = it.range->color;
			color.a = uint8_t(0.75f * color.a);
			setColor(color);
			Rc<font::FontLayout> layout(it.range->layout);
			Rc<font::FontData> data(layout->getData());
			drawRectangleFill(Rect(bbox.origin.x + firstChar.pos, bbox.origin.y + it.line->pos - data->metrics.height / 8.0f,
					lastChar.pos + lastChar.advance - firstChar.pos, data->metrics.height / 16.0f));
		}
	}
}

void Drawer::update() {
	if (!_cacheUpdated) {
		return;
	}

	if (Time::now() - _updated > TimeInterval::seconds(1)) {
		_cacheUpdated = false;
		TextureCache::thread().perform([this] (const Task &) -> bool {
			TextureCache::getInstance()->performWithGL([&] {
				performUpdate();
			});
			return true;
		}, [this] (const Task &, bool) {
			_cacheUpdated = true;
			_updated = Time::now();
		});
	}
}

void Drawer::clearCache() {
	TextureCache::thread().perform([this] (const Task &) -> bool {
		TextureCache::getInstance()->performWithGL([&] {
			_cache.clear();
		});
		return true;
	});
}

void Drawer::performUpdate() {
	if (_cache.empty()) {
		return;
	}

	auto time = Time::now();
	Vector<String> keys;
	for (auto &it : _cache) {
		if (it.second.second - time > TimeInterval::seconds(30)) {
			keys.push_back(it.first);
		}
	}

	for (auto &it : keys) {
		_cache.erase(it);
	}
}

void Drawer::addBitmap(const String &str, cocos2d::Texture2D *bmp) {
	_cache.emplace(str, pair(bmp, Time::now()));
}

void Drawer::addBitmap(const String &str, cocos2d::Texture2D *bmp, const Size &size) {
	addBitmap(toString(str, "?w=", int(roundf(size.width)), "&h=", int(roundf(size.height))), bmp);
}

cocos2d::Texture2D *Drawer::getBitmap(const std::string &key) {
	auto it = _cache.find(key);
	if (it != _cache.end()) {
		it->second.second = Time::now();
		return it->second.first;
	}
	return nullptr;
}

cocos2d::Texture2D *Drawer::getBitmap(const String &key, const Size &size) {
	return getBitmap(toString(key, "?w=", int(roundf(size.width)), "&h=", int(roundf(size.height))));
}

Thread &Drawer::thread() {
	return TextureCache::thread();
}

NS_SP_EXT_END(rich_text)

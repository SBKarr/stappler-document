// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
Copyright (c) 2017 Roman Katuntsev <sbkarr@stappler.org>

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

#include "Material.h"
#include "MaterialLinearProgress.h"
#include "MaterialScene.h"

#include "RTTooltip.h"
#include "RTView.h"
#include "RTGalleryLayout.h"
#include "RTImageView.h"

#include "SPDevice.h"
#include "SPEventListener.h"
#include "SPRichTextRenderer.h"

NS_MD_BEGIN

SP_DECLARE_EVENT_CLASS(RichTextView, onImageLink);
SP_DECLARE_EVENT_CLASS(RichTextView, onExternalLink);
SP_DECLARE_EVENT_CLASS(RichTextView, onContentLink);
SP_DECLARE_EVENT_CLASS(RichTextView, onError);
SP_DECLARE_EVENT_CLASS(RichTextView, onDocument);
SP_DECLARE_EVENT_CLASS(RichTextView, onLayout);

RichTextView::~RichTextView() { }

bool RichTextView::init(rich_text::Source *source) {
	return init(RichTextListenerView::Horizontal, source);
}

bool RichTextView::init(Layout l, rich_text::Source *source) {
	if (!RichTextListenerView::init(l, source)) {
		return false;
	}

	if (Device::getInstance()->isTablet()) {
		_pageMargin = Margin(4.0f, 12.0f, 12.0f);
	} else {
		_pageMargin = Margin(2.0f, 6.0f, 12.0f);
	}

	_progress = construct<LinearProgress>();
	_progress->setAnchorPoint(cocos2d::Vec2(0.0f, 1.0f));
	_progress->setPosition(0.0f, 0.0f);
	_progress->setLineColor(Color::Blue_500);
	_progress->setLineOpacity(56);
	_progress->setBarColor(Color::Blue_500);
	_progress->setBarOpacity(255);
	_progress->setVisible(false);
	_progress->setAnimated(false);
	addChild(_progress, 2);

	_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};

	_highlight = construct<Highlight>(this);
	addChild(_highlight, 1);

	updateProgress();

	if (_renderer) {
		_renderer->setEnabled(_renderingEnabled);
	}

	return true;
}

void RichTextView::onPosition() {
	_highlight->setPosition(_root->getPosition());
	_highlight->setContentSize(_root->getContentSize());
	RichTextListenerView::onPosition();
}

void RichTextView::onRestorePosition(rich_text::Result *res, float pos) {
	if (_layoutChanged || (!isnan(_savedFontScale) && _savedFontScale != res->getMedia().fontScale)) {
		setViewPosition(_savedPosition);
	} else if (!isnan(_savedRelativePosition) && _savedRelativePosition != 0.0f) {
		_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
		setScrollRelativePosition(_savedRelativePosition);
	} else if (!isnan(pos) && pos != 0.0f) {
		_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
		setScrollRelativePosition(pos);
	}
}

void RichTextView::onRenderer(rich_text::Result *res, bool status) {
	_highlight->setDirty();
	auto pos = getScrollRelativePosition();
	if (!isnan(pos) && pos != 0.0f) {
		_savedRelativePosition = pos;
	}
	if (!_renderSize.equals(Size::ZERO) && _savedPosition.object == maxOf<size_t>()) {
		_savedPosition = getViewPosition();
		_renderSize = Size::ZERO;
	}

	RichTextListenerView::onRenderer(res, status);
	if (status) {
		_rendererUpdate = true;
		updateProgress();
	} else {
		_rendererUpdate = false;
		updateProgress();
	}

	if (res) {
		_renderSize = _contentSize;
		_savedRelativePosition = nan();
		updateScrollBounds();
		onRestorePosition(res, pos);
		_savedFontScale = res->getMedia().fontScale;
		_layoutChanged = false;
		onDocument(this);
	}
}

void RichTextView::onContentSizeDirty() {
	if (!_renderSize.equals(Size::ZERO) && !_renderSize.equals(_contentSize)) {
		_savedPosition = getViewPosition();
		_renderSize = Size::ZERO;
		_layoutChanged = true;
	}

	RichTextListenerView::onContentSizeDirty();
	if (getLayout() == Horizontal) {
		_progress->setPosition(0.0f, _contentSize.height);
	}
	_progress->setContentSize(Size(_contentSize.width, 6.0f));
	_highlight->setDirty();
}

void RichTextView::setLayout(Layout l) {
	if (!_renderSize.equals(Size::ZERO)) {
		_savedPosition = getViewPosition();
		_renderSize = Size::ZERO;
		_layoutChanged = true;
	}
	RichTextListenerView::setLayout(l);
	_highlight->setDirty();
	onLayout(this);
}

void RichTextView::setOverscrollFrontOffset(float value) {
	RichTextListenerView::setOverscrollFrontOffset(value);
	_progress->setPosition(0.0f, _contentSize.height - value);
}

void RichTextView::setSource(rich_text::Source *source) {
	if (_source) {
		if (_sourceErrorListener) { _eventListener->removeHandlerNode(_sourceErrorListener); }
		if (_sourceUpdateListener) { _eventListener->removeHandlerNode(_sourceUpdateListener); }
		if (_sourceAssetListener) { _eventListener->removeHandlerNode(_sourceAssetListener); }
	}
	RichTextListenerView::setSource(source);
	if (_source) {
		_source->setEnabled(_renderingEnabled);
		_sourceErrorListener = _eventListener->onEventWithObject(rich_text::Source::onError, _source,
				[this] (const Event &ev) {
			onSourceError((rich_text::Source::Error)ev.getIntValue());
		});
		_sourceUpdateListener = _eventListener->onEventWithObject(rich_text::Source::onUpdate, _source,
				std::bind(&RichTextView::onSourceUpdate, this));
		_sourceAssetListener = _eventListener->onEventWithObject(rich_text::Source::onDocument, _source,
				std::bind(&RichTextView::onSourceAsset, this));
	}
}

void RichTextView::setProgressColor(const Color &color) {
	_progress->setBarColor(color);
}

void RichTextView::onLightLevelChanged() {
	RichTextListenerView::onLightLevelChanged();
	_rendererUpdate = true;
	updateProgress();
}

void RichTextView::onLink(const String &str, const Vec2 &vec) {
	if (str.front() == '#') {
		onId(str, vec);
		return;
	}

	if (str.compare(0, 7, "http://") == 0 || str.compare(0, 8, "https://") == 0) {
		stappler::Device::getInstance()->goToUrl(str);
		onExternalLink(this, str);
	} else if (str.compare(0, 7, "mailto:") == 0) {
		stappler::Device::getInstance()->mailTo(str);
		onExternalLink(this, str);
	} else if (str.compare(0, 4, "tel:") == 0) {
		stappler::Device::getInstance()->makePhoneCall(str);
		onExternalLink(this, str);
	} else if (str.compare(0, 10, "gallery://") == 0) {
		StringView r(str);
		r.offset(10);
		StringView name = r.readUntil<StringView::Chars<':'>>();
		if (r.is(':')) {
			++ r;
			onGallery(name.str(), r.str(), vec);
		} else {
			onGallery(name.str(), "", vec);
		}
	} else {
		onContentLink(this, str);
		onFile(str, vec);
	}
}

void RichTextView::onId(const String &str, const Vec2 &vec) {
	auto doc = _renderer->getDocument();
	if (!doc) {
		return;
	}
	Vector<String> ids;
	string::split(str, ",", [&ids] (const StringView &r) {
		ids.push_back(r.str());
	});
	for (auto &it : ids) {
		if (it.front() == '#') {
			it = it.substr(1);
		}
	}

	if (ids.empty()) {
		return;
	}

	if (ids.size() == 1) {
		auto node = doc->getNodeByIdGlobal(ids.front());

		if (!node.first || !node.second) {
			return;
		}

		auto attrIt = node.second->getAttributes().find("x-type");
		if (node.second->getHtmlName() == "img" || (attrIt != node.second->getAttributes().end() && attrIt->second == "image")) {
			onImage(ids.front(), vec);
			return;
		}
	}

	auto pos = convertToNodeSpace(vec);

	float width = _contentSize.width - metrics::horizontalIncrement();
	if (width > metrics::horizontalIncrement() * 9) {
		width = metrics::horizontalIncrement() * 9;
	}

	if (!_tooltip) {
		_tooltip = construct<RichTextTooltip>(_source, ids);
		_tooltip->setPosition(pos);
		_tooltip->setAnchorPoint(Vec2(0, 1));
		_tooltip->setMaxContentSize(Size(width, _contentSize.height - metrics::horizontalIncrement() ));
		_tooltip->setOriginPosition(pos, _contentSize, convertToWorldSpace(pos));
		_tooltip->setCloseCallback([this] {
			_tooltip = nullptr;
		});
		_tooltip->pushToForeground();
	}
}

void RichTextView::onImage(const String &id, const Vec2 &) {
	auto node = _source->getDocument()->getNodeByIdGlobal(id);

	if (!node.first || !node.second) {
		return;
	}

	String src = _renderer->getLegacyBackground(*node.second, "image-view"), alt;
	if (src.empty() && node.second->getHtmlName() == "img") {
		auto &attr = node.second->getAttributes();
		auto srcIt = attr.find("src");
		if (srcIt != attr.end()) {
			src = srcIt->second;
		}
	}

	if (src.empty()) {
		auto &nodes = node.second->getNodes();
		for (auto &it : nodes) {
			String legacyImage = _renderer->getLegacyBackground(it, "image-view");
			auto &attr = it.getAttributes();
			if (!legacyImage.empty()) {
				src = legacyImage;
			} else if (it.getHtmlName() == "img") {
				auto srcIt = attr.find("src");
				if (srcIt != attr.end()) {
					src = srcIt->second;
				}
			}

			if (!src.empty()) {
				auto altIt = attr.find("alt");
				if (altIt != attr.end()) {
					alt = altIt->second;
				}
				break;
			}
		}
	} else {
		auto &attr = node.second->getAttributes();
		auto altIt = attr.find("alt");
		if (altIt != attr.end()) {
			alt = altIt->second;
		}
	}

	if (!src.empty()) {
		if (node.second->getNodes().empty()) {
			construct<RichTextImageView>(_source, String(), src, alt);
		} else {
			construct<RichTextImageView>(_source, id, src, alt);
		}
	}
}

void RichTextView::onGallery(const String &name, const String &image, const Vec2 &) {
	if (_source) {
		auto l = Rc<RichTextGalleryLayout>::create(_source, name, image);
		if (l) {
			Scene::getRunningScene()->pushContentNode(l);
		}
	}
}

void RichTextView::onContentFile(const String &str) {
	onFile(str, Vec2(0.0f, 0.0f));
}

void RichTextView::setRenderingEnabled(bool value) {
	if (_renderingEnabled != value) {
		_renderingEnabled = value;
		if (_renderer) {
			_renderer->setEnabled(value);
		}
		if (_source) {
			_source->setEnabled(value);
		}
	}
}

void RichTextView::clearHighlight() {
	_highlight->clearSelection();
}
void RichTextView::addHighlight(const Pair<SelectionPosition, SelectionPosition> &p) {
	_highlight->addSelection(p);
}
void RichTextView::addHighlight(const SelectionPosition &first, const SelectionPosition &second) {
	addHighlight(pair(first, second));
}
float RichTextView::getBookmarkScrollPosition(size_t objIdx, uint32_t pos) const {
	auto res = _renderer->getResult();
	if (res) {
		if (auto obj = res->getObject(objIdx)) {
			if (obj->type == layout::Object::Type::Label) {
				auto line = obj->value.label.format.getLine(pos);
				if (line) {
					return (obj->bbox.origin.y + (line->pos - line->height) / res->getMedia().density) / res->getContentSize().height;
				}
			}
			return obj->bbox.origin.y / res->getContentSize().height;
		}
	}
	return nan();
}

void RichTextView::onFile(const String &str, const Vec2 &) {
	auto res = _renderer->getResult();
	if (res) {
		auto &idx = res->getIndex();
		auto it = idx.find(str);
		if (it != idx.end()) {
			float pos = it->second.y;
			if (res->getMedia().flags & layout::RenderFlag::PaginatedLayout) {
				int num = roundf(pos / res->getSurfaceSize().height);
				if (_renderer->isPageSplitted()) {
					if (_positionCallback) {
						_positionCallback((num / 2) * _renderSize.width);
					}
					setScrollPosition((num / 2) * _renderSize.width);
				} else {
					if (_positionCallback) {
						_positionCallback(num * _renderSize.width);
					}
					setScrollPosition(num * _renderSize.width);
				}
			} else {
				pos -= getOverscrollFrontOffset();
				if (pos > getScrollMaxPosition()) {
					pos = getScrollMaxPosition();
				} else if (pos < getScrollMinPosition()) {
					pos = getScrollMinPosition();
				}
				if (_positionCallback) {
					_positionCallback(pos);
				}
				setScrollPosition(pos);
			}
		}
	}
}

void RichTextView::onObjectPressEnd(const Vec2 &vec, const rich_text::Object &obj) {
	if (obj.type == rich_text::Object::Type::Ref) {
		auto &ref = obj.value.ref.target;
		onLink(ref, vec);
	}
}

void RichTextView::onSourceError(rich_text::Source::Error err) {
	onError(this, err);
	updateProgress();
}

void RichTextView::onSourceUpdate() {
	updateProgress();
}

void RichTextView::onSourceAsset() {
	updateProgress();
}

void RichTextView::updateProgress() {
	if (_progress) {
		if (!_renderingEnabled || !_source) {
			_progress->setVisible(false);
		} else {
			auto a = _source->getAsset();
			if (a && a->isDownloadInProgress()) {
				_progress->setVisible(true);
				_progress->setAnimated(false);
				_progress->setProgress(a->getProgress());
			} else if (_source->isDocumentLoading() || _rendererUpdate) {
				_progress->setVisible(true);
				_progress->setAnimated(true);
			} else {
				_progress->setVisible(false);
				_progress->setAnimated(false);
			}
		}
	}
}

RichTextView::ViewPosition RichTextView::getViewObjectPosition(float pos) const {
	ViewPosition ret{maxOf<size_t>(), 0.0f, pos};
	auto res = _renderer->getResult();
	if (res) {
		size_t objectId = 0;
		auto &objs = res->getObjects();
		for (auto &it : objs) {
			if (it.bbox.origin.y > pos) {
				ret.object = objectId;
				break;
			}

			if (it.bbox.origin.y <= pos && it.bbox.origin.y + it.bbox.size.height >= pos) {
				ret.object = objectId;
				ret.position = (it.bbox.origin.y + it.bbox.size.height - pos) / it.bbox.size.height;
				break;
			}

			objectId ++;
		}
	}
	return ret;
}

RichTextView::Page *RichTextView::onConstructPageNode(const PageData &data, float density) {
	return construct<PageWithLabel>(data, density);
}

float RichTextView::getViewContentPosition(float pos) const {
	if (isnan(pos)) {
		pos = getScrollPosition();
	}
	auto res = _renderer->getResult();
	if (res) {
		auto flags = res->getMedia().flags;
		if (flags & layout::RenderFlag::PaginatedLayout) {
			float segment = (flags & layout::RenderFlag::SplitPages)?_renderSize.width/2.0f:_renderSize.width;
			int num = roundf(pos / segment);
			if (flags & layout::RenderFlag::SplitPages) {
				++ num;
			}

			auto data = getPageData(num);
			return data.texRect.origin.y;
		} else {
			return pos + getScrollSize() / 2.0f;
		}
	}
	return 0.0f;
}

RichTextView::ViewPosition RichTextView::getViewPosition() const {
	if (_renderSize.equals(Size::ZERO)) {
		return ViewPosition{maxOf<size_t>(), 0.0f, 0.0f};
	}

	return getViewObjectPosition(getViewContentPosition());
}

void RichTextView::setViewPosition(const ViewPosition &pos, bool offseted) {
	if (_renderingEnabled) {
		auto res = _renderer->getResult();
		if (res && pos.object != maxOf<size_t>()) {
			auto &objs = res->getObjects();
			if (objs.size() > pos.object) {
				auto &obj = objs.at(pos.object);
				if (res->getMedia().flags & layout::RenderFlag::PaginatedLayout) {
					float scrollPos = obj.bbox.origin.y + obj.bbox.size.height * pos.position;
					int num = roundf(scrollPos / res->getSurfaceSize().height);
					if (res->getMedia().flags & layout::RenderFlag::SplitPages) {
						setScrollPosition((num / 2) * _renderSize.width);
					} else {
						setScrollPosition(num * _renderSize.width);
					}
				} else {
					float scrollPos = (obj.bbox.origin.y + obj.bbox.size.height * pos.position) - (!offseted?(getScrollSize() / 2.0f):0.0f);
					if (scrollPos < getScrollMinPosition()) {
						scrollPos = getScrollMinPosition();
					} else if (scrollPos > getScrollMaxPosition()) {
						scrollPos = getScrollMaxPosition();
					}
					setScrollPosition(scrollPos);
				}
				if (_scrollCallback) {
					_scrollCallback(0.0f, true);
				}
			}
		}
		_savedPosition = ViewPosition{maxOf<size_t>(), 0.0f};
	} else {
		_savedPosition = pos;
		_layoutChanged = true;
	}
}

void RichTextView::setPositionCallback(const PositionCallback &cb) {
	_positionCallback = cb;
}
const RichTextView::PositionCallback &RichTextView::getPositionCallback() const {
	return _positionCallback;
}

NS_MD_END

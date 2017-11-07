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
#include "MaterialScene.h"
#include "MaterialContentLayer.h"
#include "MaterialToolbar.h"
#include "MaterialMenuSource.h"
#include "MaterialFlexibleLayout.h"
#include "MaterialImageLayer.h"
#include "RTImageView.h"

#include "SLResult.h"
#include "SLDocument.h"
#include "SPRichTextRenderer.h"
#include "SPRichTextDrawer.h"
#include "SPTextureCache.h"

NS_MD_BEGIN

RichTextImageView::~RichTextImageView() { }

bool RichTextImageView::init(layout::Source *source, const String &id, const String &src, const String &alt) {
	if (!Layout::init() || !source) {
		return false;
	}

	_source = source;
	if (!_source->isActual()) {
		return false;
	}

	auto doc = _source->getDocument();
	if (!doc->isFileExists(src)) {
		return false;
	}

	auto contentLayer = Scene::getRunningScene()->getContentLayer();

	auto incr = metrics::horizontalIncrement();
	auto size = contentLayer->getContentSize();
	auto pos = Vec2(incr / 4.0f, incr / 2.0f);

	_tooltip = construct<RichTextTooltip>(source, id.empty()?Vector<String>():Vector<String>{id});

	Size maxSize;
	maxSize.width = size.width - metrics::horizontalIncrement();
	maxSize.height = size.height - metrics::horizontalIncrement() * 2.0f;

	if (maxSize.width > metrics::horizontalIncrement() * 9) {
		maxSize.width = metrics::horizontalIncrement() * 9;
	}

	if (maxSize.height > metrics::horizontalIncrement() * 9) {
		maxSize.height = metrics::horizontalIncrement() * 9;
	}

	size.width -= metrics::horizontalIncrement();
	_tooltip->setOriginPosition(Vec2(0, incr / 2.0f), size, contentLayer->convertToWorldSpace(pos));
	_tooltip->setMaxContentSize(maxSize);
	_tooltip->setPosition(pos);
	if (auto r = _tooltip->getRenderer()) {
		r->addOption("image-view");
	}
	addChild(_tooltip, 2);

	auto actions = _tooltip->getActions();
	actions->clear();
	if (!id.empty()) {
		_expandButton = actions->addButton("Expand", IconName::Navigation_expand_less, std::bind(&RichTextImageView::onExpand, this));
	} else {
		_expanded = false;
	}

	auto toolbar = _tooltip->getToolbar();
	toolbar->setTitle(alt);
	toolbar->setNavButtonIcon(IconName::Dynamic_Navigation);
	toolbar->setNavButtonIconProgress(1.0f, 0.0f);
	toolbar->setNavCallback(std::bind(&RichTextImageView::close, this));
	toolbar->setSwallowTouches(true);

	_sprite = construct<ImageLayer>();
	_sprite->setPosition(0, 0);
	_sprite->setAnchorPoint(Vec2(0, 0));
	_sprite->setActionCallback([this] {
		if (!_expanded) {
			_tooltip->onFadeOut();
		}
	});
	addChild(_sprite, 1);

	if (_source->tryReadLock(this)) {
		onAssetCaptured(src);
	}

	contentLayer->pushNode(this);
	return true;
}

Rc<cocos2d::Texture2D> RichTextImageView::readImage(const std::string &src) {
	auto doc = _source->getDocument();
	return TextureCache::uploadTexture(doc->getImageData(src));
}

void RichTextImageView::onImage(cocos2d::Texture2D *img) {
	_sprite->setTexture(img);
}

void RichTextImageView::onAssetCaptured(const String &src) {
	Rc<cocos2d::Texture2D> *img = new Rc<cocos2d::Texture2D>(nullptr);
	rich_text::Drawer::thread().perform([this, img, src] (const Task &) -> bool {
		*img = readImage(src);
		return true;
	}, [this, img] (const Task &, bool) {
		onImage(*img);
		_source->releaseReadLock(this);
		delete img;
	}, this);
}


void RichTextImageView::onContentSizeDirty() {
	cocos2d::Node::onContentSizeDirty();

	if (!_contentSize.equals(Size::ZERO)) {
		Size maxSize;
		maxSize.width = _contentSize.width - metrics::horizontalIncrement();
		maxSize.height = _contentSize.height - metrics::horizontalIncrement() * 2.0f;

		if (maxSize.width > metrics::horizontalIncrement() * 9) {
			maxSize.width = metrics::horizontalIncrement() * 9;
		}

		if (maxSize.height > metrics::horizontalIncrement() * 9) {
			maxSize.height = metrics::horizontalIncrement() * 9;
		}

		if (_tooltip->getRenderer()) {
			_tooltip->setMaxContentSize(maxSize);
		} else {
			//_tooltip->setContentSize(Size(maxSize.width, _tooltip->getToolbar()->getContentSize().height));
			_tooltip->setContentSize(Size(maxSize.width, metrics::miniBarHeight()));
		}
	}

	_sprite->setContentSize(_contentSize);
}

void RichTextImageView::close() {
	auto contentLayer = Scene::getRunningScene()->getContentLayer();
	contentLayer->popNode(this);
}

void RichTextImageView::onExpand() {
	if (_expanded) {
		_tooltip->setExpanded(false);
		if (_expandButton) {
			_expandButton->setNameIcon(IconName::Navigation_expand_more);
		}
		_expanded = false;
	} else {
		_tooltip->setExpanded(true);
		if (_expandButton) {
			_expandButton->setNameIcon(IconName::Navigation_expand_less);
		}
		_expanded = true;
	}
}

NS_MD_END

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
#include "MaterialResize.h"
#include "MaterialScene.h"
#include "MaterialForegroundLayer.h"
#include "MaterialToolbarLayout.h"
#include "MaterialToolbar.h"
#include "MaterialMenuSource.h"

#include "RTListenerView.h"
#include "RTTooltip.h"

#include "SPRichTextView.h"
#include "SPRichTextRenderer.h"
#include "SLResult.h"
#include "SPScrollView.h"
#include "SPGestureListener.h"
#include "SPEventListener.h"
#include "SPActions.h"

NS_MD_BEGIN

SP_DECLARE_EVENT_CLASS(RichTextTooltip, onCopy);

RichTextTooltip::~RichTextTooltip() { }

bool RichTextTooltip::init(rich_text::Source *s, const Vector<String> &ids) {
	if (!MaterialNode::init()) {
		return false;
	}

	auto l = construct<gesture::Listener>();
	l->setTouchCallback([this] (gesture::Event ev, const gesture::Touch &) {
		if (ev == gesture::Event::Ended || ev == gesture::Event::Cancelled) {
			if (!_expanded) {
				onDelayedFadeOut();
			}
		} else {
			onFadeIn();
		}
		return true;
	});
	//l->setOpacityFilter(1);
	l->setSwallowTouches(true);
	addComponent(l);
	_listener = l;

	if (!ids.empty()) {
		_view = construct<RichTextListenerView>(RichTextListenerView::Vertical, s, ids);
		_view->setAnchorPoint(Vec2(0, 0));
		_view->setPosition(Vec2(0, 0));
		_view->setResultCallback(std::bind(&RichTextTooltip::onResult, this, std::placeholders::_1));
		_view->setUseSelection(true);
		_view->getGestureListener()->setSwallowTouches(true);

		auto el = Rc<EventListener>::create();
		el->onEventWithObject(RichTextListenerView::onSelection, _view, std::bind(&RichTextTooltip::onSelection, this));
		_view->addComponent(el);

		auto r = _view->getRenderer();
		r->addOption("tooltip");
		r->addFlag(layout::RenderFlag::NoImages);
		r->addFlag(layout::RenderFlag::NoHeightCheck);
		r->addFlag(layout::RenderFlag::RenderById);
	}

	_toolbar = construct<material::Toolbar>();
	_toolbar->setShadowZIndex(1.0f);
	_toolbar->setMaxActionIcons(2);

	_layout = construct<material::ToolbarLayout>(_toolbar);
	_layout->setFlexibleToolbar(false);
	_layout->setStatusBarTracked(false);

	_actions = Rc<material::MenuSource>::create();;
	_actions->addButton("Close", IconName::Navigation_close, std::bind(&RichTextTooltip::close, this));

	_selectionActions = Rc<material::MenuSource>::create();
	_selectionActions->addButton("Copy", IconName::Content_content_copy, std::bind(&RichTextTooltip::copySelection, this));
	_selectionActions->addButton("Close", IconName::Content_remove_circle_outline, std::bind(&RichTextTooltip::cancelSelection, this));

	_toolbar->setActionMenuSource(_actions);
	_toolbar->setNavButtonIcon(material::IconName::None);
	_toolbar->setTitle("");
	_toolbar->setMinified(true);
	_toolbar->setBarCallback([this] {
		onFadeIn();
	});

	if (_view) {
		_layout->setBaseNode(_view);
		_layout->setOpacity(0);
	} else {
		_layout->setOpacity(255);
		_toolbar->setShadowZIndex(0.0f);
		_toolbar->setSwallowTouches(false);
	}
	_layout->setAnchorPoint(Vec2(0, 0));
	_layout->setPosition(0, 0);
	addChild(_layout);

	_contentSizeDirty = true;

	return true;
}

void RichTextTooltip::onContentSizeDirty() {
	MaterialNode::onContentSizeDirty();
	if (_originPosition.y - metrics::horizontalIncrement() / 4 < _contentSize.height * _anchorPoint.y) {
		setAnchorPoint(Vec2(_anchorPoint.x,
				(_originPosition.y - metrics::horizontalIncrement() / 4) / _contentSize.height));
	}

	_layout->onContentSizeDirty();
	if (_maxContentSize.width != 0.0f) {
		_layout->setContentSize(Size(_maxContentSize.width,
				MAX(_contentSize.height, _layout->getCurrentFlexibleMax())));
	} else {
		_layout->setContentSize(Size(_contentSize.width,
				MAX(_contentSize.height, _layout->getCurrentFlexibleMax())));
	}
}

void RichTextTooltip::onEnter()  {
	if (!_view) {
		setShadowZIndex(3.0f);
	}
	MaterialNode::onEnter();
}
void RichTextTooltip::onResult(rich_text::Result *r) {
	auto bgColor = r->getBackgroundColor();
	setBackgroundColor(bgColor);
	_toolbar->setColor(bgColor);

	Size cs = r->getContentSize();

	cs.height += _layout->getFlexibleMaxHeight() + 16.0f;
	if (cs.height > _maxContentSize.height) {
		cs.height = _maxContentSize.height;
	}

	_defaultSize = cs;
	if (!_expanded) {
		cs.height = material::metrics::appBarHeight();
	}

	stopAllActions();
	setShadowZIndexAnimated(3.0f, 0.25f);
	runAction(action::sequence(construct<ResizeTo>(0.25, cs), [this] {
		if (_layout->getOpacity() != 255) {
			_layout->setOpacity(255);
		}
	}));
}

void RichTextTooltip::setMaxContentSize(const Size &size) {
	_maxContentSize = size;
	if (_layout->getContentSize().width != _maxContentSize.width) {
		_layout->setContentSize(Size(_maxContentSize.width,
				MAX(_layout->getContentSize().height, _layout->getCurrentFlexibleMax())));
	}
}

void RichTextTooltip::setOriginPosition(const Vec2 &pos, const Size &parentSize, const Vec2 &worldPos) {
	_originPosition = pos;
	_worldPos = worldPos;
	_parentSize = parentSize;

	setAnchorPoint(Vec2(_originPosition.x / _parentSize.width, 1.0f));
}

void RichTextTooltip::setExpanded(bool value) {
	if (value != _expanded) {
		_expanded = value;

		if (_expanded) {
			stopAllActions();
			runAction(action::sequence(construct<ResizeTo>(0.25, _defaultSize), [this] {
				_view->setVisible(true);
				_view->setOpacity(0);
				_view->runAction(cocos2d::FadeIn::create(0.15f));
			}));
			_toolbar->setShadowZIndex(1.5f);
			onFadeIn();
		} else {
			_view->setVisible(false);

			auto newSize = Size(_defaultSize.width, _toolbar->getDefaultToolbarHeight());
			if (!newSize.equals(_defaultSize)) {
				stopAllActions();
				runAction(action::sequence(construct<ResizeTo>(0.25, newSize), [this] {
					_toolbar->setShadowZIndex(0.0f);
					onDelayedFadeOut();
				}));
			} else {
				onDelayedFadeOut();
			}
		}
	}
}

void RichTextTooltip::pushToForeground() {
	auto foreground = Scene::getRunningScene()->getForegroundLayer();
	setPosition(foreground->convertToNodeSpace(_worldPos));
	foreground->pushNode(this, std::bind(&RichTextTooltip::close, this));
}

void RichTextTooltip::close() {
	if (_closeCallback) {
		_closeCallback();
	}

	_layout->setVisible(false);
	stopAllActions();
	setShadowZIndexAnimated(0.0f, 0.25f);
	runAction(action::sequence(construct<ResizeTo>(0.25, Size(0, 0)), [this] {
		auto foreground = Scene::getRunningScene()->getForegroundLayer();
		foreground->popNode(this);
	}));
}

void RichTextTooltip::onDelayedFadeOut() {
	stopActionByTag(2);
	runAction(action::sequence(3.0f, cocos2d::FadeTo::create(0.25f, 48)), 2);
}

void RichTextTooltip::onFadeIn() {
	stopActionByTag(1);
	auto a = getActionByTag(3);
	if (!a || getOpacity() != 255) {
		runAction(cocos2d::FadeTo::create(0.15f, 255), 3);
	}
}

void RichTextTooltip::onFadeOut() {
	stopActionByTag(3);
	auto a = getActionByTag(2);
	if (!a || getOpacity() != 48) {
		runAction(cocos2d::FadeTo::create(0.15f, 48), 2);
	}
}

void RichTextTooltip::setCloseCallback(const CloseCallback &cb) {
	_closeCallback = cb;
}

rich_text::Renderer *RichTextTooltip::getRenderer() const {
	return _view?_view->getRenderer():nullptr;
}

String RichTextTooltip::getSelectedString() const {
	return (_view && _view->isSelectionEnabled())?_view->getSelectedString():String();
}

void RichTextTooltip::onLightLevel() {
	MaterialNode::onLightLevel();
}

void RichTextTooltip::copySelection() {
	if (_view && _view->isSelectionEnabled()) {
		onCopy(this);
		_view->disableSelection();
	}
}
void RichTextTooltip::cancelSelection() {
	if (_view) {
		_view->disableSelection();
	}
}
void RichTextTooltip::onSelection() {
	if (_view) {
		if (_view->isSelectionEnabled()) {
			_toolbar->replaceActionMenuSource(_selectionActions, 2);
		} else {
			_toolbar->replaceActionMenuSource(_actions, 2);
		}
	}
}

NS_MD_END

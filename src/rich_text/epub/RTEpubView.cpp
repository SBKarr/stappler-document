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

#include "RTView.h"
#include "RTEpubView.h"
#include "RTEpubContentsView.h"
#include "RTEpubNavigation.h"

#include "MaterialMenuSource.h"
#include "MaterialFontSizeMenu.h"
#include "MaterialLightLevelMenu.h"
#include "MaterialResourceManager.h"
#include "MaterialButton.h"
#include "MaterialToolbar.h"
#include "MaterialSidebarLayout.h"
#include "MaterialFloatingActionButton.h"
#include "MaterialButtonLabelSelector.h"
#include "MaterialButtonIcon.h"

#include "SPFilesystem.h"
#include "SPActions.h"
#include "SPEventListener.h"
#include "SPDevice.h"
#include "SPScreen.h"
#include "SPRichTextRenderer.h"

NS_MD_BEGIN

bool EpubView::init(RichTextSource *source, const String &title, font::HyphenMap *hmap) {
	if (!ToolbarLayout::init()) {
		return false;
	}

	setToolbarColor(Color::Grey_300);
	setMaxActions(4);
	setFlexibleToolbar(false);

	_savedTitle = title;
	_toolbar->setTitle(title);
	_toolbar->setLocalZOrder(4);
	_hyphens = hmap;

	_source = source;
	_source->setEnabled(true);

	_buttonLeft = construct<Button>(std::bind(&EpubView::onButtonLeft, this), [this] {
		_buttonLeft->cancel();
	});
	_buttonLeft->setStyle(Button::Style::FlatBlack);
	_buttonLeft->setAnchorPoint(Vec2(0, 0));
	_buttonLeft->setPosition(Vec2(0, 0));
	_buttonLeft->setSwallowTouches(false);
	_buttonLeft->setVisible(false);
	addChild(_buttonLeft, 2);

	_buttonRight = construct<Button>(std::bind(&EpubView::onButtonRight, this), [this] {
		_buttonRight->cancel();
	});
	_buttonRight->setStyle(Button::Style::FlatBlack);
	_buttonRight->setAnchorPoint(Vec2(1, 0));
	_buttonRight->setPosition(Vec2(0, 0));
	_buttonRight->setSwallowTouches(false);
	_buttonRight->setVisible(false);
	addChild(_buttonRight, 2);

	_backButton = construct<FloatingActionButton>(std::bind(&EpubView::onReaderBackButton, this));
	_backButton->setIcon(IconName::Content_undo);
	_backButton->setAnchorPoint(Vec2(0, 0));
	_backButton->setSwallowTouches(true);
	_backButton->setVisible(false);
	_backButton->setBackgroundColor(Color::White);
	addChild(_backButton, 3);

	_viewSource = Rc<MenuSource>::create();
	_viewSource->addItem(Rc<FontSizeMenuButton>::create());
	_viewSource->addButton("", IconName::Action_list, std::bind([this] {
		if (_sidebar->getProgress() == 0.0f) {
			setFlexibleLevelAnimated(1.0f, 0.25f);
			_sidebar->show();
		} else {
			_sidebar->hide();
		}
	}));
	_layoutButton = _viewSource->addButton("horizontal", IconName::Stappler_layout_horizontal, std::bind(&EpubView::onLayoutButton, this));
	_toolbar->setActionMenuSource(_viewSource);

	_selectionSource = Rc<MenuSource>::create();
	_selectionSource->addButton("Copy", IconName::Content_content_copy, std::bind(&EpubView::onCopyButton, this));
	_selectionSource->addButton("Bookmark", IconName::Action_bookmark_border, std::bind(&EpubView::onBookmarkButton, this));
	_selectionSource->addButton("AppCopy"_locale, IconName::Content_content_copy, std::bind(&EpubView::onCopyButton, this));
	_selectionSource->addButton("AppMakeBookmark"_locale, IconName::Action_bookmark_border, std::bind(&EpubView::onBookmarkButton, this));
	//_selectionSource->addButton("AppSendEmail"_locale, IconName::Content_mail, std::bind(&EpubView::onEmailButton, this));
	//_selectionSource->addButton("AppShare"_locale, IconName::Social_share, std::bind(&EpubView::onShareButton, this));
	_selectionSource->addButton("AppReportMisprint"_locale, IconName::Content_report, std::bind(&EpubView::onMisprintButton, this));

	_view = construct<RichTextView>(_source);
	_view->setHyphens(_hyphens);
	_view->setIndicatorVisible(false);
	_view->setTapCallback([this] (int count, const Vec2 &loc) {
		_navigation->close();
		if (_sidebar->isNodeEnabled()) {
			_sidebar->hide();
		}
		setFlexibleLevelAnimated(1.0f, 0.25f);
		if (_view->getLayout() == ScrollView::Horizontal) {
			auto pos = _view->convertToNodeSpace(loc);
		}
	});
	_view->setAnimationCallback([this] {
		_buttonLeft->setSwallowTouches(false);
		_buttonRight->setSwallowTouches(false);
	});
	_view->setPositionCallback([this] (float val) {
		_navigation->setReaderProgress(_view->getScrollRelativePosition(val));
		showBackButton(_view->getScrollRelativePosition());
	});

	setBaseNode(_view, 1);

	_navigation = construct<EpubNavigation>(_view, [this] (float val) {
		auto res =_view->getResult();
		if (res->getMedia().flags & layout::RenderFlag::PaginatedLayout) {
			auto pages = res->getNumPages();
			val = floorf(val * (pages + 1)) / (pages + 1);
			if (val > 1.0f) {
				val = 1.0f;
			} else if (val < 0.0f) {
				val = 0.0f;
			}
			onProgressPosition(val);
		} else {
			onProgressPosition(val);
		}
	});
	addChild(_navigation, 2);

	_sidebar = construct<material::SidebarLayout>(material::SidebarLayout::Right);
	_sidebar->setNodeWidthCallback([] (const Size &size) -> float {
		return MIN(size.width - 56.0f, material::metrics::horizontalIncrement() * 6);
	});
	_sidebar->setNodeVisibleCallback([this] (bool value) {
		if (value) {
			setFlexibleLevelAnimated(1.0f, 0.25f);
			_contents->setSelectedPosition(_view->getViewContentPosition());
		}
	});
	_sidebar->setBackgroundActiveOpacity(0);

	_contents = construct<EpubContentsView>(std::bind(&EpubView::onContentsRef, this, std::placeholders::_1),
			std::bind(&EpubView::onBookmarkData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	_contents->setAnchorPoint(Vec2(1.0f, 0.0f));
	_contents->setVisible(false);
	_contents->setColors(Color::Blue_500, Color::Yellow_A400);

	_sidebar->setNode(_contents);
	_sidebar->setEnabled(false);
	addChild(_sidebar, 3);

	auto l = Rc<EventListener>::create();
	l->onEventWithObject(RichTextSource::onDocument, source, [this] (const Event &) {
		onDocument(_source->getDocument());
	});
	l->onEventWithObject(RichTextView::onDocument, _view, [this] (const Event &) {
		onDocument(_view->getDocument());
	});
	l->onEventWithObject(RichTextView::onSelection, _view, [this] (const Event &) {
		onSelection(_view->isSelectionEnabled());
	});
	l->onEventWithObject(RichTextView::onContentLink, _view, [this] (const Event &) {
		//showBackButton(_view->getScrollRelativePosition());
	});
	addComponent(l);

	return true;
}

void EpubView::onContentSizeDirty() {
	if (_view->getLayout() == RichTextView::Horizontal && !Device::getInstance()->isTablet() && _contentSize.height < 440.0f && _contentSize.width > _contentSize.height) {
		_toolbar->setMinified(true);
	} else {
		_toolbar->setMinified(false);
	}

	ToolbarLayout::onContentSizeDirty();
	if (_extendedNavigation) {
		if (_view->getLayout() == RichTextView::Horizontal) {
			_baseNode->setContentSize(_baseNode->getContentSize() - Size(0.0f, 20.0f));
			_baseNode->setPosition(0.0f, 20.0f);

			_navigation->setContentSize(Size(_contentSize.width, 20.0f));
			_navigation->setPosition(0.0f, 0.0f);
			_navigation->setAnchorPoint(Vec2(0.0f, 0.0f));

			_buttonLeft->setVisible(true);
			_buttonRight->setVisible(true);
			_sidebar->setContentSize(Size(_contentSize.width, _contentSize.height - getCurrentFlexibleMax()));
		} else {
			_baseNode->setContentSize(_baseNode->getContentSize() - Size(20.0f, 0.0f));
			_navigation->setContentSize(Size(20.0f, _contentSize.height));
			_navigation->setPosition(_contentSize.width, 0.0f);
			_navigation->setAnchorPoint(Vec2(1.0f, 0.0f));

			_buttonLeft->setVisible(false);
			_buttonRight->setVisible(false);
			_sidebar->setContentSize(Size(_contentSize.width, _contentSize.height - getCurrentFlexibleMax()));
		}
	}

	_buttonLeft->setPosition(Vec2(0, 20.0f));
	_buttonLeft->setContentSize(cocos2d::Size(48.0f, _contentSize.height - 20.0f - getCurrentFlexibleMax()));

	_buttonRight->setPosition(Vec2(_contentSize.width, 20.0f));
	_buttonRight->setContentSize(cocos2d::Size(48.0f, _contentSize.height - 20.0f - getCurrentFlexibleMax()));

	_backButton->setPosition(16.0f, 24.0f + 8.0f);
}

void EpubView::visit(Renderer *r, const Mat4 &t, uint32_t f, ZPath &z) {
	if (!_visible) {
		return;
	}

	if (_backButtonStatus != None) {
		switch (_backButtonStatus) {
		case Show:
			onShowBackButton(_backButtonValue);
			_backButtonStatus = Wait;
			break;
		case Hide:
			onHideBackButton();
			_backButtonStatus = None;
			break;
		default:
			break;
		}
	}

	ToolbarLayout::visit(r, t, f, z);
}

void EpubView::onEnter() {
	ToolbarLayout::onEnter();
	_view->setScrollCallback([this] (float delta, bool finished) {
		onScroll(delta, finished);
		_navigation->setReaderProgress(_view->getScrollRelativePosition());
		hideBackButton();
		updateTitle();

		if (_sidebar->isNodeEnabled()) {
			_sidebar->hide();
		}
	});
	_view->setResultCallback([this] (rich_text::Result *res) {
		_navigation->onResult(res);
		_navigation->setReaderProgress(_view->getScrollRelativePosition());
	});

	updateTitle();
}

void EpubView::onExit() {
	_view->setScrollCallback(nullptr);
	_view->setResultCallback(nullptr);

	ToolbarLayout::onExit();
}

void EpubView::setTitle(const String &value) {
	_savedTitle = value;
	updateTitle();
}
const String &EpubView::getTitle() const {
	return _savedTitle;
}

void EpubView::setLocalHeader(bool value) {
	if (_localHeader != value) {
		_localHeader = value;
		updateTitle();
	}
}
bool EpubView::isLocalHeader() const {
	return _localHeader;
}

void EpubView::onScroll(float delta, bool finished) {
	if (!_view->isSelectionEnabled()) {
		ToolbarLayout::onScroll(delta, finished);
	}
}

void EpubView::setBookmarksSource(data::Source *source) {
	_contents->setBookmarksSource(source);
}

void EpubView::onRefreshButton() {
	_view->refresh();
}

void EpubView::setLayout(RichTextView::Layout l) {
	if (l == RichTextView::Horizontal) {
		setFlexibleToolbar(false);
		_view->setLayout(RichTextView::Horizontal);
		_layoutButton->setNameIcon(IconName::Stappler_layout_horizontal);
		_layoutButton->setName("horizontal");
	} else {
		setStatusBarTracked(true);
		setFlexibleToolbar(true);
		_view->setLayout(RichTextView::Vertical);
		_layoutButton->setNameIcon(IconName::Stappler_layout_vertical);
		_layoutButton->setName("vertical");
	}
}

void EpubView::setBookmarksEnabled(bool value) {
	_contents->setBookmarksEnabled(value);
}

bool EpubView::isBookmarksEnabled() const {
	return _contents->isBookmarksEnabled();
}

void EpubView::setExtendedNavigationEnabled(bool value) {
	if (value != _extendedNavigation) {
		_extendedNavigation = value;
		_navigation->setVisible(_extendedNavigation);
		_baseNode->setIndicatorVisible(!_extendedNavigation);
	}
}

bool EpubView::isExtendedNavigationEnabled() const {
	return _extendedNavigation;
}

void EpubView::onLayoutButton() {
	if (_layoutButton && _view) {
		auto l = _view->getLayout();
		if (l == RichTextView::Vertical) {
			setLayout(RichTextView::Horizontal);
		} else {
			setLayout(RichTextView::Vertical);
		}
	}
}

void EpubView::onButtonLeft() {
	if (_view->showPrevPage()) {
		_buttonLeft->setSwallowTouches(true);
		_buttonRight->setSwallowTouches(true);
	}
}

void EpubView::onButtonRight() {
	if (_view->showNextPage()) {
		_buttonLeft->setSwallowTouches(true);
		_buttonRight->setSwallowTouches(true);
	}
}

void EpubView::onReaderBackButton() {
	_view->setScrollRelativePosition(_backButtonValue);
	_navigation->setReaderProgress(_backButtonValue);
	hideBackButton();
}

void EpubView::onProgressPosition(float val) {
	_view->disableSelection();
	float v = _view->getScrollRelativePosition();
	_view->setScrollRelativePosition(val);
	showBackButton(v);
}

void EpubView::showBackButton(float val) {
	_backButtonStatus = Show;
	_backButtonValue = val;
}

void EpubView::hideBackButton() {
	if (_backButtonStatus == None && !isnan(_backButtonValue)) {
		_backButtonStatus = Hide;
	}
}

void EpubView::onShowBackButton(float val) {
	stopActionByTag(134);
	_backButton->clearAnimations();
	_backButton->setVisible(true);
	_backButton->stopAllActions();
	auto a = action::sequence(cocos2d::EaseQuadraticActionOut::create(
				cocos2d::MoveTo::create(0.35f, Vec2(16.0f, 24.0f + 8.0f))),
			[this] {
		_backButtonStatus = None;
	});
	_backButton->runAction(a);
	updateTitle();
}

void EpubView::onHideBackButton() {
	_backButtonValue = nan();
	_backButton->stopAllActions();
	auto a = cocos2d::Sequence::createWithTwoActions(
			cocos2d::EaseQuadraticActionIn::create(cocos2d::MoveTo::create(0.25f, Vec2(16.0f, -48.0f))),
			cocos2d::Hide::create());
	_backButton->runAction(a);
	stopActionByTag(134);
}

void EpubView::onContentsRef(const String &ref) {
	if (_view) {
		_view->onContentFile(ref);
		_sidebar->hide();
		updateTitle();
	}
}

void EpubView::onDocument(rich_text::Document *doc) {
	auto r = _view->getRenderer();
	auto res = r->getResult();
	if (_contents) {
		_contents->onResult(res);
	}
	updateTitle();
}

void EpubView::onBookmarkData(size_t object, size_t pos, float scroll) {
	auto res = _view->getResult();
	if (res) {
		const rich_text::Object * obj = res->getObject(object);
		if (obj && obj->type == rich_text::Object::Type::Label) {
			float objPos = float(pos) / float(obj->value.label.format.chars.size());
			_view->setViewPosition(RichTextView::ViewPosition{object, objPos, scroll}, true);
		}
	}

	_sidebar->hide();
}

void EpubView::updateTitle() {
	auto value = _view->getViewContentPosition();
	if (_currentBounds.idx == maxOf<size_t>() || value < _currentBounds.start || value >= _currentBounds.end) {
		auto r = _view->getRenderer();
		if (r) {
			auto res = r->getResult();
			if (res) {
				_currentBounds = res->getBoundsForPosition(value);
				// log::format("Bounds", "%f %f %f %d %s", value, _currentBounds.start, _currentBounds.end, _currentBounds.page, _currentBounds.label.c_str());
				if (_localHeader && !_currentBounds.label.empty()) {
					if (_savedTitle.empty()) {
						_toolbar->setTitle(_currentBounds.label);
					} else {
						_toolbar->setTitle(toString(_currentBounds.label, " — ", _savedTitle));
					}
				} else {
					_toolbar->setTitle(_savedTitle);
				}
			} else {
				_toolbar->setTitle(_savedTitle);
			}
		}
	}
}

void EpubView::onSelection(bool enabled) {
	if (enabled) {
		_buttonLeft->setEnabled(false);
		_buttonRight->setEnabled(false);
		showSelectionToolbar();
	} else {
		_buttonLeft->setEnabled(true);
		_buttonRight->setEnabled(true);
		hideSelectionToolbar();
	}
}

void EpubView::showSelectionToolbar() {
	_tmpCallback = _toolbar->getNavCallback();
	_tmpIconProgress = _toolbar->getNavButtonIconProgress();
	_toolbar->setActionMenuSource(_selectionSource);
	_toolbar->setMaxActionIcons(2);
	_toolbar->setNavButtonIconProgress(2.0f, 0.25f);
	_toolbar->setNavCallback([this] {
		_view->disableSelection();
	});
}
void EpubView::hideSelectionToolbar() {
	_toolbar->setActionMenuSource(_viewSource);
	_toolbar->setMaxActionIcons(3);
	_toolbar->setNavCallback(_tmpCallback);
	_toolbar->setNavButtonIconProgress(_tmpIconProgress, 0.25f);
}

void EpubView::onCopyButton() {

}
void EpubView::onBookmarkButton() {

}
void EpubView::onEmailButton() {

}
void EpubView::onShareButton() {

}
void EpubView::onMisprintButton() {

}

NS_MD_END

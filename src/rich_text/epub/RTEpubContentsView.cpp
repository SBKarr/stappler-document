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
#include "MaterialLabel.h"
#include "MaterialScroll.h"
#include "MaterialScrollSlice.h"
#include "MaterialResourceManager.h"
#include "MaterialButton.h"
#include "MaterialTabBar.h"
#include "MaterialRecyclerScroll.h"
#include "MaterialScrollFixed.h"

#include "RTEpubContentsView.h"

#include "SLDocument.h"
#include "SLResult.h"
#include "SPActions.h"
#include "SPGestureListener.h"

NS_MD_BEGIN

class EpubContentsNode : public MaterialNode {
public:
	using Callback = std::function<void(const String &)>;

	static Size compute(const data::Value &data, float width) {
		auto page = data.getInteger("page");
		auto font = material::FontType::Body_1;
		auto str = data.getString("label");
		if (page != maxOf<int64_t>()) {
			str += toString(" — %Shortcut:Pages%. ", page + 1);
		}
		auto labelSize = Label::getLabelSize(font, str, width - 32.0f, 0.0f, true);
		return Size(width, labelSize.height + 32.0f);
	}

	virtual bool init(const data::Value &data, Callback & cb) {
		if (!MaterialNode::init()) {
			return false;
		}

		auto page = data.getInteger("page");

		_callback = cb;
		_href = data.getString("href");

		_label = construct<Label>(material::FontType::Body_1);
		_label->setLocaleEnabled(true);
		_label->setString(data.getString("label"));
		_label->setAnchorPoint(Vec2(0.0f, 1.0f));
		if (page != maxOf<int64_t>()) {
			_label->appendTextWithStyle(toString(" — %Shortcut:Pages%. ", page + 1), Label::Style(Label::Opacity(100)));
		}
		_label->tryUpdateLabel();
		addChild(_label);

		_button = construct<material::Button>([this] {
			if (_available && _callback) {
				_callback(_href);
			}
		});
		_button->setStyle(material::Button::Style::FlatBlack);
		_button->setAnchorPoint(cocos2d::Vec2(0.0f, 0.0f));
		_button->setSwallowTouches(false);
		_button->setOpacity(255);
		_button->setEnabled(!_href.empty());
		addChild(_button, 2);

		setPadding(Padding(1.0f));
		setShadowZIndex(0.5f);

		return true;
	}

	virtual void onContentSizeDirty() override {
		MaterialNode::onContentSizeDirty();

		auto size = getContentSize();
		_label->setPosition(Vec2(16.0f, size.height - 16.0f));
		_label->setWidth(size.width - 32.0f);
		_label->tryUpdateLabel();

		_button->setPosition(getAnchorPositionWithPadding());
		_button->setContentSize(getContentSizeWithPadding());
	}

protected:
	bool _selected = false;
	bool _available = true;

	String _href;
	Callback _callback = nullptr;
	material::Label *_label = nullptr;
	material::Button *_button = nullptr;
};

class EpubContentsScroll : public Scroll {
public:
	using Callback = std::function<void(const String &)>;

	virtual bool init(const Callback &cb) {
		if (!Scroll::init()) {
			return false;
		}

		getGestureListener()->setSwallowTouches(true);
		_callback = cb;

		setHandlerCallback([this] (Scroll *s) -> Rc<Handler> {
			class Handler : public material::ScrollHandlerSlice {
			public:
				virtual bool init(Scroll *s) override {
					if (!ScrollHandlerSlice::init(s, nullptr)) {
						return false;
					}
					return true;
				}
			protected:
				virtual Rc<Scroll::Item> onItem(data::Value &&data, const Vec2 &origin) override {
					auto width = _size.width;
					Size size = EpubContentsNode::compute(data, width);
					return Rc<Scroll::Item>::create(std::move(data), origin, size);
				}
			};

			return Rc<Handler>::create(s);
		});

		setItemCallback( [this] (Item *item) -> Rc<MaterialNode> {
			return Rc<EpubContentsNode>::create(item->getData(), _callback);
		});

		setLookupLevel(10);
		setItemsForSubcats(true);
		setMaxSize(64);

		return true;
	}

	virtual void onResult(layout::Result *res) {
		_result = res;
		if (!res) {
			setSource(nullptr);
		} else {
			setSource(Rc<data::Source>::create(data::Source::ChildsCount(res->getBounds().size()),
					[this] (const data::Source::DataCallback &cb, data::Source::Id id) {
				if (_result) {
					auto &bounds = _result->getBounds();
					if (id.get() < bounds.size()) {
						auto &d = bounds.at(size_t(id.get()));
						cb(data::Value{
							pair("page", data::Value(d.page)),
							pair("label", data::Value(d.label)),
							pair("href", data::Value(d.href)),
						});
						return;
					}
				}
				cb(data::Value());
			}));
		}
	}

	virtual void setSelectedPosition(float value) {
		if (_result) {
			auto bounds = _result->getBoundsForPosition(value);
			setOriginId(data::Source::Id(bounds.idx));
			resetSlice();
		}
	}
protected:
	Rc<layout::Result> _result;
	float _maxPos = nan();
	Callback _callback;
};

class EpubBookmarkNode : public MaterialNode {
public:
	using Callback = std::function<void(size_t object, float position, float scroll)>;

	static Size compute(const data::Value &data, float width) {
		auto name = data.getString("item");
		auto label = data.getString("label");

		auto l = Label::getLabelSize(FontType::Body_2, name, width - 16.0f);
		auto n = Label::getLabelSize(FontType::Caption, label, width - 16.0f);

		return Size(width, 20.0f + l.height + n.height);
	}

	virtual bool init(const data::Value &data, const Size &size, const Callback &cb) {
		if (!MaterialNode::init()) {
			return false;
		}

		_callback = cb;
		_object = size_t(data.getInteger("object"));
		_position = size_t(data.getInteger("position"));
		_scroll = data.getDouble("scroll");

		_name = construct<Label>(FontType::Body_2);
		_name->setString(data.getString("item"));
		_name->setWidth(size.width - 16.0f);
		_name->setAnchorPoint(Vec2(0.0f, 1.0f));
		_name->setPosition(8.0f, size.height - 8.0f);
		addChild(_name, 1);

		_label = construct<Label>(FontType::Caption);
		_label->setString(data.getString("label"));
		_label->setWidth(size.width - 16.0f);
		_label->setAnchorPoint(Vec2(0.0f, 0.0f));
		_label->setPosition(8.0f, 8.0f);
		addChild(_label, 1);

		_button = construct<Button>(std::bind(&EpubBookmarkNode::onButton, this));
		_button->setStyle(Button::Style::FlatBlack);
		_button->setSwallowTouches(false);
		addChild(_button, 2);

		setPadding(1.0f);
		setShadowZIndex(1.0f);

		return true;
	}

	virtual void onContentSizeDirty() override {
		MaterialNode::onContentSizeDirty();

		_button->setPosition(getAnchorPositionWithPadding());
		_button->setContentSize(getContentSizeWithPadding());
	}

protected:
	virtual void onButton() {
		if (_callback) {
			_callback(_object, _position, _scroll);
		}
	}

	size_t _object = 0;
	size_t _position = 0;
	float _scroll = nan();

	Callback _callback;
	Label *_name = nullptr;
	Label *_label = nullptr;
	Button *_button = nullptr;
};

class EpubBookmarkScroll : public RecyclerScroll {
public:
	using Callback = std::function<void(size_t object, float position, float scroll)>;

	virtual bool init(const Callback &cb) {
		if (!RecyclerScroll::init()) {
			return false;
		}

		_callback = cb;
		getGestureListener()->setSwallowTouches(true);

		setHandlerCallback([this] (Scroll *s) -> Handler * {
			return construct<ScrollHandlerSlice>(s, [] (ScrollHandlerSlice *h, data::Value &&data, const Vec2 &origin) -> Rc<Scroll::Item> {
				auto width = h->getContentSize().width;
				Size size = EpubBookmarkNode::compute(data, width);
				return Rc<Scroll::Item>::create(std::move(data), origin, size);
			});
		});

		setItemCallback( [this] (Item *item) -> material::MaterialNode * {
			return construct<EpubBookmarkNode>(item->getData(), item->getContentSize(), _callback);
		});

		return true;
	}

protected:
	Callback _callback;
	data::Value _data;
};


bool EpubContentsView::init(const Callback &cb, const BookmarkCallback &bcb) {
	if (!MaterialNode::init()) {
		return false;
	}

	_header = construct<MaterialNode>();
	_header->setShadowZIndex(1.5f);
	_header->setBackgroundColor(Color::Grey_200);
	_header->setAnchorPoint(Vec2(0.0f, 1.0f));

	auto source = Rc<MenuSource>::create();
	source->addButton("Содержание", [this] (Button *b, MenuSourceButton *) {
		showContents();
	})->setSelected(true);
	source->addButton("Закладки", [this] (Button *b, MenuSourceButton *) {
		showBookmarks();
	});

	auto title = Rc<Label>::create(FontType::Subhead);
	title->setString("Содержание");
	title->setAnchorPoint(Anchor::MiddleLeft);
	_header->addChild(title);
	_title = title;

	_tabBar = construct<TabBar>(source);
	_tabBar->setAlignment(TabBar::Alignment::Justify);
	_tabBar->setAnchorPoint(Vec2(0.0f, 0.0f));
	_tabBar->setPosition(Vec2(0.0f, 0.0f));
	_header->addChild(_tabBar);

	addChild(_header, 2);

	_contentNode = construct<StrictNode>();
	_contentNode->setAnchorPoint(Vec2(0.0f, 0.0f));

	_contentsScroll = construct<EpubContentsScroll>(cb);
	_contentsScroll->setAnchorPoint(Vec2(0.0f, 0.0f));
	_contentNode->addChild(_contentsScroll, 1);

	_bookmarksScroll = construct<EpubBookmarkScroll>(bcb);
	_bookmarksScroll->setAnchorPoint(Vec2(0.0f, 0.0f));
	_contentNode->addChild(_bookmarksScroll, 2);

	addChild(_contentNode, 1);

	setShadowZIndex(1.5f);

	return true;
}

void EpubContentsView::onContentSizeDirty() {
	MaterialNode::onContentSizeDirty();
	auto size = getContentSizeWithPadding();

	if (_header) {
		_header->setContentSize(Size(size.width, 48.0f));
		_header->setPosition(getAnchorPositionWithPadding() + Vec2(0.0f, size.height));

		_tabBar->setContentSize(_header->getContentSize());
		_title->setPosition(16.0f, 24.0f);
	}

	_contentNode->setContentSize(Size(size.width, size.height - 48.0f));
	_contentNode->setPosition(getAnchorPositionWithPadding());

	if (_contentsScroll) {
		_contentsScroll->setVisible(fabs(_progress - 1.0f) > std::numeric_limits<float>::epsilon());
		_contentsScroll->setContentSize(_contentNode->getContentSize());
		_contentsScroll->setPosition(Vec2(- size.width * _progress, 0.0f));
	}

	if (_bookmarksScroll) {
		_bookmarksScroll->setVisible(fabs(_progress) > std::numeric_limits<float>::epsilon());
		_bookmarksScroll->setContentSize(_contentNode->getContentSize());
		_bookmarksScroll->setPosition(Vec2(size.width * (1.0f - _progress), 0.0f));
	}
}

void EpubContentsView::onResult(layout::Result *res) {
	_contentsScroll->onResult(res);
}

void EpubContentsView::setSelectedPosition(float value) {
	_contentsScroll->setSelectedPosition(value);
}

void EpubContentsView::setColors(const Color &bar, const Color &accent) {
	_header->setBackgroundColor(bar);
	_tabBar->setAccentColor(accent);
	_tabBar->setTextColor(bar.text());
	_tabBar->setSelectedColor(bar.text());
	_title->setColor(bar.text());
}

void EpubContentsView::setBookmarksSource(data::Source *source) {
	_bookmarksScroll->setSource(source);
}

void EpubContentsView::showBookmarks() {
	auto a = cocos2d::EaseQuadraticActionInOut::create(construct<ProgressAction>(0.25f, _progress, 1.0f, [this] (ProgressAction *a, float p) {
		_progress = p;
		_contentSizeDirty = true;
	}));
	runAction(a, "SwitchAction"_tag);
}

void EpubContentsView::showContents() {
	auto a = cocos2d::EaseQuadraticActionInOut::create(construct<ProgressAction>(0.25f, _progress, 0.0f, [this] (ProgressAction *a, float p) {
		_progress = p;
		_contentSizeDirty = true;
	}));
	runAction(a, "SwitchAction"_tag);
}

void EpubContentsView::setBookmarksEnabled(bool value) {
	if (_bookmarksEnabled != value) {
		_bookmarksEnabled = value;
		_tabBar->setVisible(_bookmarksEnabled);
		_title->setVisible(!_bookmarksEnabled);
	}
}

bool EpubContentsView::isBookmarksEnabled() const {
	return _bookmarksEnabled;
}

NS_MD_END

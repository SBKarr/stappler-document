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

#ifndef LIBS_MATERIAL_GUI_RICHTEXT_MATERIALRICHTEXTLISTENERVIEW_H_
#define LIBS_MATERIAL_GUI_RICHTEXT_MATERIALRICHTEXTLISTENERVIEW_H_

#include "RTSource.h"
#include "SPRichTextView.h"
#include "SPEventHeader.h"
#include "SPDynamicBatchNode.h"

NS_MD_BEGIN

class RichTextListenerView : public rich_text::View {
public:
	static EventHeader onSelection;

	struct SelectionPosition {
		size_t object;
		uint32_t position;
	};

	class Selection : public DynamicBatchNode {
	public:
		virtual bool init(RichTextListenerView *);

		virtual void clearSelection();
		virtual void selectLabel(const rich_text::Object *, const Vec2 &);
		virtual void selectWholeLabel();

		virtual bool onTap(int, const Vec2 &);

		virtual bool onPressBegin(const Vec2 &);
		virtual bool onLongPress(const Vec2 &, const TimeInterval &, int);
		virtual bool onPressEnd(const Vec2 &, const TimeInterval &);
		virtual bool onPressCancel(const Vec2 &);

		virtual bool onSwipeBegin(const Vec2 &);
		virtual bool onSwipe(const Vec2 &, const Vec2 &);
		virtual bool onSwipeEnd(const Vec2 &);

		virtual void setEnabled(bool);
		virtual bool isEnabled() const;

		virtual bool hasSelection() const;

		virtual String getSelectedString(size_t maxWords) const;
		virtual Pair<SelectionPosition, SelectionPosition> getSelectionPosition() const;

	protected:
		virtual const rich_text::Object *getSelectedObject(rich_text::Result *, const Vec2 &) const;
		virtual const rich_text::Object *getSelectedObject(rich_text::Result *, const Vec2 &, size_t pos, int32_t offset) const;

	    virtual void updateBlendFunc(cocos2d::Texture2D *) override;
		virtual void emplaceRect(const Rect &, size_t idx, size_t count);
		virtual void updateRects();

		RichTextListenerView *_view = nullptr;
		const rich_text::Object *_object = nullptr;
		size_t _index = 0;
		Pair<SelectionPosition, SelectionPosition> _selectionBounds;

		draw::PathNode *_markerStart = nullptr;
		draw::PathNode *_markerEnd = nullptr;
		draw::PathNode *_markerTarget = nullptr;
		bool _enabled = false;
	};

	virtual ~RichTextListenerView();
	virtual bool init(Layout, rich_text::Source * = nullptr, const Vector<String> &ids = {}) override;

	virtual void setLayout(Layout l) override;
	virtual void setUseSelection(bool);

	virtual void disableSelection();
	virtual bool isSelectionEnabled() const;

	virtual String getSelectedString(size_t maxWords = maxOf<size_t>()) const;
	virtual Pair<SelectionPosition, SelectionPosition> getSelectionPosition() const;

protected:
	virtual void onTap(int count, const Vec2 &loc) override;
	virtual void onLightLevelChanged();

	virtual bool onSwipeEventBegin(const Vec2 &loc, const Vec2 &d, const Vec2 &v) override;
	virtual bool onSwipeEvent(const Vec2 &loc, const Vec2 &d, const Vec2 &v) override;
	virtual bool onSwipeEventEnd(const Vec2 &loc, const Vec2 &d, const Vec2 &v) override;

	virtual bool onPressBegin(const Vec2 &) override;
	virtual bool onLongPress(const Vec2 &, const TimeInterval &, int count) override;
	virtual bool onPressEnd(const Vec2 &, const TimeInterval &) override;
	virtual bool onPressCancel(const Vec2 &, const TimeInterval &) override;

	virtual void onPosition() override;
	virtual void onRenderer(rich_text::Result *, bool) override;

	bool _useSelection = false;

	EventListener *_eventListener = nullptr;
	Selection *_selection = nullptr;
};

NS_MD_END

#endif /* LIBS_MATERIAL_GUI_RICHTEXT_MATERIALRICHTEXTLISTENERVIEW_H_ */

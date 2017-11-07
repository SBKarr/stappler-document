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

#ifndef LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTVIEW_H_
#define LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTVIEW_H_

#include "RTListenerView.h"
#include "SPEventHeader.h"

NS_MD_BEGIN

class RichTextTooltip;

class RichTextView : public RichTextListenerView {
public:
	static EventHeader onImageLink;
	static EventHeader onExternalLink;
	static EventHeader onContentLink;
	static EventHeader onError;
	static EventHeader onDocument;
	static EventHeader onLayout;

	using PositionCallback = std::function<void(float)>;
	using PageData = rich_text::PageData;

	struct ViewPosition {
		size_t object;
		float position;
		float scroll;
	};

	class PageWithLabel : public Page {
	public:
		virtual bool init(const PageData &, float) override;
		virtual void onContentSizeDirty() override;

	protected:
		Label *_label;
	};

	class Highlight : public DynamicBatchNode {
	public:
		virtual bool init(RichTextView *);
	    virtual void visit(Renderer *, const Mat4 &, uint32_t, ZPath &) override;

		virtual void clearSelection();
		virtual void addSelection(const Pair<SelectionPosition, SelectionPosition> &);

		virtual void setDirty();
	protected:
	    virtual void updateBlendFunc(cocos2d::Texture2D *) override;
		virtual void emplaceRect(const Rect &, size_t idx, size_t count);
		virtual void updateRects();

		RichTextView *_view = nullptr;
		Vector<Pair<SelectionPosition, SelectionPosition>> _selectionBounds;

		bool _dirty = false;
		bool _enabled = false;
	};

	virtual ~RichTextView();

	virtual bool init(rich_text::Source *);
	virtual bool init(Layout, rich_text::Source *);
	virtual void onContentSizeDirty() override;
	virtual void setLayout(Layout l) override;

	virtual void setOverscrollFrontOffset(float value) override;
	virtual void setSource(rich_text::Source *) override;

	virtual void setProgressColor(const Color &);

	virtual float getViewContentPosition(float = nan()) const;
	virtual ViewPosition getViewObjectPosition(float) const;

	virtual ViewPosition getViewPosition() const;
	virtual void setViewPosition(const ViewPosition &, bool offseted = false);

	virtual void setPositionCallback(const PositionCallback &);
	virtual const PositionCallback &getPositionCallback() const;

	virtual void onContentFile(const String &);

	virtual void setRenderingEnabled(bool);

	virtual void clearHighlight();
	virtual void addHighlight(const Pair<SelectionPosition, SelectionPosition> &);
	virtual void addHighlight(const SelectionPosition &, const SelectionPosition &);

	virtual float getBookmarkScrollPosition(size_t, uint32_t) const;

protected:
	virtual void onPosition() override;

	virtual void onRestorePosition(rich_text::Result *res, float pos);
	virtual void onRenderer(rich_text::Result *, bool) override;

	virtual void onLightLevelChanged() override;
	virtual void onLink(const String &, const Vec2 &);
	virtual void onId(const String &, const Vec2 &);
	virtual void onImage(const String &id, const Vec2 &);
	virtual void onGallery(const String &name, const String &image, const Vec2 &);
	virtual void onFile(const String &, const Vec2 &);

	virtual void onObjectPressEnd(const Vec2 &, const rich_text::Object &) override;

	virtual void onSourceError(rich_text::Source::Error);
	virtual void onSourceUpdate();
	virtual void onSourceAsset();

	virtual void updateProgress();

	virtual Page *onConstructPageNode(const PageData &, float) override;

	float _savedFontScale = nan();
	Size _renderSize;
	ViewPosition _savedPosition;
	bool _layoutChanged = false;
	bool _rendererUpdate = false;
	bool _renderingEnabled = true;
	EventHandlerNode *_sourceErrorListener = nullptr;
	EventHandlerNode *_sourceUpdateListener = nullptr;
	EventHandlerNode *_sourceAssetListener = nullptr;
	LinearProgress *_progress = nullptr;
	RichTextTooltip *_tooltip = nullptr;
	PositionCallback _positionCallback = nullptr;
	Highlight *_highlight = nullptr;
};

NS_MD_END

#endif /* LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTVIEW_H_ */

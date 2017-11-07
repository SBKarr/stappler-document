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

#ifndef CLASSES_GUI_GALLERYLAYOUT_H_
#define CLASSES_GUI_GALLERYLAYOUT_H_

#include "MaterialToolbarLayout.h"
#include "SPLayoutSource.h"

NS_MD_BEGIN

class RichTextGalleryLayout : public ToolbarLayout {
public:
	virtual bool init(layout::Source *source, const std::string &name, const std::string &sel);
	virtual void onContentSizeDirty() override;

	virtual void onEnter() override;

	virtual void onForegroundTransitionBegan(ContentLayer *l, Layout *overlay) override;
	virtual void onPush(ContentLayer *l, bool replace) override;

protected:
	void onPosition(float val);

	void onImage(const String &, const Function<void(cocos2d::Texture2D *)> &);
	void onAssetCaptured(const String &image, const Function<void(cocos2d::Texture2D *)> &tex);

	Rc<layout::Source> _source;
	String _name;
	Vector<String> _title;

	GalleryScroll *_scroll;
};

NS_MD_END

#endif /* CLASSES_GUI_GALLERYLAYOUT_H_ */

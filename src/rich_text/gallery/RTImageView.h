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

#ifndef LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTIMAGEVIEW_H_
#define LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTIMAGEVIEW_H_

#include "MaterialLayout.h"
#include "RTTooltip.h"
#include "SPLayoutSource.h"

NS_MD_BEGIN

class RichTextImageView : public Layout {
public:
	virtual ~RichTextImageView();
	virtual bool init(layout::Source *, const String &id, const String &src, const String &alt = "");

	virtual void onContentSizeDirty() override;

	virtual void close();

protected:
	virtual void onExpand();
	virtual Rc<cocos2d::Texture2D> readImage(const String &);
	virtual void onImage(cocos2d::Texture2D *);
	virtual void onAssetCaptured(const String &);

	RichTextTooltip *_tooltip = nullptr;
	MenuSourceButton *_expandButton = nullptr;
	bool _expanded = true;

	ImageLayer *_sprite = nullptr;
	Rc<layout::Source> _source;
};

NS_MD_END

#endif /* LIBS_MATERIAL_GUI_RICHTEXTVIEW_MATERIALRICHTEXTIMAGEVIEW_H_ */

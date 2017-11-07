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

#ifndef LIBS_LIBRARY_RICH_TEXT_EPUB_RTEPUBCONTENTSVIEW_H_
#define LIBS_LIBRARY_RICH_TEXT_EPUB_RTEPUBCONTENTSVIEW_H_

#include "MaterialNode.h"

NS_MD_BEGIN

class EpubContentsScroll;
class EpubBookmarkScroll;

class EpubContentsView : public MaterialNode {
public:
	using Callback = std::function<void(const String &)>;
	using BookmarkCallback = std::function<void(size_t object, size_t position, float scroll)>;
	using Bookmark = std::function<void(const String &)>;

	virtual bool init(const Callback &, const BookmarkCallback &);
	virtual void onContentSizeDirty() override;

	virtual void onResult(layout::Result *);

	virtual void setSelectedPosition(float);
	virtual void setColors(const Color &bar, const Color &accent);

	virtual void setBookmarksSource(data::Source *);

	virtual void setBookmarksEnabled(bool);
	virtual bool isBookmarksEnabled() const;

protected:
	virtual void showBookmarks();
	virtual void showContents();

	bool _bookmarksEnabled = true;
	StrictNode *_contentNode = nullptr;
	TabBar *_tabBar = nullptr;
	material::Label *_title = nullptr;
	MaterialNode *_header = nullptr;
	EpubContentsScroll *_contentsScroll = nullptr;
	EpubBookmarkScroll *_bookmarksScroll = nullptr;
	float _progress = 0.0f;
};

NS_MD_END

#endif /* LIBS_LIBRARY_RICH_TEXT_EPUB_RTEPUBCONTENTSVIEW_H_ */

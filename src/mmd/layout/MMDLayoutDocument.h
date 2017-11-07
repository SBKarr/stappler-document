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

#ifndef EXTENSIONS_DOCUMENT_SRC_MMD_LAYOUT_MMDLAYOUTDOCUMENT_H_
#define EXTENSIONS_DOCUMENT_SRC_MMD_LAYOUT_MMDLAYOUTDOCUMENT_H_

#include "MMDCommon.h"
#include "SLDocument.h"

NS_MMD_BEGIN

class LayoutDocument : public layout::Document {
public:
	static DocumentFormat MmdFormat;

	using Style = layout::Style;
	using Node = layout::Node;
	using MediaParameters = layout::MediaParameters;

	virtual ~LayoutDocument() { }

	virtual bool init(const FilePath &, const String &ct = String());
	virtual bool init(const DataReader<ByteOrder::Network> &, const String &ct = String());

	// Default style, that can be redefined with css
	virtual Style beginStyle(const Node &, const Vector<const Node *> &, const MediaParameters &) const override;

	// Default style, that can NOT be redefined with css
	virtual Style endStyle(const Node &, const Vector<const Node *> &, const MediaParameters &) const override;

protected:
	friend class LayoutProcessor;

	layout::ContentPage *acquireRootPage();
};

NS_MMD_END

#endif /* EXTENSIONS_DOCUMENT_SRC_MMD_LAYOUT_MMDLAYOUTDOCUMENT_H_ */

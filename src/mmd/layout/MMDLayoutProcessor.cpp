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

#include "SPLayout.h"
#include "MMDLayoutProcessor.h"
#include "MMDLayoutDocument.h"
#include "SLParser.h"
#include "SLStyle.h"

NS_MMD_BEGIN

bool LayoutProcessor::init(LayoutDocument *doc) {
	_document = doc;
	_page = _document->acquireRootPage();
	_nodeStack.push_back(&_page->root);

	layout::Style style;
	_page->root.pushStyle(move(style));

	_page->strings.insert(pair(layout::CssStringId("monospace"_hash), "monospace"));

	auto mediaCssStringFn = [&] (layout::CssStringId strId, const StringView &string) {
		_page->strings.insert(pair(strId, string.str()));
	};

	_minWidthQuery = _page->queries.size();
	_page->queries.emplace_back();
	_page->queries.back().parse("max-width:500px", mediaCssStringFn);

	_mediumWidthQuery = _page->queries.size();
	_page->queries.emplace_back();
	_page->queries.back().parse("min-width:500px and max-width:750px", mediaCssStringFn);

	_maxWidthQuery = _page->queries.size();
	_page->queries.emplace_back();
	_page->queries.back().parse("min-width:750px", mediaCssStringFn);

	return true;
}

void LayoutProcessor::processHtml(const Content &c, const StringView &str, const Token &t) {
	source = str;
	exportTokenTree(buffer, t);
	flushBuffer();
}

void LayoutProcessor::addCssString(const stappler::String &origStr) {
	auto str = layout::parser::resolveCssString(StringView(origStr));
	layout::CssStringId value = hash::hash32(str.data(), str.size());
	_page->strings.insert(pair(value, str.str()));
}

void LayoutProcessor::processTagStyle(const StringView &name, layout::Style &style) {
	style.merge(_nodeStack.back()->getStyle(), true);
}

void LayoutProcessor::processStyle(const StringView &name, layout::Style &style, const StringView &styleData) {
	StringViewUtf8 r(styleData);
	layout::parser::readHtmlStyleValue(r, [&] (const stappler::String &name, const stappler::String &value) {
		if (name == "font-family" || name == "background-image") {
			addCssString(value);
		}
		style.read(name, value);
	});
}

template <typename T>
void LayoutProcessor_processAttr(LayoutProcessor &p, const T &container, const StringView &name,
		stappler::Map<stappler::String, stappler::String> &attributes, layout::Style &style, StringView &id) {
	for (auto &it : container) {
		if (it.first == "id") {
			id = it.second;
		} else if (it.first == "style") {
			p.processStyle(name, style, it.second);
			attributes.emplace(it.first.str(), it.second.str());
		} else if (name == "img" && it.first == "src") {
			p._page->assets.push_back(it.second.str());
			attributes.emplace(it.first.str(), it.second.str());
		} else {
			attributes.emplace(it.first.str(), it.second.str());
		}
	}
}

layout::Node *LayoutProcessor::makeNode(const StringView &name, InitList &&attr, VecList &&vec) {
	stappler::Map<stappler::String, stappler::String> attributes;
	layout::Style style;
	StringView id;
	processTagStyle(name, style);
	LayoutProcessor_processAttr(*this, attr, name, attributes, style, id);
	LayoutProcessor_processAttr(*this, vec, name, attributes, style, id);

	auto &node = _nodeStack.back()->pushNode(name.str(), id.str(), style, std::move(attributes));

	return &node;
}

void LayoutProcessor::pushNode(const StringView &name, InitList &&attr, VecList &&vec) {
	flushBuffer();
	auto node = makeNode(name, move(attr), move(vec));
	_nodeStack.push_back(node);
}

void LayoutProcessor::pushInlineNode(const StringView &name, InitList &&attr, VecList &&vec) {
	flushBuffer();
	makeNode(name, move(attr), move(vec));
}

void LayoutProcessor::popNode() {
	flushBuffer();
	_nodeStack.pop_back();
}

void LayoutProcessor::flushBuffer() {
	auto str = buffer.str();
	StringView r(str);
	if (!r.empty()) {
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (r.empty()) {
			auto b = _nodeStack.back();
			if (b->hasValue()) {
				b->pushValue(" ");
			}
		} else {
			r = StringView(str);
			if (r.back() == '\n') {
				size_t s = r.size();
				while (s > 0 && (r[s - 1] == '\n' || r[s - 1] == '\r')) {
					-- s;
				}
				_nodeStack.back()->pushValue(StringView(r.data(), s).str());
			} else {
				_nodeStack.back()->pushValue(r.str());
			}
		}
	}
	buffer.clear();
}

NS_MMD_END

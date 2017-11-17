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
#include "MMDLayoutDocument.h"
#include "MMDEngine.h"
#include "MMDLayoutProcessor.h"

#include "SPCharReader.h"

NS_MMD_BEGIN

bool LayoutDocument::isMmdData(const DataReader<ByteOrder::Network> &data) {
	StringView str((const char *)data.data(), data.size());
	str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	if (str.is("#") || str.is("{{TOC}}") || str.is("Title:")) {
		return true;
	}

	str.skipUntilString("\n#", true);
	if (str.is("\n#")) {
		return true;
	}

	return false;
}

bool LayoutDocument::isMmdFile(const String &path) {
	auto ext = filepath::lastExtension(path);
	if (ext == "md" || ext == "markdown") {
		return true;
	}

	if (ext == "text" || ext == "txt" || ext.empty()) {
		if (auto file = filesystem::openForReading(path)) {
			StackBuffer<512> data;
			if (io::Producer(file).seekAndRead(0, data, 512) > 0) {
				return isMmdData(DataReader<ByteOrder::Network>(data.data(), data.size()));
			}
		}
	}

	return false;
}

static bool checkMmdFile(const String &path, const String &ct) {
	StringView ctView(ct);

	return ctView.is("text/markdown") || ctView.is("text/x-markdown") || LayoutDocument::isMmdFile(path);
}

static Rc<layout::Document> loadMmdFile(const String &path, const String &ct) {
	return Rc<LayoutDocument>::create(FilePath(path), ct);
}

static bool checkMmdData(const DataReader<ByteOrder::Network> &data, const String &ct) {
	StringView ctView(ct);

	return ctView.is("text/markdown") || ctView.is("text/x-markdown") || LayoutDocument::isMmdData(data);
}

static Rc<layout::Document> loadMmdData(const DataReader<ByteOrder::Network> &data, const String &ct) {
	return Rc<LayoutDocument>::create(data, ct);
}

LayoutDocument::DocumentFormat LayoutDocument::MmdFormat(&checkMmdFile, &loadMmdFile, &checkMmdData, &loadMmdData);

bool LayoutDocument::init(const FilePath &path, const String &ct) {
	if (path.get().empty()) {
		return false;
	}

	_filePath = path.get();

	auto data = filesystem::readFile(path.get());

	return init(data, ct);
}

bool LayoutDocument::init(const DataReader<ByteOrder::Network> &data, const String &ct) {
	if (data.empty()) {
		return false;
	}

	Engine e; e.init(StringView((const char *)data.data(), data.size()), StapplerExtensions);
	e.process([&] (const Content &c, const StringView &s, const Token &t) {
		LayoutProcessor p; p.init(this);
		p.process(c, s, t);
	});

	return !_pages.empty();
}

layout::ContentPage *LayoutDocument::acquireRootPage() {
	if (_pages.empty()) {
		_pages.emplace(String(), layout::ContentPage{String(), layout::Node("body", String()), true});
	}

	return &(_pages.begin()->second);
}

static void LayoutDocument_onTag(layout::Style &style, const StringView &tag, const StringView &parent) {
	using namespace layout;
	using namespace layout::style;

	if (tag == "div") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
	}

	if (tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
		if (parent != "li" && parent != "blockquote") {
			style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
			style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));
			if (parent != "dd" && parent != "figcaption") {
				style.data.push_back(Parameter::create<ParameterName::TextIndent>(Metric(1.5f, Metric::Units::Rem)));
			}
		}
		style.data.push_back(Parameter::create<ParameterName::LineHeight>(Metric(1.2f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
	}

	if (tag == "span" || tag == "strong" || tag == "em" || tag == "nobr"
			|| tag == "sub" || tag == "sup" || tag == "inf" || tag == "b"
			|| tag == "i" || tag == "u" || tag == "code") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Inline));
	}

	if (tag == "h1") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(uint8_t(32)));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W200));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(222)));

	} else if (tag == "h2") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(uint8_t(28)));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W400));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(222)));

	} else if (tag == "h3") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(FontSize::XXLarge));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W400));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(200)));

	} else if (tag == "h4") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(FontSize::XLarge));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W500));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(188)));

	} else if (tag == "h5") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(uint8_t(18)));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W400));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(222)));

	} else if (tag == "h6") {
		style.set(Parameter::create<ParameterName::MarginTop>(Metric(0.8f, Metric::Units::Em)), true);
		style.data.push_back(Parameter::create<ParameterName::FontSize>(FontSize::Large));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W500));
		style.data.push_back(Parameter::create<ParameterName::Opacity>(uint8_t(216)));

	} else if (tag == "p") {
		style.data.push_back(Parameter::create<ParameterName::TextAlign>(TextAlign::Justify));
		style.data.push_back(Parameter::create<ParameterName::Hyphens>(Hyphens::Auto));

	} else if (tag == "hr") {
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginRight>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginLeft>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::Height>(Metric(2, Metric::Units::Px)));
		style.data.push_back(Parameter::create<ParameterName::BackgroundColor>(Color4B(0, 0, 0, 127)));

	} else if (tag == "a") {
		style.data.push_back(Parameter::create<ParameterName::TextDecoration>(TextDecoration::Underline));
		style.data.push_back(Parameter::create<ParameterName::Color>(Color3B(0x0d, 0x47, 0xa1)));

	} else if (tag == "b" || tag == "strong") {
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::Bold));

	} else if (tag == "i" || tag == "em") {
		style.data.push_back(Parameter::create<ParameterName::FontStyle>(FontStyle::Italic));

	} else if (tag == "u") {
		style.data.push_back(Parameter::create<ParameterName::TextDecoration>(TextDecoration::Underline));

	} else if (tag == "nobr") {
		style.data.push_back(Parameter::create<ParameterName::WhiteSpace>(WhiteSpace::Nowrap));

	} else if (tag == "pre") {
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::WhiteSpace>(WhiteSpace::Pre));
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::BackgroundColor>(Color4B(228, 228, 228, 255)));

		style.data.push_back(Parameter::create<ParameterName::PaddingTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::PaddingBottom>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::PaddingRight>(Metric(0.5f, Metric::Units::Em)));

	} else if (tag == "code") {
		style.data.push_back(Parameter::create<ParameterName::FontFamily>(CssStringId("monospace"_hash)));
		style.data.push_back(Parameter::create<ParameterName::BackgroundColor>(Color4B(228, 228, 228, 255)));

	} else if (tag == "sub" || tag == "inf") {
		style.data.push_back(Parameter::create<ParameterName::VerticalAlign>(VerticalAlign::Sub));
		style.data.push_back(Parameter::create<ParameterName::FontSizeIncrement>(FontSizeIncrement::XSmaller));

	} else if (tag == "sup") {
		style.data.push_back(Parameter::create<ParameterName::VerticalAlign>(VerticalAlign::Super));
		style.data.push_back(Parameter::create<ParameterName::FontSizeIncrement>(FontSizeIncrement::XSmaller));

	} else if (tag == "body") {
		style.data.push_back(Parameter::create<ParameterName::MarginLeft>(Metric(0.8f, Metric::Units::Rem), MediaQuery::IsScreenLayout));
		style.data.push_back(Parameter::create<ParameterName::MarginRight>(Metric(0.8f, Metric::Units::Rem), MediaQuery::IsScreenLayout));

	} else if (tag == "br") {
		style.data.push_back(Parameter::create<ParameterName::WhiteSpace>(style::WhiteSpace::PreLine));
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Inline));

	} else if (tag == "li") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::ListItem));
		style.data.push_back(Parameter::create<ParameterName::LineHeight>(Metric(1.2f, Metric::Units::Em)));

	} else if (tag == "ol") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::ListStyleType>(ListStyleType::Decimal));
		style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(2.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::XListStyleOffset>(Metric(0.6f, Metric::Units::Rem)));

	} else if (tag == "ul") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		if (parent == "li") {
			style.data.push_back(Parameter::create<ParameterName::ListStyleType>(ListStyleType::Circle));
		} else {
			style.data.push_back(Parameter::create<ParameterName::ListStyleType>(ListStyleType::Disc));
		}
		style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(2.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.25f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::XListStyleOffset>(Metric(0.6f, Metric::Units::Rem)));

	} else if (tag == "img") {
		style.data.push_back(Parameter::create<ParameterName::BackgroundSizeWidth>(Metric(1.0, Metric::Units::Contain)));
		style.data.push_back(Parameter::create<ParameterName::BackgroundSizeHeight>(Metric(1.0, Metric::Units::Contain)));
		style.data.push_back(Parameter::create<ParameterName::PageBreakInside>(PageBreak::Avoid));
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::InlineBlock));

	} else if (tag == "table") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::None));

	} else if (tag == "blockquote") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));

		if (parent == "blockquote") {
			style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(0.8f, Metric::Units::Rem)));
		} else {
			style.data.push_back(Parameter::create<ParameterName::MarginLeft>(Metric(1.5f, Metric::Units::Rem)));
			style.data.push_back(Parameter::create<ParameterName::MarginRight>(Metric(1.5f, Metric::Units::Rem)));
			style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(1.0f, Metric::Units::Rem)));
		}

		style.data.push_back(Parameter::create<ParameterName::PaddingTop>(Metric(0.1f, Metric::Units::Rem)));
		style.data.push_back(Parameter::create<ParameterName::PaddingBottom>(Metric(0.3f, Metric::Units::Rem)));

		style.data.push_back(Parameter::create<ParameterName::BorderLeftColor>(Color4B(0, 0, 0, 64)));
		style.data.push_back(Parameter::create<ParameterName::BorderLeftWidth>(Metric(3, Metric::Units::Px)));
		style.data.push_back(Parameter::create<ParameterName::BorderLeftStyle>(BorderStyle::Solid));

	} else if (tag == "dl") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginLeft>(Metric(1.5f, Metric::Units::Rem)));

	} else if (tag == "dt") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W700));

	} else if (tag == "dd") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::PaddingLeft>(Metric(1.0f, Metric::Units::Rem)));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));

		style.data.push_back(Parameter::create<ParameterName::BorderLeftColor>(Color4B(0, 0, 0, 64)));
		style.data.push_back(Parameter::create<ParameterName::BorderLeftWidth>(Metric(2, Metric::Units::Px)));
		style.data.push_back(Parameter::create<ParameterName::BorderLeftStyle>(BorderStyle::Solid));

	} else if (tag == "figure") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(1.0f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));

	} else if (tag == "figcaption") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::MarginTop>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::MarginBottom>(Metric(0.5f, Metric::Units::Em)));
		style.data.push_back(Parameter::create<ParameterName::FontSize>(FontSize::Small));
		style.data.push_back(Parameter::create<ParameterName::FontWeight>(FontWeight::W500));
		style.data.push_back(Parameter::create<ParameterName::TextAlign>(TextAlign::Center));

	}
}

static void LayoutDocument_onClass(layout::Style &style, const StringView &name, const StringView &classStr) {
	using namespace layout;
	using namespace layout::style;

	if (classStr == "math") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::None));
	}

	if (name == "figure") {
		if (classStr == "middle") {
			style.data.push_back(Parameter::create<ParameterName::Float>(Float::Right));
			style.data.push_back(Parameter::create<ParameterName::Width>(Metric(1.0f, Metric::Units::Percent)));
		}

	} else if (name == "img") {
		style.data.push_back(Parameter::create<ParameterName::Display>(Display::Block));
		style.data.push_back(Parameter::create<ParameterName::MaxWidth>(Metric(70.0f, Metric::Units::Vw)));
		style.data.push_back(Parameter::create<ParameterName::MaxHeight>(Metric(70.0f, Metric::Units::Vh)));
		style.data.push_back(Parameter::create<ParameterName::MinWidth>(Metric(100.8f, Metric::Units::Px)));
		style.data.push_back(Parameter::create<ParameterName::MinHeight>(Metric(88.8f, Metric::Units::Px)));

		style.data.push_back(Parameter::create<ParameterName::MarginRight>(Metric(0.0f, Metric::Units::Auto)));
		style.data.push_back(Parameter::create<ParameterName::MarginLeft>(Metric(0.0f, Metric::Units::Auto)));

	} else if (name == "figcaption") {
		style.data.push_back(Parameter::create<ParameterName::TextAlign>(TextAlign::Center));

	}
}

// Default style, that can be redefined with css
layout::Style LayoutDocument::beginStyle(const Node &node, const Vector<const Node *> &stack, const MediaParameters &media) const {
	const Node *parent = nullptr;
	if (stack.size() > 1) {
		parent = stack.at(stack.size() - 2);
	}

	Style style;
	LayoutDocument_onTag(style, node.getHtmlName(), parent ? StringView(parent->getHtmlName()) : StringView());

	auto &attr = node.getAttributes();
	for (auto &it : attr) {
		if (it.first == "class") {
			StringView r(it.second);
			r.split<StringView::CharGroup<CharGroupId::WhiteSpace>>([&] (const StringView &classStr) {
				LayoutDocument_onClass(style, node.getHtmlName(), classStr);
			});
		} else if ((node.getHtmlName() == "img" || node.getHtmlName() == "figcaption") && it.first == "type") {
			LayoutDocument_onClass(style, node.getHtmlName(), it.second);
		} else {
			onStyleAttribute(style, node.getHtmlName(), it.first, it.second, media);
		}
	}

	return style;
}

// Default style, that can NOT be redefined with css
layout::Style LayoutDocument::endStyle(const Node &node, const Vector<const Node *> &stack, const MediaParameters &media) const {
	return Document::endStyle(node, stack, media);
}

NS_MMD_END

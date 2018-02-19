/*

	Copyright © 2016 - 2017 Fletcher T. Penney.
	Copyright © 2017 Roman Katuntsev <sbkarr@stappler.org>


	The `MultiMarkdown 6` project is released under the MIT License..

	GLibFacade.c and GLibFacade.h are from the MultiMarkdown v4 project:

		https://github.com/fletcher/MultiMarkdown-4/

	MMD 4 is released under both the MIT License and GPL.


	CuTest is released under the zlib/libpng license. See CuTest.c for the text
	of the license.


	## The MIT License ##

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

*/

#include "SPCommon.h"
#include "MMDHtmlProcessor.h"
#include "MMDEngine.h"
#include "MMDContent.h"
#include "MMDChars.h"
#include "MMDCore.h"
#include "SPString.h"

NS_MMD_BEGIN

using Traits = string::ToStringTraits<memory::PoolInterface>;

void HtmlProcessor::exportBlockquote(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("blockquote");
	if (!spExt) { out << "\n"; }
	padded = 2;
	exportTokenTree(out, t->child);
	pad(out, 1);
	popNode();
	padded = 0;
}

void HtmlProcessor::exportDefinition(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("dd");

	auto temp_short = list_is_tight;
	if (t->child) {
		if (!(t->child->next && (t->child->next->type == BLOCK_EMPTY) && t->child->next->next)) {
			list_is_tight = true;
		}

		exportTokenTree(out, t->child);
	}

	popNode();
	padded = 0;
	list_is_tight = temp_short;
}

void HtmlProcessor::exportDefList(std::ostream &out, token *t) {
	pad(out, 2);

	// Group consecutive definition lists into a single list.
	// lemon's LALR(1) parser can't properly handle this (to my understanding).

	if (!(t->prev && (t->prev->type == BLOCK_DEFLIST))) {
		pushNode("dl");
		if (!spExt) { out << "\n"; }
	}

	padded = 2;
	exportTokenTree(out, t->child);
	pad(out, 1);

	if (!(t->next && (t->next->type == BLOCK_DEFLIST))) {
		popNode();
		if (!spExt) { out << "\n"; }
	}
	padded = 1;
}

void HtmlProcessor::exportDefTerm(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("dt");
	exportTokenTree(out, t->child);
	popNode();
	if (!spExt) { out << "\n"; }
	padded = 2;
}

void HtmlProcessor::exportFencedCodeBlock(std::ostream &out, token *t) {
	pad(out, 2);

	token * temp_token = nullptr;
	StringView temp_char = getFenceLanguageSpecifier(source, t->child->child);

	if (!temp_char.empty()) {
		if (temp_char.is("{=")) {
			// Raw source
			if (rawFilterTextMatches(temp_char, "html")) {
				switch (t->child->tail->type) {
					case LINE_FENCE_BACKTICK_3:
					case LINE_FENCE_BACKTICK_4:
					case LINE_FENCE_BACKTICK_5:
						temp_token = t->child->tail;
						break;

					default:
						temp_token = nullptr;
				}

				if (temp_token) {
					out << StringView(&source[t->child->next->start], temp_token->start - t->child->next->start);
					padded = 1;
				} else {
					out << StringView(&source[t->child->start + t->child->len], t->start + t->len - t->child->next->start);
					padded = 0;
				}
			}
			return;
		}

		pushNode("pre");
		pushNode("code", { pair("class", temp_char) });
	} else {
		pushNode("pre"); pushNode("code");
	}

	exportTokenTreeRaw(out, t->child->next);
	popNode(); popNode();
	padded = 0;
}

void HtmlProcessor::exportIndentedCodeBlock(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("pre"); pushNode("code");

	exportTokenTreeRaw(out, t->child);
	popNode(); popNode();
	padded = 0;
}

static Content::String HtmlProcessor_decodeHeaderIdLabel(const StringView &source, token *t, bool isManual) {
	StringView r(&source[t->start], t->len);

	if (isManual) {
		r = r.readUntil<StringView::Chars<'['>>();
	}

	return Content::labelFromString(r);
}

static Pair<Content::String, Content::String> HtmlProcessor_decodeHeaderLabel(const StringView &source, token *t) {
	if (auto temp_token = manual_label_from_header(t, source.data())) {
		StringView tokenView(&source[temp_token->start], temp_token->len);
		if (tokenView.is('[')) { ++ tokenView; }
		if (tokenView.back() == ']') { tokenView = StringView(tokenView.data(), tokenView.size() - 1); }

		StringView r(tokenView);
		StringView first, second;
		first = r.readChars<
				StringView::CharGroup<CharGroupId::Alphanumeric>,
				StringView::Chars<'.', '_', ':', '-', ' '>,
				stappler::chars::UniChar>();
		first.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (r.is<StringView::Chars<','>>()) {
			++ r;
			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

			second = r;
		} else {
			first = tokenView;
		}

		if (second.empty()) {
			if (first.front() == '.') {
				++ first;
				return pair(HtmlProcessor_decodeHeaderIdLabel(source, t, true), Content::String(first.data(), first.data()));
			} else {
				return pair(Content::labelFromString(first), Content::String());
			}
		} else {
			if (first.front() == '.') {
				return pair(Content::labelFromString(second), Content::String(first.data(), first.data()));
			} else {
				return pair(Content::labelFromString(first), Content::String(second.data(), second.data()));
			}
		}
	} else {
		return pair(HtmlProcessor_decodeHeaderIdLabel(source, t, false), Content::String());
	}
}

void HtmlProcessor::exportHeader(std::ostream &out, token *t) {
	pad(out, 2);
	auto h_idx = t->type - BLOCK_H1  + base_header_level;

	String head = Traits::toString("h", h_idx);

	if ((extensions & Extensions::NoLabels) != Extensions::None) {
		pushNode(head);
	} else {
		auto idClassPair = HtmlProcessor_decodeHeaderLabel(source, t);
		if (idClassPair.second.empty()) {
			pushNode(head, { pair("id", StringView(idClassPair.first)) });
		} else {
			pushNode(head, { pair("id", StringView(idClassPair.first)), pair("class", StringView(idClassPair.second)) });
		}
	}

	exportTokenTree(out, t->child);
	popNode();
	padded = 0;
}

void HtmlProcessor::exportHr(std::ostream &out) {
	pad(out, 2);
	pushInlineNode("hr");
	padded = 0;
}

void HtmlProcessor::exportHtml(std::ostream &out, token *t) {
	pad(out, 2);
	pushHtmlEntity(out, t);
	padded = 1;
}

void HtmlProcessor::exportListBulleted(std::ostream &out, token *t) {
	auto temp_short = list_is_tight;

	switch (t->type) {
		case BLOCK_LIST_BULLETED_LOOSE:
			list_is_tight = false;
			break;

		case BLOCK_LIST_BULLETED:
			list_is_tight = true;
			break;
	}

	pad(out, 2);
	pushNode("ul");
	padded = 0;
	exportTokenTree(out, t->child);
	pad(out, 1);
	popNode();
	padded = 0;
	list_is_tight = temp_short;
}

void HtmlProcessor::exportListEnumerated(std::ostream &out, token *t) {
	auto temp_short = list_is_tight;

	switch (t->type) {
		case BLOCK_LIST_ENUMERATED_LOOSE:
			list_is_tight = false;
			break;

		case BLOCK_LIST_ENUMERATED:
			list_is_tight = true;
			break;
	}

	pad(out, 2);
	pushNode("ol");
	padded = 0;
	exportTokenTree(out, t->child);
	pad(out, 1);
	popNode();
	padded = 0;
	list_is_tight = temp_short;
}

void HtmlProcessor::exportListItem(std::ostream &out, token *t, bool tight) {
	pad(out, 1);
	pushNode("li");
	if (tight) {
		if (!list_is_tight) {
			pushNode("p");
		}

		padded = 2;
		exportTokenTree(out, t->child);

		if (close_para) {
			if (!list_is_tight) {
				popNode();
			}
		} else {
			close_para = true;
		}

	} else {
		padded = 2;
		exportTokenTree(out, t->child);
	}

	popNode();
	padded = 0;
}

void HtmlProcessor::exportDefinitionBlock(std::ostream &out, token *t) {
	pad(out, 2);

	bool open_para = !list_is_tight;
	if (open_para && t->child) {
		switch (t->child->type) {
		case PAIR_BRACKET_IMAGE:
			open_para = !isImageFigure(t->child);
			if (!open_para) {
				close_para = false;
			}
			break;
		default:
			break;
		}
	}

	if (open_para) {
		pushNode("p");
	}

	exportTokenTree(out, t->child);

	if (citation_being_printed) {
		footnote_para_counter--;

		if (footnote_para_counter == 0) {
			out << " ";
			String ref = Traits::toString("#cnref_", citation_being_printed);
			if (!spExt) {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reversecitation") });
			} else {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reversecitation"), pair("target", "_self") });
			}
			out << "&#160;&#8617;";
			popNode();
		}
	}

	if (footnote_being_printed) {
		footnote_para_counter--;

		if (footnote_para_counter == 0) {
			out << " ";
			auto temp_short = footnote_being_printed;

			if ((extensions & Extensions::RandomFoot) != Extensions::None) {
				srand(random_seed_base + temp_short);
				temp_short = rand() % 32000 + 1;
			}

			String ref = Traits::toString("#fnref_", temp_short);
			if (!spExt) {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reversefootnote") });
			} else {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reversefootnote"), pair("target", "_self") });
			}
			out << "&#160;&#8617;";
			popNode();
		}
	}

	if (glossary_being_printed) {
		footnote_para_counter--;

		if (footnote_para_counter == 0) {
			out << " ";
			String ref = Traits::toString("#gnref_", glossary_being_printed);
			if (!spExt) {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reverseglossary") });
			} else {
				pushNode("a", { pair("href", ref), pair("title", localize("return")), pair("class", "reverseglossary"), pair("target", "_self") });
			}
			out << "&#160;&#8617;";
			popNode();
		}
	}

	if (close_para) {
		if (!list_is_tight) {
			popNode();
		}
	} else {
		close_para = true;
	}

	padded = 0;
}

void HtmlProcessor::exportHeaderText(std::ostream &out, token *t, uint8_t level) {
	pad(out, 2);
	auto temp_short = level;

	String head = Traits::toString("h", (temp_short + base_header_level - 1));

	if ((extensions & Extensions::NoLabels) != Extensions::None) {
		pushNode(head);
	} else {
		auto idClassPair = HtmlProcessor_decodeHeaderLabel(source, t);
		if (idClassPair.second.empty()) {
			pushNode(head, { pair("id", StringView(idClassPair.first)) });
		} else {
			pushNode(head, { pair("id", StringView(idClassPair.first)), pair("class", StringView(idClassPair.second)) });
		}
	}

	exportTokenTree(out, t->child);
	popNode();
	padded = 0;
}

void HtmlProcessor::exportTable(std::ostream &out, token *t) {
	pad(out, 2);

	pushNode("table");
	if (!spExt) { out << "\n"; }

	int16_t temp_short;

	// Are we followed by a caption?
	if (table_has_caption(t)) {
		auto temp_token = t->next->child;
		if (temp_token->next && temp_token->next->type == PAIR_BRACKET) {
			temp_token = temp_token->next;
		}
		auto id = label_from_token(source, temp_token);
		pushNode("caption", { pair("style", "caption-side: bottom;"), pair("id", id) });
		t->next->child->child->type = TEXT_EMPTY;
		t->next->child->child->mate->type = TEXT_EMPTY;
		exportTokenTree(out, t->next->child->child);
		popNode();
		if (!spExt) { out << "\n"; }
		temp_short = 1;
	} else {
		temp_short = 0;
	}

	padded = 2;
	readTableColumnAlignments(t);

	pushNode("colgroup");
	if (!spExt) { out << "\n"; }

	for (int i = 0; i < table_column_count; ++i) {
		switch (table_alignment[i]) {
			case 'l': pushInlineNode("col", { pair("style", "text-align:left;") }); if (!spExt) { out << "\n"; } break;
			case 'N':
			case 'L': pushInlineNode("col", { pair("style", "text-align:left;"), pair("class", "extended") }); if (!spExt) { out << "\n"; } break;
			case 'r': pushInlineNode("col", { pair("style", "text-align:right;") }); if (!spExt) { out << "\n"; } break;
			case 'R': pushInlineNode("col", { pair("style", "text-align:right;"), pair("class", "extended") }); if (!spExt) { out << "\n"; } break;
			case 'c': pushInlineNode("col", { pair("style", "text-align:center;") }); if (!spExt) { out << "\n"; } break;
			case 'C': pushInlineNode("col", { pair("style", "text-align:center;"), pair("class", "extended") }); if (!spExt) { out << "\n"; } break;
			default: pushInlineNode("col"); if (!spExt) { out << "\n"; } break;
		}
	}

	popNode();
	if (!spExt) { out << "\n"; }
	padded = 1;
	exportTokenTree(out, t->child);
	pad(out, 1);
	popNode();
	padded = 0;
	skip_token = temp_short;
}

void HtmlProcessor::exportTableHeader(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("thead");
	if (!spExt) { out << "\n"; }
	in_table_header = 1;
	exportTokenTree(out, t->child);
	in_table_header = 0;
	popNode();
	if (!spExt) { out << "\n"; }
	padded = 1;
}

void HtmlProcessor::exportTableSection(std::ostream &out, token *t) {
	pad(out, 2);
	pushNode("tbody");
	if (!spExt) { out << "\n"; }
	padded = 2;
	exportTokenTree(out, t->child);
	popNode();
	padded = 0;
}

static StringView getTableAlignmentStyle(uint8_t a) {
	switch (a) {
	case 'l':
	case 'L':
		return StringView("text-align:left;");
		break;

	case 'r':
	case 'R':
		return StringView("text-align:right;");
		break;

	case 'c':
	case 'C':
		return StringView("text-align:center;");
		break;
	}
	return StringView();
}

void HtmlProcessor::exportTableCell(std::ostream &out, token *t) {
	out << "\t";
	if (t->next && t->next->type == TABLE_DIVIDER && t->next->len > 1) {
		String colspan = Traits::toString(t->next->len);
		switch (table_alignment[table_cell_count]) {
		case 'l': case 'L': case 'r': case 'R': case 'c': case 'C':
			pushNode(in_table_header ? "th" : "td", {
				pair("style", getTableAlignmentStyle(table_alignment[table_cell_count])),
				pair("colspan", colspan)
			});
			break;
		default:
			pushNode(in_table_header ? "th" : "td", {
				pair("colspan", colspan)
			});
			break;
		}
	} else {
		switch (table_alignment[table_cell_count]) {
		case 'l': case 'L': case 'r': case 'R': case 'c': case 'C':
			pushNode(in_table_header ? "th" : "td", {
				pair("style", getTableAlignmentStyle(table_alignment[table_cell_count]))
			});
			break;
		default:
			pushNode(in_table_header ? "th" : "td");
			break;
		}
	}

	exportTokenTree(out, t->child);

	popNode();
	if (!spExt) { out << "\n"; }

	if (t->next) {
		table_cell_count += t->next->len;
	} else {
		table_cell_count++;
	}
}

void HtmlProcessor::exportTableRow(std::ostream &out, token *t) {
	pushNode("tr");
	if (!spExt) { out << "\n"; }
	table_cell_count = 0;
	exportTokenTree(out, t->child);
	popNode();
	if (!spExt) { out << "\n"; }
}

void HtmlProcessor::exportToc(std::ostream &out, token *t) {
	if (!spExt) {
		pad(out, 2);
		pushNode("div", { pair("class", "TOC") });
		if (!spExt) { out << "\n"; }
		size_t counter = 0;
		exportTocEntry(out, counter, 0);
		popNode();
		padded = 0;
	}
}

void HtmlProcessor::exportTocEntry(std::ostream &out, size_t &counter, uint16_t level) {
	token * entry, * next;
	short entry_level, next_level;

	if (!spExt) { out << "\n"; }
	pushNode("ul");
	if (!spExt) { out << "\n"; }

	auto &header_stack = content->getHeaders();
	while (counter < header_stack.size()) {
		entry = header_stack.at(counter);
		entry_level = rawLevelForHeader(entry);

		if (entry_level >= level) {
			// This entry is a direct descendant of the parent
			auto idClassPair = HtmlProcessor_decodeHeaderLabel(source, entry);
			auto ref = Traits::toString("#", idClassPair.first);
			pushNode("li");
			pushNode("a", { pair("href", ref) });
			exportTokenTree(out, entry->child);
			popNode();

			if (counter < header_stack.size() - 1) {
				next = header_stack.at(counter + 1);
				next_level = next->type - BLOCK_H1 + 1;

				if (next_level > entry_level) {
					// This entry has children
					++ counter;
					exportTocEntry(out, counter, entry_level + 1);
				}
			}

			popNode();
			if (!spExt) { out << "\n"; }
		} else if (entry_level < level ) {
			// If entry < level, exit this level
			// Decrement counter first, so that we can test it again later
			-- counter;
			break;
		}

		// Increment counter
		++ counter;
	}

	popNode();
	if (!spExt) { out << "\n"; }
}

void HtmlProcessor::exportBacktick(std::ostream &out, token *t) {
	if (t->mate == NULL) {
		out << printToken(source, t);
	} else if (t->mate->type == QUOTE_RIGHT_ALT)
		if ((extensions & Extensions::Smart) == Extensions::None) {
			out << printToken(source, t);
		} else {
			printLocalizedChar(out, QUOTE_LEFT_DOUBLE);
		} else if (t->start < t->mate->start) {
			pushNode("code");
	} else {
		popNode();
	}
}

void HtmlProcessor::exportPairBacktick(std::ostream &out, token *t) {
	// Strip leading whitespace
	switch (t->child->next->type) {
		case TEXT_NL:
		case INDENT_TAB:
		case INDENT_SPACE:
		case NON_INDENT_SPACE:
			t->child->next->type = TEXT_EMPTY;
			break;

		case TEXT_PLAIN:
			while (t->child->next->len && chars::isWhitespace(source[t->child->next->start])) {
				t->child->next->start++;
				t->child->next->len--;
			}

			break;
	}

	// Strip trailing whitespace
	switch (t->child->mate->prev->type) {
		case TEXT_NL:
		case INDENT_TAB:
		case INDENT_SPACE:
		case NON_INDENT_SPACE:
			t->child->mate->prev->type = TEXT_EMPTY;
			break;

		case TEXT_PLAIN:
			while (t->child->mate->prev->len && chars::isWhitespace(source[t->child->mate->prev->start + t->child->mate->prev->len - 1])) {
				t->child->mate->prev->len--;
			}

			break;
	}

	t->child->type = TEXT_EMPTY;
	t->child->mate->type = TEXT_EMPTY;

	if (t->next && t->next->type == PAIR_RAW_FILTER) {
		// Raw text?
		if (rawFilterMatches(t->next, source, "html")) {
			out << StringView(&(source[t->child->start + t->child->len]), t->child->mate->start - t->child->start - t->child->len);
		}

		// Skip over PAIR_RAW_FILTER
		skip_token = 1;
		return;
	}

	pushNode("code");
	exportTokenTreeRaw(out, t->child);
	popNode();
}

void HtmlProcessor::exportPairAngle(std::ostream &out, token *t) {
	auto temp_char = url_accept(source.data(), t->start + 1, t->len - 2, NULL, true);

	if (!temp_char.empty()) {
		flushBuffer();

		if (scan_email(temp_char.data())) {
			if (strncmp("mailto:", temp_char.data(), 7) != 0) {
				printHtml(buffer, "mailto:");
			}
		}

		printHtml(buffer, temp_char);
		auto ref = buffer.str();
		buffer.clear();
		pushNode("a", { pair("href", ref) });
		printHtml(out, temp_char);
		popNode();
	} else if (scan_html(&source[t->start])) {
		pushHtmlEntity(out, t);
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportPairBracketImage(std::ostream &out, token *t) {
	int16_t temp_short = 0;
	Content::Link *temp_link = parseBrackets(t, &temp_short);

	if (temp_link) {
		if (t->type == PAIR_BRACKET) {
			// Link
			exportLink(out, t, temp_link);
		} else {
			exportImage(out, t, temp_link, isImageFigure(t));
		}

		skip_token = temp_short;
		return;
	}

	// No links exist, so treat as normal
	exportTokenTree(out, t->child);
}

void HtmlProcessor::exportPairBracketAbbreviation(std::ostream &out, token *t) {
	// Which might also be an "auto-tagged" abbreviation
	if ((extensions & Extensions::Notes) != Extensions::None) {
		// Note-based syntax enabled

		// Classify this use

		auto temp_short2 = used_abbreviations.size();
		auto temp_note = parseAbbrBracket(t);

		if (!temp_note) {
			// This instance is not properly formed
			out << "[>";
			exportTokenTree(out, t->child->next);
			out << "]";
			return;
		}

		if (t->child) {
			t->child->type = TEXT_EMPTY;
			t->child->mate->type = TEXT_EMPTY;
		}

		if (temp_note->reference) {
			// This is a reference definition

			if (temp_short2 == used_abbreviations.size()) {
				flushBuffer();
				printHtml(buffer, temp_note->clean_text);
				auto title = buffer.str();
				buffer.clear();
				pushNode("abbr", { pair("title", title) });

				if (t->child) {
					exportTokenTree(out, t->child);
				} else {
					out << printToken(source, t);
				}

				popNode();
			} else {
				// This is the first time this note was used
				printHtml(out, temp_note->clean_text);
				out << " (";

				flushBuffer();
				printHtml(buffer, temp_note->clean_text);
				auto title = buffer.str();
				buffer.clear();
				pushNode("abbr", { pair("title", title) });

				if (t->child) {
					exportTokenTree(out, t->child);
				} else {
					out << printToken(source, t);
				}

				popNode();
				out << ")";
			}
		} else {
			// This is an inline definition (and therefore the first use)
			printHtml(out, temp_note->clean_text);
			out << " (";

			flushBuffer();
			printHtml(buffer, temp_note->clean_text);
			auto title = buffer.str();
			buffer.clear();
			pushNode("abbr", { pair("title", title) });

			printHtml(out, temp_note->label_text);
			popNode();
			out << ")";
		}
	} else {
		// Note-based syntax disabled
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportPairBracketCitation(std::ostream &out, token *t) {
	auto temp_bool = true;		// Track whether this is regular vs 'not cited'
	auto temp_token = t;			// Remember whether we need to skip ahead

	if ((extensions & Extensions::Notes) != Extensions::None) {
		// Note-based syntax enabled
		StringView temp_char;
		Content::String temp_char2;
		if (t->type == PAIR_BRACKET) {
			// This is a locator for a subsequent citation (e.g. `[foo][#bar]`)
			temp_char = text_inside_pair(source, t);
			temp_char2 = Content::labelFromString(temp_char);

			if (temp_char2 == "notcited") {
				temp_char = StringView();
				temp_bool = false;
			}

			// Move ahead to actual citation
			t = t->next;
		} else {
			// This is the actual citation (e.g. `[#foo]`)
			// No locator
			temp_char = StringView();
		}

		// Classify this use
		auto temp_short2 = used_citations.size();
		auto temp_note = parseCitationBracket(t);
		auto temp_short = temp_note->count;

		if (!temp_note) {
			// This instance is not properly formed
			out << "[#";
			exportTokenTree(out, t->child->next);
			out << "]";
			return;
		}

		if (temp_bool) {
			// This is a regular citation

			String ref = Traits::toString("#cn_", temp_short);
			String id = Traits::toString("cnref_", temp_short);

			if (temp_short2 == used_citations.size()) {
				pushNode("a", { pair("href", ref), pair("title", localize("see citation")), pair("class", "citation") });
			} else {
				pushNode("a", { pair("href", ref), pair("id", id), pair("title", localize("see citation")), pair("class", "citation") });
			}

			switch (quotes_lang) {
			case QuotesLanguage::Russian:
				if (!temp_char.empty()) {
					// No locator
					out << "[" << temp_char << ", "  << temp_short << "]";
				} else {
					out << "[" << temp_short << "]";
				}
				break;
			default:
				if (!temp_char.empty()) {
					// No locator
					out << "(" << temp_char << ", "  << temp_short << ")";
				} else {
					out << "(" << temp_short << ")";
				}
				break;
			}

			popNode();
		} else {
			// This is a "nocite"
		}

		if (temp_token != t) {
			// Skip citation on next pass
			skip_token = 1;
		}
	} else {
		// Note-based syntax disabled
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportPairBracketFootnote(std::ostream &out, token *t) {
	if ((extensions & Extensions::Notes) != Extensions::None) {
		// Note-based syntax enabled

		// Classify this use
		auto temp_short2 = used_footnotes.size();
		auto temp_note = parseFootnoteBracket(t);
		auto temp_short = temp_note->count;
		uint16_t temp_short3 = 0;

		if (!temp_note) {
			// This instance is not properly formed
			out << "[^";
			exportTokenTree(out, t->child->next);
			out << "]";
			return;
		}

		if ((extensions & Extensions::RandomFoot) != Extensions::None) {
			srand(unsigned(random_seed_base + temp_short));
			temp_short3 = rand() % 32000 + 1;
		} else {
			temp_short3 = temp_short;
		}

		String ref = Traits::toString("#fn_", temp_short3);
		if (temp_short2 == used_footnotes.size()) {
			pushNode("a", { pair("href", ref), pair("title", localize("see footnote")), pair("class", "footnote") });
		} else {
			String id = Traits::toString("fnref_", temp_short3);
			pushNode("a", { pair("href", ref), pair("id", id), pair("title", localize("see footnote")), pair("class", "footnote") });
		}
		pushNode("sup");
		out << temp_short;
		popNode();
		popNode();
	} else {
		// Note-based syntax disabled
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportPairBracketGlossary(std::ostream &out, token *t) {
	// Which might also be an "auto-tagged" glossary
	if ((extensions & Extensions::Notes) != Extensions::None) {
		// Note-based syntax enabled

		// Classify this use
		auto temp_short2 = used_glossaries.size();
		auto temp_note = parseGlossaryBracket(t);
		auto temp_short = temp_note->count;

		if (!temp_note) {
			// This instance is not properly formed
			out << "[?";
			if (t->child) {
				exportTokenTree(out, t->child->next);
			} else {
				out << printToken(source, t);
			}
			out << "]";
			return;
		}

		String ref = Traits::toString("#gn_", temp_short);
		if (temp_short2 == used_glossaries.size()) {
			pushNode("a", { pair("href", ref), pair("title", localize("see glossary")), pair("class", "glossary") });
		} else {
			String id = Traits::toString("gnref_", temp_short);
			pushNode("a", { pair("href", ref), pair("id", id), pair("title", localize("see glossary")), pair("class", "glossary") });
		}
		printHtml(out, temp_note->clean_text);
		popNode();
	} else {
		// Note-based syntax disabled
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportPairBracketVariable(std::ostream &out, token *t) {
	auto temp_char = text_inside_pair(source, t);
	if (temp_char.is('%')) {
		++ temp_char;
	}
	auto temp_char2 = content->getMeta(temp_char);

	if (!temp_char2.empty()) {
		printHtml(out, temp_char2);
	} else {
		if (meta_callback) {
			temp_char2 = meta_callback(temp_char);
			if (!temp_char2.empty()) {
				printHtml(out, temp_char2);
				return;
			}
		}

		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticAdd(std::ostream &out, token *t) {
	// Ignore if we're rejecting
	if ((extensions & Extensions::CriticReject) != Extensions::None) {
		return;
	}

	if ((extensions & Extensions::Critic) != Extensions::None) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;

		if ((extensions & Extensions::CriticAccept) != Extensions::None) {
			exportTokenTree(out, t->child);
		} else {
			pushNode("ins");
			exportTokenTree(out, t->child);
			popNode();
		}
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticDel(std::ostream &out, token *t) {
	// Ignore if we're accepting
	if ((extensions & Extensions::CriticAccept) != Extensions::None) {
		return;
	}

	if ((extensions & Extensions::Critic) != Extensions::None) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;

		if ((extensions & Extensions::CriticReject) != Extensions::None) {
			exportTokenTree(out, t->child);
		} else {
			pushNode("del");
			exportTokenTree(out, t->child);
			popNode();
		}
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticCom(std::ostream &out, token *t) {
	// Ignore if we're rejecting or accepting
	if ((extensions & Extensions::CriticAccept) != Extensions::None ||
			(extensions & Extensions::CriticReject) != Extensions::None) {
		return;
	}

	if ((extensions & Extensions::Critic) != Extensions::None) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;

		pushNode("span", { pair("class", "critic comment") });
		exportTokenTree(out, t->child);
		popNode();
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticHi(std::ostream &out, token *t) {
	// Ignore if we're rejecting or accepting
	if ((extensions & Extensions::CriticAccept) != Extensions::None ||
			(extensions & Extensions::CriticReject) != Extensions::None) {
		return;
	}

	if ((extensions & Extensions::Critic) != Extensions::None) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;
		pushNode("mark");
		exportTokenTree(out, t->child);
		popNode();
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticPairSubDel(std::ostream &out, token *t) {
	if ((extensions & Extensions::Critic) != Extensions::None &&
	        (t->next) && (t->next->type == PAIR_CRITIC_SUB_ADD)) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;

		if ((extensions & Extensions::CriticAccept) != Extensions::None) {

		} else if ((extensions & Extensions::CriticReject) != Extensions::None) {
			exportTokenTree(out, t->child);
		} else {
			pushNode("del");
			exportTokenTree(out, t->child);
			popNode();
		}
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportCriticPairSubAdd(std::ostream &out, token *t) {
	if ((extensions & Extensions::Critic) != Extensions::None &&
	        (t->prev) && (t->prev->type == PAIR_CRITIC_SUB_DEL)) {
		t->child->type = TEXT_EMPTY;
		t->child->mate->type = TEXT_EMPTY;

		if ((extensions & Extensions::CriticReject) != Extensions::None) {

		} else if ((extensions & Extensions::CriticAccept) != Extensions::None) {
			exportTokenTree(out, t->child);
		} else {
			pushNode("ins");
			exportTokenTree(out, t->child);
			popNode();
		}
	} else {
		exportTokenTree(out, t->child);
	}
}

void HtmlProcessor::exportMath(std::ostream &out, token *t) {
	pushNode("span", { pair("class", "math") });
	exportTokenTreeMath(out, t->child);
	popNode();
}

void HtmlProcessor::exportSubscript(std::ostream &out, token *t) {
	if (t->mate) {
		((t->start < t->mate->start) ? pushNode("sub") : popNode());
	} else if (t->len != 1) {
		pushNode("sub");
		exportTokenTree(out, t->child);
		popNode();
	} else {
		out << "~";
	}
}

void HtmlProcessor::exportSuperscript(std::ostream &out, token *t) {
	if (t->mate) {
		((t->start < t->mate->start) ? pushNode("sup") : popNode());
	} else if (t->len != 1) {
		pushNode("sup");
		exportTokenTree(out, t->child);
		popNode();
	} else {
		out << "^";
	}
}

void HtmlProcessor::exportLineBreak(std::ostream &out, token *t) {
	if (t->next) {
		pushInlineNode("br");
		if (!spExt) { out << "\n"; }
		padded = 1;
	}
}

NS_MMD_END

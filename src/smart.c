#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "cmark.h"
#include "node.h"
#include "utf8.h"
#include "buffer.h"
#include "chunk.h"

void escape_with_smart(cmark_strbuf *buf,
		       cmark_node *node,
		       void (*escape)(cmark_strbuf *, const unsigned char *, int),
		       const char *left_double_quote,
		       const char *right_double_quote,
		       const char *left_single_quote,
		       const char *right_single_quote,
		       const char *em_dash,
		       const char *en_dash,
		       const char *ellipses)
{
	int32_t c = 0;
	int32_t after_char = 0;
	int32_t before_char = 0;
	int len;
	bool left_flanking, right_flanking;
	int lastout = 0;
	int i = 0;
	cmark_chunk lit = node->as.literal;

	// set before_char based on previous text node if there is one:
	if (node->prev) {
		if (node->prev->type == CMARK_NODE_TEXT) {

			// walk back to the beginning of the UTF_8 sequence:
			i = node->prev->as.literal.len - 1;
			while (i > 0 && node->prev->as.literal.data[i] >> 6 == 2) {
				i -= 1;
			}
			len = utf8proc_iterate(node->prev->as.literal.data + i,
					       node->prev->as.literal.len - i,
					       &before_char);
			if (len == -1) {
				before_char = 10;
			}

		} else if (node->prev->type == CMARK_NODE_SOFTBREAK ||
			   node->prev->type == CMARK_NODE_LINEBREAK) {
			before_char = 10;

		} else {
			before_char = 65;
		}
	} else {
		before_char = 10;
	}

	while (i < lit.len) {
		len = utf8proc_iterate(lit.data + i, lit.len - i, &c);
		i += len;

		// replace with efficient lookup table:
		if (!(c == 34 || c == 39 || c == 45 || c == 46)) {
			before_char = c;
			continue;
		}
		(*escape)(buf, lit.data + lastout, i - len - lastout);

		if (c == 34 || c == 39) {

			if (i >= lit.len) {
				if (node->next) {
					if (node->next->type == CMARK_NODE_TEXT) {
						utf8proc_iterate(node->next->as.literal.data,
								 node->next->as.literal.len,
								 &after_char);
					} else if (node->next->type == CMARK_NODE_SOFTBREAK ||
						   node->next->type == CMARK_NODE_LINEBREAK) {
						after_char = 10;
					} else {
						after_char = 65;
					}
				} else {
					after_char = 10;
				}
			} else {
				utf8proc_iterate(lit.data + i, lit.len - i, &after_char);
			}

			left_flanking = !utf8proc_is_space(after_char) &&
				!(utf8proc_is_punctuation(after_char) &&
				  !utf8proc_is_space(before_char) &&
				  !utf8proc_is_punctuation(before_char));
			right_flanking = !utf8proc_is_space(before_char) &&
				!(utf8proc_is_punctuation(before_char) &&
				  !utf8proc_is_space(after_char) &&
				  !utf8proc_is_punctuation(after_char));
		}

		switch (c) {
		case 34: // "
			if (right_flanking) {
				cmark_strbuf_puts(buf, right_double_quote);
			} else {
				cmark_strbuf_puts(buf, left_double_quote);
			}
			break;
		case 39: // '
			if (left_flanking && !right_flanking) {
				cmark_strbuf_puts(buf, left_single_quote);
			} else {
				cmark_strbuf_puts(buf, right_single_quote);
			}
			break;
		case 45: // -
			if (i < lit.len && lit.data[i] == '-') {
				if (lit.data[i + 1] == '-') {
					cmark_strbuf_puts(buf, em_dash);
					i += 2;
				} else {
					cmark_strbuf_puts(buf, en_dash);
					i += 1;
				}
			} else {
				cmark_strbuf_putc(buf, c);
			}
			break;
		case 46: // .
			if (i < lit.len - 1 && lit.data[i] == '.' &&
			    lit.data[i + 1] == '.') {
				cmark_strbuf_puts(buf, ellipses);
				i += 2;
			} else {
				cmark_strbuf_putc(buf, c);
			}
			break;
		default:
			cmark_strbuf_putc(buf, c);
		}
		lastout = i;
	}
	(*escape)(buf, node->as.literal.data + lastout, lit.len - lastout);

}
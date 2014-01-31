#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTML_PARSE_STATE_TYPE struct HtmlParseState

#include "stack.h"
#include "html.h"

#define CASE_SPACE case ' ': case '\r': case '\n': case '\t'

/*keep sorted, binary search*/
const char const *html_tag[HTML_TAGS] = {
	[HTML_TAG_NONE] = "",
	[HTML_TAG_COMMENT] = "!--",
	[HTML_TAG_DOCTYPE] = "!doctype",
	
	[HTML_TAG_A] = "a",
	[HTML_TAG_ABBR] = "abbr",
	[HTML_TAG_ACRONYM] = "acronym",
	[HTML_TAG_ADDRESS] = "address",
	[HTML_TAG_APPLET] = "applet",
	[HTML_TAG_AREA] = "area",
	
	[HTML_TAG_B] = "b",
	[HTML_TAG_BASE] = "base",
	[HTML_TAG_BASEFONT] = "basefont",
	[HTML_TAG_BDO] = "bdo",
	[HTML_TAG_BIG] = "big",
	[HTML_TAG_BLOCKQUOTE] = "blockquote",
	[HTML_TAG_BODY] = "body",
	[HTML_TAG_BR] = "br",
	[HTML_TAG_BUTTON] = "button",
	
	[HTML_TAG_CAPTION] = "caption",
	[HTML_TAG_CENTER] = "center",
	[HTML_TAG_CITE] = "cite",
	[HTML_TAG_CODE] = "code",
	[HTML_TAG_COL] = "col",
	[HTML_TAG_COLGROUP] = "colgroup",
	[HTML_TAG_DD] = "dd",
	[HTML_TAG_DEL] = "del",
	
	[HTML_TAG_DFN] = "dfn",
	[HTML_TAG_DIR] = "dir",
	[HTML_TAG_DIV] = "div",
	[HTML_TAG_DL] = "dl",
	[HTML_TAG_DT] = "dt",
	
	[HTML_TAG_EM] = "em",
	[HTML_TAG_FIELDSET] = "fieldset",
	[HTML_TAG_FONT] = "font",
	[HTML_TAG_FORM] = "form",
	[HTML_TAG_FRAME] = "frame",
	[HTML_TAG_FRAMESET] = "frameset",
	
	[HTML_TAG_H1] = "h1",
	[HTML_TAG_H2] = "h2",
	[HTML_TAG_H3] = "h3",
	[HTML_TAG_H4] = "h4",
	[HTML_TAG_H5] = "h5",
	[HTML_TAG_H6] = "h6",
	[HTML_TAG_HEAD] = "head",
	[HTML_TAG_HR] = "hr",
	[HTML_TAG_HTML] = "html",
	
	[HTML_TAG_I] = "i",
	[HTML_TAG_IFRAME] = "iframe",
	[HTML_TAG_IMG] = "img",
	[HTML_TAG_INPUT] = "input",
	[HTML_TAG_INS] = "ins",
	
	[HTML_TAG_KBD] = "kbd",
	
	[HTML_TAG_LABEL] = "label",
	[HTML_TAG_LEGEND] = "legend",
	[HTML_TAG_LI] = "li",
	[HTML_TAG_LINK] = "link",
	
	[HTML_TAG_MAP] = "map",
	[HTML_TAG_MENU] = "menu",
	[HTML_TAG_META] = "meta",
	
	[HTML_TAG_NOFRAMES] = "noframes",
	[HTML_TAG_NOSCRIPT] = "noscript",
	
	[HTML_TAG_OBJECT] = "object",
	[HTML_TAG_OL] = "ol",
	[HTML_TAG_OPTGROUP] = "optgroup",
	[HTML_TAG_OPTION] = "option",
	
	[HTML_TAG_P] = "p",
	[HTML_TAG_PARAM] = "param",
	[HTML_TAG_PRE] = "pre",
	
	[HTML_TAG_Q] = "q",
	
	[HTML_TAG_S] = "s",
	[HTML_TAG_SAMP] = "samp",
	[HTML_TAG_SCRIPT] = "script",
	[HTML_TAG_SELECT] = "select",
	[HTML_TAG_SMALL] = "small",
	[HTML_TAG_SPAN] = "span",
	[HTML_TAG_STRIKE] = "strike",
	[HTML_TAG_STRONG] = "strong",
	[HTML_TAG_STYLE] = "style",
	[HTML_TAG_SUB] = "sub",
	[HTML_TAG_SUP] = "sup",
	
	[HTML_TAG_TABLE] = "table",
	[HTML_TAG_TBODY] = "tbody",
	[HTML_TAG_TD] = "td",
	[HTML_TAG_TEXTAREA] = "textarea",
	[HTML_TAG_TFOOT] = "tfoot",
	[HTML_TAG_TH] = "th",
	[HTML_TAG_THEAD] = "thead",
	[HTML_TAG_TITLE] = "title",
	[HTML_TAG_TR] = "tr",
	[HTML_TAG_TT] = "tt",
	
	[HTML_TAG_U] = "u",
	[HTML_TAG_UL] = "ul",
	
	[HTML_TAG_VAR] = "var",
};

enum State {
	STATE_CHILD,
	STATE_OPEN,
	STATE_DECLARATION,
	STATE_BEGIN,
	STATE_END,
	STATE_ATTRIB,
	STATE_ATTRIB_KEY,
	STATE_ATTRIB_VALUE,
	STATE_ATTRIB_QUOTEVALUE,
	STATE_CLOSE,
	STATE_SELFCLOSE,
	STATE_END_CLOSE,
	STATE_ENTITY,
	
	/*This is silly*/
	STATE_COMMENT_BEGIN,
	STATE_COMMENT,
	STATE_COMMENT_END1,
	STATE_COMMENT_END2,
	
	STATES,
};

struct HtmlParseState {
	HtmlDocument *document;
	Stack *stack;
	HtmlElement *elem;
	HtmlTag tag;
	enum State state;
};

static int findtag(void *elem, void *tag) {
	if(((HtmlElement *) elem)->tag == *((int *) tag))
		return 1;
	return 0;
}

static int stringcompare_tag(const char *s1, const char *s2, size_t length) {
	int diff, i;
	for(i = 0; ; i++) {
		if(i >= length) {
			if(!s2[i])
				return 0;
			return -1;
		}
		diff = tolower(s1[i]) - tolower(s2[i]);
		if(diff)
			return diff;
		if(!s1[i]) {
			if(!s2[i])
				return 0;
			else
				return -1;
		}
		if(!s2[i]) {
			if(!s1[i])
				return 0;
			else
				return 1;
		}
	}
}

int html_tag_is_script(HtmlTag tag) {
	switch(tag) {
		case HTML_TAG_SCRIPT:
		case HTML_TAG_STYLE:
			return 1;
		default:
			return 0;
	}
}

int html_tag_is_selfclose(HtmlTag tag) {
	switch(tag) {
		case HTML_TAG_BASE:
		case HTML_TAG_BASEFONT:
		case HTML_TAG_FRAME:
		case HTML_TAG_LINK:
		case HTML_TAG_META:
		case HTML_TAG_AREA:
		case HTML_TAG_BR:
		case HTML_TAG_COL:
		case HTML_TAG_HR:
		case HTML_TAG_IMG:
		case HTML_TAG_INPUT:
		case HTML_TAG_PARAM:
			return 1;
		default:
			return 0;
	}
}

HtmlTag html_lookup_length_tag(const char *string, size_t length) {
	//TODO: optimize string compare for binary tree search (no "restarts")
	int i, imin = 0, imax = HTML_TAGS, res;
	
	while(imax >= imin) {
		i = (imax - imin)/2 + imin;
		res = stringcompare_tag(string, html_tag[i], length);
		if(res < 0)
			imax = i - 1;
		else if(res > 0)
			imin = i + 1;
		else
			return i;
	}
	
	return 0;
}

HtmlTag html_lookup_tag(const char *string) {
	int i, imin = 0, imax = HTML_TAGS, res;
	
	while(imax >= imin) {
		i = (imax - imin)/2 + imin;
		res = stringcompare_tag(string, html_tag[i], ~0);
		if(res < 0)
			imax = i - 1;
		else if(res > 0)
			imin = i + 1;
		else
			return i;
	}
	
	return 0;
}

HtmlElement *html_new_element(HtmlTag tag, HtmlAttrib *attrib, HtmlElement *child, HtmlElement *sibbling) {
	HtmlElement *elem;
	if(!(elem = malloc(sizeof(HtmlElement))))
		return NULL;
	
	elem->tag = tag;
	elem->attrib = attrib;
	elem->child = child;
	elem->sibbling = sibbling;
	
	return elem;
}

HtmlParseState *html_parse_begin() {
	HtmlParseState *state;
	if(!(state = malloc(sizeof(HtmlParseState))))
		return NULL;
	
	state->document = NULL;
	state->stack = NULL;
	state->elem = NULL;
	state->tag = 0;
	state->state = STATE_CHILD;
	
	if(!(state->document = malloc(sizeof(HtmlDocument))))
		goto error;
	if(!(state->elem = html_new_element(HTML_TAG_NONE, NULL, NULL, NULL)))
		goto error;
	if(!stack_push(&state->stack, state->elem))
		goto error;
	
	state->document->root_element = state->elem;
	state->elem = NULL;
	
	return state;
	
	error:
	free(state->document);
	free(state->elem);
	while(stack_pop(&state->stack));
	free(state);
	
	return NULL;
}

const char *html_parse_stream(HtmlParseState *state, const char *stream, const char *token, size_t len) {
	//TODO: support text content
	//TODO: support attributes (with and without quotes)
	//TODO: support entities
	//TODO: support more xml bullcrap, like CDATA
	//TODO: handle unknown html elements
	
	char c;
	HtmlElement *elem_tmp;
	size_t i;
	
	if(!(state && stream && len))
		return NULL;
	
	for(i = 0; i < len; i++) {
		c = *stream++;
		reswitch:
		switch(state->state) {
			case STATE_CHILD:
				switch(c) {
					case '<':
						//add containing text
						token = stream;
						state->state = STATE_OPEN;
						//tag = 0;
						continue;
					default:
						if(html_tag_is_script(state->tag))
							token = stream;
						continue;
				}
			case STATE_OPEN:
				if(html_tag_is_script(state->tag)) {
					/*I hate script tags*/
					if(c != '/') {
						token = stream;
						state->state = STATE_CHILD;
						continue;
					}
				}
				switch(c) {
					CASE_SPACE:
						token = stream;
						/*testing this*/
						//state = STATE_CHILD;
						continue;
					
					/*Comments, doctypes, xml-stuff and other crap we don't care about*/
					case '!':
						token = stream;
						state->state = STATE_DECLARATION;
						continue;
					case '?':
						token = stream;
						state->state = STATE_END_CLOSE;
						continue;
					
					case '/':
						token = stream;
						state->state = STATE_END;
						continue;
					default:
						state->state = STATE_BEGIN;
						continue;
				}
			case STATE_DECLARATION:
				token = stream;
				switch(c) {
					case '-':
						state->state = STATE_COMMENT_BEGIN;
						continue;
					default:
						state->state = STATE_END_CLOSE;
						continue;
				}
			case STATE_COMMENT_BEGIN:
				token = stream;
				state->state = STATE_COMMENT;
				continue;
			case STATE_COMMENT:
			case STATE_COMMENT_END1:
				token = stream;
				switch(c) {
					case '-':
						state->state++;
						continue;
					default:
						state->state = STATE_COMMENT;
						continue;
				}
			case STATE_COMMENT_END2:
				token = stream;
				switch(c) {
					case '>':
						state->state = STATE_CHILD;
						continue;
					default:
						state->state = STATE_COMMENT;
						continue;
				}
			case STATE_BEGIN:
				switch(c) {
					CASE_SPACE:
						state->tag = html_lookup_length_tag(token, (stream - 1) - token);
						state->state = STATE_ATTRIB;
						token = stream;
						continue;
					case '>':
						state->tag = html_lookup_length_tag(token, (stream - 1) - token);
						state->state = STATE_CLOSE;
						token = stream;
						goto reswitch;
					case '/':
						state->tag = html_lookup_length_tag(token, (stream - 1) - token);
						state->state = STATE_SELFCLOSE;
						token = stream;
						continue;
					default:
						continue;
				}
			case STATE_ATTRIB:
				switch(c) {
					CASE_SPACE:
						token = stream;
						continue;
					case '/':
						state->state = STATE_SELFCLOSE;
						token = stream;
						continue;
					case '>':
						state->state = STATE_CLOSE;
						token = stream;
						goto reswitch;
					default:
						state->state = STATE_ATTRIB_KEY;
						token = stream;
						continue;
				}
			case STATE_ATTRIB_KEY:
				switch(c) {
					CASE_SPACE:
						//key=key add attrib
						state->state = STATE_ATTRIB;
						token = stream;
						continue;
					case '>':
						//add attrib
						state->state = STATE_CLOSE;
						token = stream;
						goto reswitch;
					case '/':
						//add attrib
						state->state = STATE_SELFCLOSE;
						token = stream;
						continue;
					case '=':
						state->state = STATE_ATTRIB_VALUE;
						token = stream;
						continue;
					default:
						continue;
				}
			case STATE_ATTRIB_QUOTEVALUE:
				switch(c) {
					case '"':
						//add attrib
						state->state = STATE_ATTRIB;
						token = stream;
						continue;
					default:
						continue;
				}
			case STATE_ATTRIB_VALUE:
				switch(c) {
					CASE_SPACE:
						//add attrib
						state->state = STATE_ATTRIB;
						token = stream;
						continue;
					case '>':
						//add attrib
						state->state = STATE_CLOSE;
						token = stream;
						goto reswitch;
					case '"':
						state->state = STATE_ATTRIB_QUOTEVALUE;
						token = stream;
					continue;
					case '/':
						//add attrib
						state->state = STATE_SELFCLOSE;
						token = stream;
						continue;
					default:
						continue;
				}
			case STATE_CLOSE:
				switch(c) {
					case '>':
						token = stream;
						if(html_tag_is_script(state->tag)) {
							state->state = STATE_CHILD;
							continue;
						}
						//add to stack
						if(!(elem_tmp = html_new_element(state->tag, NULL, NULL, NULL)))
							goto error;
						if(state->elem) {
							state->elem->sibbling = elem_tmp;
							state->elem = elem_tmp;
						} else {
							state->elem = stack_peek(&state->stack);
							state->elem->child = elem_tmp;
							state->elem = elem_tmp;
						}
						
						if(!html_tag_is_selfclose(state->tag)) {
							stack_push(&state->stack, state->elem);
							state->elem = NULL;
						}
						state->tag = 0;
						
						state->state = STATE_CHILD;
						continue;
					default:
						token = stream;
						continue;
				}
			case STATE_SELFCLOSE:
				switch(c) {
					case '>':
						token = stream;
						if(html_tag_is_script(state->tag)) {
							state->state = STATE_CHILD;
							continue;
						}
						//add to stack
						if(!(elem_tmp = html_new_element(state->tag, NULL, NULL, NULL)))
							goto error;
						if(state->elem) {
							state->elem->sibbling = elem_tmp;
							state->elem = elem_tmp;
						} else {
							state->elem = stack_peek(&state->stack);
							state->elem->child = elem_tmp;
							state->elem = elem_tmp;
						}
						
						state->state = STATE_CHILD;
						continue;
					default:
						token = stream;
						continue;
				}
			case STATE_END:
				switch(c) {
					CASE_SPACE:
						//find tag to close
						if(html_tag_is_script(state->tag)) {
							if((stream - 1) - token > strlen(html_tag[state->tag]))
								state->state = STATE_CHILD;
							else if(html_lookup_length_tag(token, (stream - 1) - token) == state->tag) {
								state->tag = 0;
								state->state = STATE_END_CLOSE;
							} else
								state->state = STATE_CHILD;
							token = stream;
							continue;
						}
						if((state->tag = html_lookup_length_tag(token, (stream - 1) - token)) < 0)
							state->tag = state->elem->tag;
						
						token = stream;
						if(!stack_find(&state->stack, findtag, &state->tag)) {
							state->state = STATE_CHILD;
							continue;
						}
						
						do {
							/*check for null, broken pages*/
							elem_tmp = stack_pop(&state->stack);
						} while(elem_tmp->tag != state->tag);
						
						state->elem = elem_tmp;
						
						state->state = STATE_END_CLOSE;
						continue;
					case '>':
						//find tag to close
						if(html_tag_is_script(state->tag)) {
							if((stream - 1) - token > strlen(html_tag[state->tag]))
								state->state = STATE_CHILD;
							else if(html_lookup_length_tag(token, (stream - 1) - token) == state->tag) {
								state->tag = 0;
								state->state = STATE_CHILD;
							} else
								state->state = STATE_CHILD;
							token = stream;
							continue;
						}
						if((state->tag = html_lookup_length_tag(token, (stream - 1) - token)) < 0)
							state->tag = state->elem->tag;
						
						token = stream;
						if(!stack_find(&state->stack, findtag, &state->tag)) {
							state->state = STATE_CHILD;
							continue;
						}
						do {
							/*check for null, broken pages*/
							elem_tmp = stack_pop(&state->stack);
						} while(elem_tmp->tag != state->tag);
						
						state->elem = elem_tmp;
						state->tag = 0;
						
						state->state = STATE_CHILD;
						continue;
					default:
						continue;
				}
			case STATE_END_CLOSE:
				switch(c) {
					case '>':
						state->state = STATE_CHILD;
						token = stream;
						continue;
					default:
						continue;
				}
			case STATE_ENTITY:
			case STATES:
				break;
		}
	}
	return token;
	
	error:
	/*Handle malloc fail somehow*/
	return NULL;
}

HtmlDocument *html_parse_end(HtmlParseState *state) {
	HtmlDocument *document;
	
	if(!state)
		return NULL;
	
	document = state->document;
	while(stack_pop(&state->stack));
	free(state);
	
	return document;
}

void *html_free_element(HtmlElement *element, int level) {
	int i;
	if(!element)
		return NULL;
	while(element) {
		HtmlElement *sibbling = element->sibbling;
		for(i = 0; i < level; i++)
			printf("\t");
		printf("element %i: %s\n", element->tag, html_tag[element->tag]);
		html_free_element(element->child, level + 1);
		free(element);
		element = sibbling;
	}
	return NULL;
}

void *html_free_document(HtmlDocument *document) {
	if(!document)
		return NULL;
	html_free_element(document->root_element, 0);
	free(document);
	return NULL;
}

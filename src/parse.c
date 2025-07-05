#include "srvsh/parse.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <scallop-lang/classifier.h>
#include <scallop-lang/lex.h>

#include "srvsh/srvsh.h"
#include "srvsh/process.h"

typedef struct libadt_lptr lptr_t;
typedef struct libadt_const_lptr const_lptr_t;
typedef struct scallop_lang_lex token_t;

#define lex_word scallop_lang_classifier_word
#define lex_word_separator scallop_lang_classifier_word_separator
#define lex_unexpected scallop_lang_classifier_unexpected
#define lex_end scallop_lang_classifier_end
#define lex_curly_block scallop_lang_classifier_curly_block
#define lex_curly_block_end scallop_lang_classifier_curly_block_end
#define lex_square_block scallop_lang_classifier_square_block
#define lex_square_block_end scallop_lang_classifier_square_block_end
#define token_next scallop_lang_lex_next

#define lptr_raw libadt_lptr_raw
#define lptr_index libadt_lptr_index
#define const_lptr libadt_const_lptr

#define LPTR_WITH LIBADT_LPTR_WITH

typedef struct word_list_s {
	lptr_t word;
	struct word_list_s *next;
} word_list_t;

static token_t parse_statement_impl(
	token_t token,
	word_list_t *previous,
	int count
);
static token_t parse_script_impl(
	token_t token
);

static token_t parse_statement_impl(
	token_t token,
	word_list_t *previous,
	int count
)
{
	token = token_next(token);

	if (token.type == lex_unexpected) {
		return (token_t){ 0 };
	} else if (token.type == lex_word_separator) {
		return parse_statement_impl(
			token,
			previous,
			count
		);
	} else if (token.type == lex_word) {
		ssize_t size = scallop_lang_lex_normalize_word(
			token.value,
			(lptr_t){ 0 }
		);
		if (size < 0)
			return (token_t){ 0 };

		token_t result = { 0 };
		LPTR_WITH(word, (size_t)size + 1, sizeof(char)) {
			scallop_lang_lex_normalize_word(
				token.value,
				word
			);

			word_list_t current = {
				word,
				previous,
			};
			result = parse_statement_impl(
				token,
				&current,
				++count
			);
		}
		return result;
	} else if (token.type == lex_curly_block) {
		// TODO: don't rely on the script always having
		// a statement separator after a closing curly bracket
		token = parse_script_impl(token);
		if (token.type != lex_curly_block_end)
			return (token_t){ 0 };
		return token;
	} else /* if (token.type == lex_statement_separator) and friends */ {
		int error = -1;
		int cli = socket(AF_UNIX, SOCK_STREAM, 0);
		if (cli < 0)
			return (token_t){ 0 };

		// TODO: depends on Linux-specific auto-binding
		struct sockaddr_un addr = { .sun_family = AF_UNIX };
		if (bind(cli, (struct sockaddr*)&addr, sizeof(addr.sun_family)) < 0) {
			close(cli);
			return (token_t){ 0 };
		}

		LPTR_WITH(statement, (size_t)count, sizeof(lptr_t)) {
			for (--count; count >= 0; --count) {
				const_lptr_t *item = lptr_raw(
					lptr_index(statement, count)
				);
				*item = const_lptr(previous->word);
				previous = previous->next;
			}

			error = fork_wrapper(const_lptr(statement), cli);
		}
		if (error < 0) {
			close(cli);
			return (token_t){ 0 };
		}
		token = parse_script_impl(token);
		close(cli);
		return token;
	}

	return token;
}

static token_t parse_script_impl(token_t token)
{
	token_t next = token_next(token);

	const bool end
		= next.type == lex_end
		|| next.type == lex_curly_block_end
		|| next.type == lex_square_block_end
		|| next.type == lex_unexpected;

	if (end)
		return next;

	if (next.type == lex_word) {
		return parse_statement_impl(next, NULL, 0);
	}

	// A curly bracket block at the top level is identical
	// to no curly bracket block
	if (next.type == lex_curly_block) {
		token_t end_block = parse_script_impl(next);
		if (end_block.type != lex_curly_block_end) {
			end_block.type = lex_unexpected;
			return end_block;
		}
		return parse_script_impl(end_block);
	}

	// skips comments, word/statement separators etc.
	return parse_script_impl(next);
}

int srvsh_parse_script(const_lptr_t script)
{
	token_t last = parse_script_impl(
		scallop_lang_lex_init(script)
	);

	return last.type == lex_end ? 0 : -1;
}

#include "srvsh/parse.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <scallop-lang/token.h>

#include "srvsh/srvsh.h"
#include "srvsh/process.h"

typedef struct libadt_lptr lptr_t;
typedef struct libadt_const_lptr const_lptr_t;
typedef struct scallop_lang_token token_t;

#define lex_word scallop_lang_lex_word
#define lex_word_separator scallop_lang_lex_word_separator
#define lex_unexpected scallop_lang_lex_unexpected
#define lex_end scallop_lang_lex_end
#define lex_curly_block scallop_lang_lex_curly_block
#define lex_curly_block_end scallop_lang_lex_curly_block_end
#define lex_square_block scallop_lang_lex_square_block
#define lex_square_block_end scallop_lang_lex_square_block_end
#define token_next scallop_lang_token_next

#define lptr_raw libadt_lptr_raw
#define lptr_index libadt_lptr_index
#define const_lptr libadt_const_lptr

#define LPTR_WITH LIBADT_LPTR_WITH

typedef struct word_list_s {
	lptr_t word;
	struct word_list_s *next;
} word_list_t;

static void free_word_list(word_list_t *list)
{
	for (; list; list = list->next)
		libadt_lptr_free(list->word);
}

static token_t parse_statement_impl(
	token_t token,
	int srv,
	word_list_t *previous,
	int count
);
static token_t parse_script_impl(
	token_t token,
	int srv
);

static token_t parse_statement_impl(
	token_t token,
	int srv,
	word_list_t *previous,
	int count
)
{
// Not sure if I prefer this over "goto error;"
#define HANDLE_ERROR() do { \
	free_word_list(previous); \
	return (token_t){ 0 }; \
} while (0)

	int cli = CLI_FILENO;

	token = token_next(token);

	if (token.type == lex_unexpected) {
		HANDLE_ERROR();
	} else if (token.type == lex_word_separator) {
		return parse_statement_impl(
			token,
			srv,
			previous,
			count
		);
	} else if (token.type == lex_word) {
		ssize_t size = scallop_lang_token_normalize_word(
			token.value,
			(lptr_t){ 0 }
		);
		if (size < 0)
			HANDLE_ERROR();

		LPTR_WITH(word, (size_t)size + 1, sizeof(char)) {
			scallop_lang_token_normalize_word(
				token.value,
				word
			);

			word_list_t current = {
				word,
				previous,
			};
			return parse_statement_impl(
				token,
				srv,
				&current,
				++count
			);
		}
		HANDLE_ERROR();
	} else if (token.type == lex_curly_block) {
		int sockets[2] = { 0 };
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
			HANDLE_ERROR();

		cli = sockets[0];
		// TODO: don't rely on the script always having
		// a statement separator after a closing curly bracket
		token = parse_script_impl(token, sockets[1]);
		if (token.type != lex_curly_block_end)
			HANDLE_ERROR();
	} else /* if (token.type == lex_statement_separator) and friends */ {
		LPTR_WITH(statement, (size_t)count, sizeof(lptr_t)) {
			for (--count; count >= 0; --count) {
				const_lptr_t *item = lptr_raw(
					lptr_index(statement, count)
				);
				*item = const_lptr(previous->word);
				previous = previous->next;
			}

			if (fork_wrapper(const_lptr(statement), srv, cli) < 0)
				HANDLE_ERROR();
		}
		return token;
	}

	return token;
#undef HANDLE_ERROR
}

static token_t parse_script_impl(token_t token, int srv)
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
		next = parse_statement_impl(token, srv, NULL, 0);
		return parse_script_impl(next, srv);
	}

	// A curly bracket block at the top level is identical
	// to no curly bracket block
	if (next.type == lex_curly_block) {
		token_t end_block = parse_script_impl(next, srv);
		if (end_block.type != lex_curly_block_end) {
			end_block.type = lex_unexpected;
			return end_block;
		}
		return parse_script_impl(end_block, srv);
	}

	// skips comments, word/statement separators etc.
	return parse_script_impl(next, srv);
}

int srvsh_parse_script(const_lptr_t script, int srv)
{
	token_t last = parse_script_impl(
		scallop_lang_token_init(script),
		srv
	);

	return last.type == lex_end ? 0 : -1;
}

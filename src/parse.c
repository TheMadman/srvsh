#include "srvsh/parse.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wait.h>

#include <scallop-lang/classifier.h>
#include <scallop-lang/lex.h>

#include "srvsh/srvsh.h"
#include "srvsh/process.h"

#define _STR(A) #A
#define STR(A) _STR(A)
#define MAX libadt_util_max

// mimicks the behaviour of bash
#define SIGNAL_RETURN_VALUE(sig) (128 + sig)

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

static int get_clients_end()
{
	// probably a better way to do this
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	close(fd);
	return fd;
}

static bool is_end_token(token_t token)
{
	return token.type == lex_end
		|| token.type == lex_curly_block_end
		|| token.type == lex_square_block_end
		|| token.type == lex_unexpected;
}

static int exec_word_list(word_list_t *words, int count)
{
	int error = -1;
	LPTR_WITH(statement, (size_t)count, sizeof(lptr_t)) {
		for (--count; count >= 0; --count) {
			const_lptr_t *item = lptr_raw(
				lptr_index(statement, count)
			);
			*item = const_lptr(words->word);
			words = words->next;
		}

		error = exec_command(const_lptr(statement));
	}
	return error;
}

static char** word_list_to_array(word_list_t *words, int count)
{
	char **error = NULL;
	// +1 for the NULL terminator
	LPTR_WITH(statement, (size_t)count + 1, sizeof(char*)) {
		for (--count; count >= 0; --count) {
			char **item = lptr_raw(
				lptr_index(statement, count)
			);
			*item = lptr_raw(words->word);
			words = words->next;
		}

		return lptr_raw(statement);
	}
	return error;
}

static token_t skip_context(token_t token)
{
	token = token_next(token);
	while (token.type != lex_curly_block_end) {
		if (token.type == lex_end || token.type == lex_unexpected) {
			return token;
		} else if (token.type == lex_curly_block) {
			token = skip_context(token);
			if (token.type == lex_end || token.type == lex_unexpected)
				return token;
			token = token_next(token);
		} else {
			token = token_next(token);
		}
	}
	return token;
}

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
	static const token_t resource_error = { 0 };
	token = token_next(token);

	if (token.type == lex_unexpected) {
		return resource_error;
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
			return resource_error;

		// if the LPTR_WITH() below fails to alloc, the context
		// won't run so we want to return an error in that case
		token_t result = resource_error;
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
		/*
		 * This whole stupid confusing mess is to try to
		 * prevent "younger siblings" from inheriting file
		 * descriptors to their "older siblings", which are
		 * intended to be opened _exclusively_ for their
		 * parent. No, I don't know how to phrase that better.
		 *
		 * I _should_ think about this more, but I'm sick to
		 * death of thinking about this as-is. Fuck elegant, I just
		 * want this bastard to work.
		 */
		int sockets[2] = { 0 };
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
			return resource_error;
		switch (fork()) {
			case -1:
				close(sockets[0]);
				close(sockets[1]);
				return resource_error;
			case 0:
				for (int i = sockets[0]; i > SRV_FILENO; i--)
					close(i);
				if (dup2(sockets[1], SRV_FILENO) < 0)
					exit(1);
				close(sockets[1]);
				token = parse_script_impl(token);
				const bool error = token.type == NULL
					|| token.type == lex_unexpected;
				if (error)
					exit(1);
				int clients_end = get_clients_end();
				if (clients_end < 0)
					exit(1);

				// 21 characters should be enough for a 64
				// bit integer + null terminator
				// and if we have that many clients then
				// I have other concerns
				typedef char int64_str[21];
				int64_str clients_end_str = { 0 };
				if (
					snprintf(
						clients_end_str,
						sizeof(clients_end_str),
						"%d",
						clients_end
					) < 0
				) {
					exit(1);
				}

				const bool overwrite = true;
				if (
					setenv(
						"SRVSH_CLIENTS_END",
						clients_end_str,
						overwrite
					) < 0
				) {
					exit(1);
				}
				// I should clean this shit up sometime
				switch (fork()) {
					case -1:
						exit(1);
					case 0:
						exec_word_list(previous, count);
						exit(1);
					default: {
						int worst_return = EXIT_SUCCESS;
						for (int wstatus = 0; wait(&wstatus) > 0;)
							if (WIFEXITED(wstatus))
								worst_return = MAX(worst_return, WEXITSTATUS(wstatus));
							else if (WIFSIGNALED(wstatus))
								worst_return = MAX(worst_return, SIGNAL_RETURN_VALUE(WTERMSIG(wstatus)));
						exit(worst_return);
					}
				}
			default:
				close(sockets[1]);
				token = skip_context(token);
				if (token.type != lex_curly_block_end) {
					// TODO: send signal to shutdown children?
					token.type = lex_unexpected;
					return token;
				}
				return token_next(token);
		}
	} else /* if (token.type == lex_statement_separator) and friends */ {
		char **statement = word_list_to_array(previous, count);
		cliexecv(*statement, statement);
		free(statement);
		return token;
	}

	return token;
}

static token_t parse_script_impl(token_t token)
{
	for (;!is_end_token(token);) {
		token_t next = token_next(token);

		if (next.type == lex_word) {
			// TODO: don't re-lex next
			token = parse_statement_impl(token, NULL, 0);
		} else if (next.type == lex_curly_block) {
			// A curly bracket block at the top level is identical
			// to no curly bracket block
			token = parse_script_impl(next);
			if (token.type != lex_curly_block_end) {
				token.type = lex_unexpected;
				return token;
			}
		} else {
			token = next;
		}
	}
	return token;
}

int srvsh_parse_script(const_lptr_t script)
{
	token_t last = parse_script_impl(scallop_lang_lex_init(script));
	const bool error = last.type == NULL
		|| last.type == lex_unexpected;
	return error ? -1 : 0;
}

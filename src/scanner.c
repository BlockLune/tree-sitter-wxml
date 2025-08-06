#include "tree_sitter/parser.h"
#include <wctype.h>
#include <string.h>

enum TokenType {
  START_TAG_NAME,
  END_TAG_NAME,
  SELF_CLOSING_TAG_DELIMITER,
  RAW_TEXT,
  COMMENT,
  INTERPOLATION_START,
  INTERPOLATION_END,
};

void *tree_sitter_wxml_external_scanner_create() { return NULL; }
void tree_sitter_wxml_external_scanner_destroy(void *p) {}
void tree_sitter_wxml_external_scanner_reset(void *p) {}
unsigned tree_sitter_wxml_external_scanner_serialize(void *p, char *buffer) { return 0; }
void tree_sitter_wxml_external_scanner_deserialize(void *p, const char *b, unsigned n) {}

static bool is_reserved_word(const char *word, size_t len) {
  static const char *reserved[] = {"template", "slot", "block", "import", "include", "wxs"};
  const size_t count = sizeof(reserved) / sizeof(reserved[0]);
  for (size_t i = 0; i < count; i++) {
    if (strlen(reserved[i]) == len && strncmp(reserved[i], word, len) == 0) {
      return true;
    }
  }
  return false;
}

static bool scan_tag_name(TSLexer *lexer) {
  if (!(iswalpha(lexer->lookahead) || lexer->lookahead == '_')) {
    return false;
  }

  char buffer[64];
  size_t len = 0;

  while (iswalnum(lexer->lookahead) || lexer->lookahead == '_' || lexer->lookahead == '-' || lexer->lookahead == ':') {
    if (len < sizeof(buffer) - 1) {
      buffer[len++] = (char)lexer->lookahead;
    }
    lexer->advance(lexer, false);
  }
  buffer[len] = '\0';

  if (is_reserved_word(buffer, len)) {
    return false;
  }

  return true;
}

static bool scan_comment(TSLexer *lexer) {
  if (lexer->lookahead != '<') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '!') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '-') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '-') return false;
  lexer->advance(lexer, false);

  while (lexer->lookahead) {
    if (lexer->lookahead == '-') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '-') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '>') {
          lexer->advance(lexer, false);
          return true;
        }
      }
    } else {
      lexer->advance(lexer, false);
    }
  }
  return false;
}

bool tree_sitter_wxml_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  while (iswspace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[INTERPOLATION_START]) {
     if (lexer->lookahead == '{') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '{') {
        lexer->advance(lexer, false);
        lexer->result_symbol = INTERPOLATION_START;
        return true;
      }
    }
  }

  if (valid_symbols[INTERPOLATION_END]) {
    if (lexer->lookahead == '}') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '}') {
        lexer->advance(lexer, false);
        lexer->result_symbol = INTERPOLATION_END;
        return true;
      }
    }
  }

  if (valid_symbols[COMMENT]) {
    if (scan_comment(lexer)) {
      lexer->result_symbol = COMMENT;
      return true;
    }
  }

  if (valid_symbols[RAW_TEXT]) {
    lexer->result_symbol = RAW_TEXT;
    bool has_content = false;

    while (lexer->lookahead) {
      if (lexer->lookahead == '<') {
        lexer->mark_end(lexer);
        lexer->advance(lexer, false);
        if (lexer->lookahead == '/') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == 'w') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == 'x') {
              lexer->advance(lexer, false);
              if (lexer->lookahead == 's') {
                lexer->advance(lexer, false);
                if (lexer->lookahead == '>') {
                  return has_content;
                }
              }
            }
          }
        }
        has_content = true;
      } else {
        lexer->advance(lexer, false);
        has_content = true;
      }
    }
    return false;
  }

  if (valid_symbols[START_TAG_NAME]) {
    if (scan_tag_name(lexer)) {
      lexer->result_symbol = START_TAG_NAME;
      return true;
    }
  }

  if (valid_symbols[END_TAG_NAME]) {
    if (scan_tag_name(lexer)) {
      lexer->result_symbol = END_TAG_NAME;
      return true;
    }
  }

  if (valid_symbols[SELF_CLOSING_TAG_DELIMITER]) {
    if (lexer->lookahead == '/') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '>') {
        lexer->advance(lexer, false);
        lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
        return true;
      }
    }
  }

  return false;
}

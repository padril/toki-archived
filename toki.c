#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


//////////////
// TOKENIZE //
//////////////

typedef enum Keyword {
    KEYWORD_O,
    KEYWORD_SITELEN,
    KEYWORD_TOKI,
    KEYWORD_E,
} Keyword;

const char* keywords[] = {
    "o",
    "sitelen",
    "toki",
    "e",
};

const int KEYWORD_COUNT = sizeof(keywords) / sizeof(const char*);


typedef const char* Lexeme;


typedef enum TokenType
{
    TOKEN_NULL,
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_SEPERATOR,
    TOKEN_LITERAL,  // The first byte of "value" represents type information
    TOKEN_END_PHRASE,
} TokenType;


typedef enum LiteralType
{
    LITERAL_STRING,
} LiteralType;


#define LITERAL_OFFSET(T) (sizeof(LiteralType) / sizeof(T))


typedef struct LexemeList
{
    Lexeme* list;
    size_t size;
} LexemeList;


typedef struct Token
{
    TokenType type;
    void* value;
    size_t size;
} Token;


typedef struct TokenList
{
    Token* list;
    size_t size;
} TokenList;


LexemeList scan(const char* input)
{
    typedef enum Mode {
        UNKNOWN,
        TEXT,
        STRING,
        END,
    } Mode;

    Lexeme* lexemes = malloc(1000 * sizeof(Lexeme));
    int lexemes_size = 0;
    Lexeme lex;
    Mode mode = UNKNOWN;
    const char *p = input, *q = input;
    int ln = 0;
    int col = 0;

    // parse tokens
    while (*(p - 1) != '\0') {  // use p - 1 in case file ends early
        if (mode == UNKNOWN) {
            if (isspace(*p) || iscntrl(*p)) {
                // do nothing!
            } else if (isalpha(*p)) {
                mode = TEXT;
            } else if (*p == '"') {
                mode = STRING;
            } else {
                fprintf(stderr,
                        "Unknown lexeme %s at Ln %d, Col %d.\n",
                        *p, ln, col);
                exit(1);
            }
        } else if (mode == TEXT) {
            if (isspace(*p) || iscntrl(*p)) {
                // construct string
                int str_size = p - q;
                char* text = malloc((str_size + 1) * sizeof(char));
                strncpy(text, q, str_size);
                text[str_size] = '\0';

                // construct token
                lex = text;

                // add token
                lexemes[lexemes_size] = lex;
                ++lexemes_size;

                // clean
                mode = UNKNOWN;
                q = p + 1;  // p will be incremented by one later
            } else if (isalnum(*p)) {
            }
        } else if (mode == STRING) {
            if (*p == '\\') {
                ++p;
            } else if (*p == '"') {
                // construct string
                int str_size = p - q + 1;  // plut one to include close quote
                char* text = malloc((str_size + 1) * sizeof(char));
                strncpy(text, q, str_size);
                text[str_size] = '\0';

                // construct token
                lex = text;

                // add token
                lexemes[lexemes_size] = lex;
                ++lexemes_size;

                // clean
                mode = UNKNOWN;
                q = p + 1;  // p will be incremented by one later
            }
        }

        ++p;
    }

    return (LexemeList) {.list = lexemes, .size = lexemes_size};
}


TokenList evaluate(LexemeList input)
{
    Lexeme* lexemes = input.list;
    Token* tokens = malloc(100 * sizeof(Token));
    size_t tokens_size = 0;
    
    for (int lex = 0; lex < input.size; ++lex) {
        TokenType type = TOKEN_NULL;
        void* value = NULL;
        size_t size = 0;

        // keyword and identifier
        // TODO: replace with binsearch, or hash
        
        for (int kw = 0; kw < KEYWORD_COUNT; ++kw) {
            if (!strcmp(lexemes[lex], keywords[kw])) {
                type = TOKEN_KEYWORD;
                size = sizeof(Keyword);
                value = malloc(size);
                * (Keyword*) value = (Keyword) kw;
                break;
            }
        }
        if (type != TOKEN_NULL) {
            tokens[tokens_size] = (Token) {
                .type = type,
                .value = value,
                .size = size
            };
            ++tokens_size;
            continue;
        }

        // literals

        if (lexemes[lex][0] == '"') {
            type = TOKEN_LITERAL;
            const char *p = lexemes[lex] + 1, *q = lexemes[lex] + 1;
            while (*p != '"') {
                ++p;
            }
            int str_size = p - q;  // no +1 to get rid of end quote
            // +1 to make room for type byte
            size = sizeof(LiteralType) + (str_size) * sizeof(char);
            value = malloc(size);
            strncpy(value + LITERAL_OFFSET(char), q, str_size);
            ((char*) value)[0] = (char) LITERAL_STRING;
            ((char*) value)[size] = '\0';
            tokens[tokens_size] = (Token) {
                .type = type,
                .value = value,
                .size = size
            };
            ++tokens_size;
            continue;
        }
    }

    return (TokenList) {.list = tokens, .size = tokens_size};
}


///////////
// PARSE //
///////////




//////////
// MAIN //
//////////


int main(int argc, char* argv[])
{
    // check number of arguments
    if (argc != 2) {
        fprintf(
            stderr,
            "Incorrect number of arguments (expected 1 got %d).\n",
            argc - 1
        );

        exit(1);
    }
    
    // open file
    const char* fname = argv[1];
    FILE* fptr = fopen(fname, "r");

    // check file existed
    if (fptr == NULL) {
        fprintf(
            stderr,
            "File \"%s\" not found.\n",
            fname
        );

        exit(1);
    }

    // read file
    char str[101];
    fgets(str, 100, fptr);
    str[100] = '\0';
    
    LexemeList lexemes = scan(str);
    TokenList tokens = evaluate(lexemes);

    for (int i = 0; i < tokens.size; ++i) {
        switch (tokens.list[i].type) {
            case TOKEN_KEYWORD:
                printf("Type: %d, Value: %s\n",
                    tokens.list[i].type,
                    keywords[* (Keyword*) tokens.list[i].value]);
                break;
            case TOKEN_LITERAL:
                switch (* (char*) tokens.list[i].value) {
                    case LITERAL_STRING:
                        printf("Type: %d, Value: %s\n",
                            tokens.list[i].type,
                            ((char*) tokens.list[i].value)
                            + LITERAL_OFFSET(char)
                            );
                        break;
                }
                break;
        }
    }

    exit(0);
}

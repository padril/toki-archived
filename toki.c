// Copyright 2023 Leo James Peckham (padril)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


// Next refactor/cleanup milestone:
// Working "Hello, world!" program!


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


typedef enum Seperator {
    SEPERATOR_PERIOD,
} Seperator;

const char* seperators[] = {
    ".",
};

const int SEPERATOR_COUNT = sizeof(seperators) / sizeof(const char*);


typedef const char* Lexeme;


typedef enum TokenType
{
    TOKEN_NULL,
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_SEPERATOR,
    TOKEN_LITERAL,  // The first byte of "value" represents type information
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

// this is some confusing naming, TODO ig
const Token NULL_TOKEN = (Token) {
    .type = TOKEN_NULL,
    .size = 0,
    .value = NULL
};


typedef struct TokenList
{
    Token* list;
    size_t size;
} TokenList;

const TokenList NULL_TOKENLIST = (TokenList) {
    .list = NULL,
    .size = 0
};


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
            } else if (*p == '.') {
                lexemes[lexemes_size] = ".";
                ++lexemes_size;
                q = p + 1;
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

        // seperators

        for (int sp = 0; sp < SEPERATOR_COUNT; ++sp) {
            if (!strcmp(lexemes[lex], seperators[sp])) {
                type = TOKEN_SEPERATOR;
                size = sizeof(Seperator);
                value = malloc(size);
                * (Seperator*) value = (Seperator) sp;
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
    }

    return (TokenList) {.list = tokens, .size = tokens_size};
}


///////////
// PARSE //
///////////

// First probably needs to be transpiled into some intermediate language
// that is more easily formed into ASM?

// I know this is no Chompsky bare phrase structure or anything, but that
// level of abstraction is worthless for this application. Basic phrase
// structure rules it is!

typedef struct NounPhrase {
    Token noun;
    TokenList adjp;
} NounPhrase;

typedef struct VerbPhrase {
    Token verb;
    TokenList advp;
    NounPhrase object;
} VerbPhrase;

// this is where any "la" phrases, clauses, subphrases etc. will eventually
// be contained (if we have any)
typedef struct Sentence {
    NounPhrase subject;
    VerbPhrase predicate;
    // tense? aspect? mood? li vs o distinction might go here later
} Sentence;

typedef struct SentenceList {
    Sentence* list;
    size_t size;
} SentenceList;


// still need to figure out how to do boolean "ands", since "en" will be used
// to conjoin subjects for APL like array processing shit

SentenceList parse(TokenList input) {
    // currently not accounting for "la"
    typedef enum Mode {
        PHRASE_EN,
        PHRASE_O,
        PHRASE_E
    } Mode;

    Mode mode = PHRASE_EN;

    Sentence s;
    TokenList block;
    block.list = NULL;
    block.size = 0;

    Sentence* sl = malloc(50 * sizeof(Sentence));
    size_t size = 0;

    Token* p = input.list;
    Token* q = input.list;
    
    // We use modes, and switch between them, updating as we go, to go with
    // the arbitrary way toki pona phrases can be ordered.
    // still need support for something like
    // >>> e "Hello, " o sitelen e "world!".
    for (int i = 0; i < input.size; ++i) {
        if (p->type == TOKEN_KEYWORD &&
            * (Keyword*) p->value == KEYWORD_O) {
            if (mode == PHRASE_EN) {
                if (block.size == 0) {
                    s.subject.noun = NULL_TOKEN;
                    s.subject.adjp = NULL_TOKENLIST;
                } else if (block.size == 1) {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = NULL_TOKENLIST;
                } else {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1
                    };
                }
            } else if (mode == PHRASE_E) {
                // TODO: implement
            }
            mode = PHRASE_O;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;
        } else if (p->type == TOKEN_KEYWORD &&
                   * (Keyword*) p->value == KEYWORD_E) {
            if (mode == PHRASE_EN) {
                if (block.size == 0) {
                    s.subject.noun = NULL_TOKEN;
                    s.subject.adjp = NULL_TOKENLIST;
                } else if (block.size == 1) {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = NULL_TOKENLIST;
                } else {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1
                    };
                }
            } else if (mode == PHRASE_O) {
                if (block.size == 0) {
                    s.predicate.verb = NULL_TOKEN;
                    s.predicate.advp = NULL_TOKENLIST;
                } else if (block.size == 1) {
                    s.predicate.verb = block.list[0];
                    s.predicate.advp = NULL_TOKENLIST;
                } else {
                    s.predicate.verb = block.list[0];
                    s.predicate.advp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1
                    };
                }
            }
            mode = PHRASE_E;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;
        } else if (p->type == TOKEN_SEPERATOR &&
                   * (Seperator*) p->value == SEPERATOR_PERIOD) {

            if (mode == PHRASE_E) {
                if (block.size == 0) {
                    s.predicate.object.noun = NULL_TOKEN;
                    s.predicate.object.adjp = NULL_TOKENLIST;
                } else if (block.size == 1) {
                    s.predicate.object.noun = block.list[0];
                    s.predicate.object.adjp = NULL_TOKENLIST;
                } else {
                    s.predicate.object.noun = block.list[0];
                    s.predicate.object.adjp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1
                    };
                }
            }

            mode == PHRASE_EN;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;

            // definitely need to reset s at some point here
            sl[size] = s;
            ++size;
        } else {
            block.list[block.size] = *p;
            ++block.size;
        }
        ++p;
    }

    return (SentenceList) {
        .list = sl,
        .size = size
    };
}

void parseVP() {

}

void printToken(Token t) {
    switch (t.type) {
            case TOKEN_KEYWORD:
                printf("Type: %d, Value: %s\n",
                    t.type,
                    keywords[* (Keyword*) t.value]);
                break;
            case TOKEN_LITERAL:
                switch (* (char*) t.value) {
                    case LITERAL_STRING:
                        printf("Type: %d, Value: \"%s\" : String\n",
                            t.type,
                            ((char*) t.value)
                            + LITERAL_OFFSET(char)
                            );
                        break;
                }
                break;
            case TOKEN_SEPERATOR:
                printf("Type: %d, Value: %s\n",
                    t.type,
                    seperators[* (Seperator*) t.value]);
                break;
    }
}




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
    SentenceList sentences = parse(tokens);

    for (int i = 0; i < sentences.size; ++i) {
        printf("[S\n");
            printf("\t[N\n\t\t");
                printToken(sentences.list[i].subject.noun);
            printf("\t]\n");
            printf("\t[VP\n");
                printf("\t\t[V\n\t\t\t");
                    printToken(sentences.list[i].predicate.verb);
                printf("\t\t]\n");
                printf("\t\t[N\n\t\t\t");
                    printToken(sentences.list[i].predicate.object.noun);
                printf("\t\t]\n");
            printf("\t]\n");
        printf("]\n");
    }

    /*
    // print token list
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
                        printf("Type: %d, Value: \"%s\" : String\n",
                            tokens.list[i].type,
                            ((char*) tokens.list[i].value)
                            + LITERAL_OFFSET(char)
                            );
                        break;
                }
                break;
            case TOKEN_SEPERATOR:
                printf("Type: %d, Value: %s\n",
                    tokens.list[i].type,
                    seperators[* (Seperator*) tokens.list[i].value]);
                break;
        }
    }
    */

    exit(0);
}

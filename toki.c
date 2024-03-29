// Copyright 2023 Leo James Peckham (padril)

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FLAG DEFINITIONS

/* Gives default arguments, if not provided. Useful for debugging.
 */
#define DEFAULT_INPUT_FILENAME \
    "./test/end_to_end/hello_world/hello_world_test.toki"
#define DEFAULT_OUTPUT_FILENAME "a"

/* When set to 1, will delete generated .asm and .obj files. Turn off if
 * you want to keep the files (e.g. to debug ASM code)
 */
#define DELETE_INTERMEDIATE 0

// GRAMMAR DEFINITIONS

/* Lexemes are raw strings which match a Token's pattern that have yet to be
 * evaluated. Created from input using `scan()`. `LexemeList` is a list of
 * Lexemes that also keeps track of the length, to allow easy passing to
 * functions and iteration. 
 */
typedef const char * Lexeme;

typedef struct LexemeList
{
    Lexeme *list;
    size_t size;
} LexemeList;

/* Keeps track of the language's Keywords as an enum `Keyword` and a list of
 * strings `KEYWORDS` as well as their length in `KEYWORD_COUNT`. Both enum
 * and list *must* be in the same order, preferably alphabetical.
 * Keywords are a type of Token.
 */
typedef enum Keyword
{
    KEYWORD_E,
    KEYWORD_O,
    KEYWORD_SITELEN,
    KEYWORD_TOKI,
    KEYWORD_LI,
    KEYWORD_KAMA,
    KEYWORD_SAMA,
    KEYWORD_KIN,
} Keyword;

const char *KEYWORDS[] = {
    "e",
    "o",
    "sitelen",
    "toki",
    "li",
    "kama",
    "sama",
    "kin"
};

const int KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(const char *);

/* Keeps track of the language's Separators (which are mostly punctuation
 * marks used) as an enum `Separator` and a list of strings `SEPARATORS`
 * as well as their length in `SEPARATOR_COUNT`. Both enum and list *must*
 * be in the same order, preferably alphabetical.
 * Separators are a type of Token.
 */
typedef enum Separator
{
    SEPARATOR_PERIOD,
} Separator;

const char *SEPARATORS[] = {
    ".",
};

const int SEPARATOR_COUNT = sizeof(SEPARATORS) / sizeof(const char *);

/* Keeps track of the different types of Literals in toki as an enum
 * `Literal` and a list of strings `LITERALS` (which is just a human
 * readable name for the type) as well as their length in `LITERAL_COUNT`.
 * Both enum and list *must* be in the same order, preferably alphabetical.
 * Literals are a type of Token.
 */
typedef enum Literal
{
    LITERAL_NULL,
    LITERAL_STRING,
    LITERAL_INTEGER,
    LITERAL_FLOAT,
} Literal;

const char *LITERALS[] = {
    "Null",
    "String",
    "Integer",
    "Float",
};

const int LITERAL_COUNT = sizeof(LITERALS) / sizeof(const char *);

typedef void NullLiteralType;
typedef char CharLiteralType;  // I dont like how I do this one, so TODO ig
typedef CharLiteralType* StringLiteralType;
typedef long long IntegerLiteralType;  // These use strto methods in the code
typedef double FloatLiteralType;       // make sure to update those too

/* Tokens are the base unit of the program's grammar. They store a type as
 * a `TokenType`, a value, and a size of that value in bytes. They are
 * created by Lexemes using `evaluate`. `TokenList` is a list of Tokens that
 * also keeps track of the length, to allow easy passing to functions and
 * iteration.
 * Default/NULL values of `Token` and `TokenList` also exist as
 * `Token_default` and `TokenList_default`, due to the necessity of
 * initializing Tokens without a value.
 */
typedef enum TokenType
{
    TOKEN_NULL,
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD,
    TOKEN_SEPARATOR,
    TOKEN_LITERAL,
    TOKEN_OPERATOR,  // Considered conjunctions semantically
} TokenType;

typedef struct Token
{
    TokenType type;
    void *value;
    Literal literal;
} Token;

#define TOKEN_DEFAULT ((Token) {\
    .type = TOKEN_NULL, .literal = LITERAL_NULL, .value = NULL\
})

typedef struct TokenList
{
    Token *list;
    size_t size;
} TokenList;

#define TOKENLIST_DEFAULT ((TokenList) {\
    .list = NULL,\
    .size = 0\
})

/* The following structs define a DCG for constructing toki's expressions,
 * which due to the emulation of a natural language look more like sentences.
 * The structs only hold information intrinsic to the type of phrase, and do
 * not describe word-order (that's dealt with in `parse()`)
 * I know this is no Chompsky bare phrase structure or anything, but that
 * level of abstraction is worthless for this application. Basic phrase
 * structure rules it is!
 * 
 * NP holds N (AdjP+) (PP+)
 * PP -> P (NP)  (N.B Operators are normally here, and are evaluated left to
 *                right, instead of in typical BEDMAS order)
 * VP holds V (AdvP+) (NP)
 * S  holds (NP) (VP)  (N.B a lone NP or doesn't do anything! could be useful
 *                      for a REPL in the future though...)
 * 
 * These use a lot of short forms because in later code it gets verbose.
 */

typedef struct NounPhraseWithoutPrep {
    Token noun;
    TokenList adjp;
} NounPhraseWithoutPrep;

#define NOUNPHRASEWITHOUTPREP_DEFAULT ((NounPhraseWithoutPrep) {\
    .noun = TOKEN_DEFAULT,\
    .adjp = TOKENLIST_DEFAULT\
})

typedef struct PrepPhrase {
    Token prep;
    NounPhraseWithoutPrep np;  // this whole PreplessNounPhrase shit prevents
                            // weird recursive stuff from happening
} PrepPhrase;

#define PREPPHRASE_DEFAULT ((PrepPhrase) {\
    .prep = TOKEN_DEFAULT,\
    .np = NOUNPHRASEWITHOUTPREP_DEFAULT\
})

typedef struct PrepPhraseList {
    PrepPhrase *list;
    size_t size;
} PrepPhraseList;

#define PREPPHRASELIST_DEFAULT ((PrepPhraseList) {\
    .list = NULL,\
    .size = 0\
})

typedef struct NounPhrase
{
    Token noun;         // noun
    TokenList adjp;     // adjective phrase
    PrepPhraseList ppl;  // prepositional phrase +
} NounPhrase;

#define NOUNPHRASE_DEFAULT ((NounPhrase) {\
    .noun = TOKEN_DEFAULT,\
    .adjp = TOKENLIST_DEFAULT,\
    .ppl = PREPPHRASELIST_DEFAULT\
})

typedef struct VerbPhrase
{
    Token verb;       // verb
    TokenList advp;   // adverb phrase
    NounPhrase obj;   // object
} VerbPhrase;

#define VERBPHRASE_DEFAULT ((VerbPhrase) {\
    .verb = TOKEN_DEFAULT,\
    .advp = TOKENLIST_DEFAULT,\
    .obj = NOUNPHRASE_DEFAULT\
})

typedef struct Sentence
{
    NounPhrase subj;  // subject
    VerbPhrase pred;  // predicate
} Sentence;

#define SENTENCE_DEFAULT ((Sentence) {\
    .subj = NOUNPHRASE_DEFAULT,\
    .pred = VERBPHRASE_DEFAULT\
})

typedef struct SentenceList
{
    Sentence *list;
    size_t size;
} SentenceList;

/* These structs hold the data that will be included in the write to ASM.
 * `SectionData` holds information in `section .data`, and
 * `SectionText` holds information in `section .text`.
 */
typedef struct SectionData
{
    const char **lines;
    size_t size;
    int literals;
} SectionData;

typedef struct SectionText
{
    const char **lines;
    size_t size;
} SectionText;

// HELPER FUNCTIONS

static inline bool is_keyword(Token t, Keyword kw) {
    return (t.type == TOKEN_KEYWORD) &&
           (* (Keyword *) t.value == kw);
}

static inline bool is_seperator(Token t, Separator sp) {
    return (t.type == TOKEN_SEPARATOR) &&
           (* (Separator *) t.value == sp);
}

static inline bool is_literal(Token t, Literal lt) {
    return (t.type == TOKEN_LITERAL) &&
           (t.literal == lt);
}

// LEXICAL ANALYSIS

/* Begins the process of Lexical Analysis by generating a list of Lexemes
 * from the input string.
 * (https://en.wikipedia.org/wiki/Lexical_analysis)
 * 
 * Matches:
 * TEXT   = [a-zA-Z][a-zA-Z0-9]*
 * NUMBER = [0-9]+(\.[0-9]+)
 * STRING = ".*"
 */
LexemeList scan(const char *input)
{
    // Match denotes what the current Lexeme being analyzed could match
    typedef enum Match
    {
        UNKNOWN,
        TEXT,
        NUMBER,
        STRING,
        END,
    } Match;

    Lexeme *lexemes = malloc(1000 * sizeof(Lexeme));
    size_t lexemes_size = 0;
    Lexeme lex;
    Match mode = UNKNOWN;
    const char *p = input;  // start of the lexeme
    const char *q = input;  // end of the lexeme
    int ln = 0;
    int col = 0;

    // parse tokens
    while (*(p - 1) != '\0')  // use p - 1 in case file ends early
    {
        if (mode == UNKNOWN)
        {
            if (isspace(*p) || iscntrl(*p))
            {
                ++q; // do nothing, whitespace
            }
            else if (isalpha(*p))
            {
                mode = TEXT;
            }
            else if (*p == '"')
            {
                mode = STRING;
            }
            else if (*p == '.')
            {
                lexemes[lexemes_size] = ".";
                ++lexemes_size;
                q = p + 1;
            }
            else if (isdigit(*p))
            {
                mode = NUMBER;
            }
            else
            {
                fprintf(stderr,
                        "Unknown lexeme %s at Ln %d, Col %d.\n",
                        *p, ln, col);
                exit(-1);
            }
        }
        else if (mode == TEXT)
        {
            if (isspace(*p) || iscntrl(*p) || (*p == '.'))
            {
                // construct string
                int str_size = p - q;
                char *text = malloc((str_size + 1) * sizeof(char));
                strncpy(text, q, str_size);
                text[str_size] = '\0';

                // construct token
                lex = text;

                // add token
                lexemes[lexemes_size] = lex;
                ++lexemes_size;
                if (*p == '.')
                {
                    lexemes[lexemes_size] = ".";
                    ++lexemes_size;
                }
                // clean
                mode = UNKNOWN;
                q = p + 1; // p will be incremented by one later
            }
            else if (isalnum(*p))
            {
                ; // continue to next
            }
        }
        else if (mode == STRING)
        {
            if (*p == '\\')
            {
                ++p;
            }
            else if (*p == '"')
            {
                // construct string
                int str_size = p - q + 1; // plus one to include close quote
                char *text = malloc((str_size + 1) * sizeof(char));
                strncpy(text, q, str_size);
                text[str_size] = '\0';

                // construct token
                lex = text;

                // add token
                lexemes[lexemes_size] = lex;
                ++lexemes_size;

                // clean
                mode = UNKNOWN;
                q = p + 1; // p will be incremented by one later
            }
        }
        else if (mode == NUMBER)
        {
            // When we encounter a period, we need to lookahead to see if there
            // is another number later
            if (isdigit(*p) || (*p == '.' && isdigit(*(p + 1))))
            {
                ; // continue to next
            }
            else
            {
                // construct string
                int str_size = p - q;
                char *text = malloc((str_size + 1) * sizeof(char));
                strncpy(text, q, str_size);
                text[str_size] = '\0';

                // construct token
                lex = text;

                // add token
                lexemes[lexemes_size] = lex;
                ++lexemes_size;
                if (*p == '.')
                {
                    lexemes[lexemes_size] = ".";
                    ++lexemes_size;
                }
                // clean
                mode = UNKNOWN;
                q = p + 1; // p will be incremented by one later
            }
        }

        ++p;
    }

    return (LexemeList) {.list = lexemes, .size = lexemes_size};
}

/* Completes the process of Lexical Analysis by evaluating a list of Lexemes
 * and creating a list of Tokens.
 * (https://en.wikipedia.org/wiki/Lexical_analysis)
 */
TokenList evaluate(LexemeList input)
{
    Lexeme *lexemes = input.list;
    // TODO: This shouldn't be static
    Token *tokens = malloc(100 * sizeof(Token));
    size_t tokens_size = 0;

    for (int lex = 0; lex < input.size; ++lex)
    {
        Token curr = TOKEN_DEFAULT;

        // keyword
        // TODO: replace with binsearch, or hash

        for (int kw = 0; kw < KEYWORD_COUNT; ++kw)
        {
            if (!strcmp(lexemes[lex], KEYWORDS[kw]))
            {
                curr.type = TOKEN_KEYWORD;
                curr.value = malloc(sizeof(Keyword));
                * (Keyword *) curr.value = (Keyword) kw;
                break;
            }
        }
        if (curr.type != TOKEN_NULL)
        {
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }

        // identifier

        bool ident = true;
        if (isalpha(*lexemes[lex]))  // is first character alpha?
        {
            const char *p = lexemes[lex];
            while (*p != '\0')
            {
                if (!isalnum(*p) && (*p != '_'))
                {
                    ident = false;
                    break;
                }
                ++p;
            }
        }
        else
        {
            ident = false;
        }
        if (ident)
        {
            curr.type = TOKEN_IDENTIFIER;
            size_t size = sizeof(char) * strlen(lexemes[lex]);
            curr.value = malloc(size);
            strncpy((char *) curr.value, lexemes[lex], size);
            ((char *) curr.value)[size] = '\0';
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }

        // literals

        if (lexemes[lex][0] == '"')  // string literal
        {
            curr.type = TOKEN_LITERAL;
            const char *p = lexemes[lex] + 1, *q = lexemes[lex] + 1;
            while (*p != '"')
            {
                ++p;
            }
            int str_size = p - q; // no +1 to get rid of end quote
            size_t size = (str_size) * sizeof(CharLiteralType);
            curr.value = malloc(size);
            strncpy((StringLiteralType) curr.value, q, str_size);
            curr.literal = LITERAL_STRING;
            ((StringLiteralType) curr.value)[size] = '\0';
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }
        else if (isdigit(lexemes[lex][0]))  // number literal
        {
            curr.type = TOKEN_LITERAL;
            const char *p = lexemes[lex];

            // is it an integer, or a float?
            bool integer = true;
            while (*p)
            {
                if (*p == '.')
                {
                    integer = false;
                    break;
                }
                ++p;
            }
            p = lexemes[lex]; // reset

            if (integer)
            {
                curr.value = malloc(sizeof(IntegerLiteralType));
                * (long long *) curr.value = strtoll(p, NULL, 10);
                curr.literal = LITERAL_INTEGER;
            }
            else
            {
                curr.value = malloc(sizeof(FloatLiteralType));
                * (double *) curr.value = strtod(p, NULL);
                curr.literal = LITERAL_FLOAT;
            }
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }

        // separators

        for (int sp = 0; sp < SEPARATOR_COUNT; ++sp)
        {
            if (!strcmp(lexemes[lex], SEPARATORS[sp]))
            {
                curr.type = TOKEN_SEPARATOR;
                curr.value = malloc(sizeof(Separator));
                * (Separator *) curr.value = (Separator)sp;
                break;
            }
        }
        if (curr.type != TOKEN_NULL)
        {
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }
    }

    return (TokenList){.list = tokens, .size = tokens_size};
}

// PARSING

/* Uses DCG to convert a `TokenList` into the information necessary to create
 * ASM. Puts information into the phrases defined under `Sentence`.
 * This process is significantly simplified by toki pona's use of particles
 * to denote the subject, object, and verb.
 * 
 * NOTE: Currently this language does not permit "nasin kijete" (free word
 * order). This feature may be available as a flag in the future.
 * 
 * PSR for toki (in their current state)
 * S --> (Literal) o sitelen e Literal
 */
SentenceList parse(TokenList input)
{
    // currently not accounting for "la"
    typedef enum Mode
    {
        PHRASE_EN,
        PHRASE_O,
        PHRASE_E,
        PHRASE_PREP,
    } Mode;

    Mode mode = PHRASE_EN;

    Sentence s = SENTENCE_DEFAULT;

    TokenList buffer;
    buffer.list = malloc(10 * sizeof(Token));
    buffer.size = 0;

    Token* head = NULL;
    TokenList* tail = NULL;
    PrepPhraseList* ppl = NULL;

    Sentence *sl = malloc(50 * sizeof(Sentence));
    size_t size = 0;

    Token *p = input.list;

    // We use modes, and switch between them, updating as we go
    // The current mode defines the head, tail, and ppl, and when we encounter
    // anything that indicates we're switching modes, we add stuff in the buffer
    // to the heads, and then finish switching modes. Rinse, repeat.
    for (int i = 0; i < input.size; ++i)
    {
        // Set head, tail, ppl
        if (mode == PHRASE_EN)
        {
            head = &(s.subj.noun);
            tail = &(s.subj.adjp);
            ppl  = &(s.subj.ppl);
        }
        else if (mode == PHRASE_O)
        {
            head = &(s.pred.verb);
            tail = &(s.pred.advp);
            ppl  = NULL;
        }
        else if (mode == PHRASE_E)
        {
            head = &(s.pred.obj.noun);
            tail = &(s.pred.obj.adjp);
            ppl  = &(s.pred.obj.ppl);
        }
        else if (mode == PHRASE_PREP)
        {
            head = &(ppl->list[ppl->size].np.noun);
            tail = &(ppl->list[ppl->size].np.adjp);
            ppl  = NULL;
        }

        // Did we encounter something indicated a switch of mode?
        if (is_keyword(*p, KEYWORD_O) || is_keyword(*p, KEYWORD_LI))
        {
            // Handle some invalid cases
            if (mode == PHRASE_E)
            {
                fprintf(
                    stderr,
                    "Unimplemented: cannot use multiple predicates");
                exit(-1);
            }
            else if (mode == PHRASE_O)
            {
                fprintf(
                    stderr,
                    "Unimplemented: cannot conjoin predicates");
                exit(-1);
            }

            // Push buffer into sentence
            if (buffer.size >= 1)
            {
                *head = buffer.list[0];
            }
            if (buffer.size >= 2)
            {
                *tail = (TokenList) {
                    .list = buffer.list + 1,
                    .size = buffer.size - 1};
            }

            // Switch mode, reset buffer
            mode = PHRASE_O;
            buffer.list = malloc(10 * sizeof(Token));
            buffer.size = 0;
        }
        else if (is_keyword(*p, KEYWORD_KIN))
        {
            // Handle some invalid cases
            if (mode == PHRASE_O)
            {
                fprintf(
                    stderr,
                    "Prepositions not allowed after verb phrases.");
                exit(-1);
            }
            else if (buffer.size == 0)
            {
                fprintf(
                    stderr,
                    "Monadic 'sin' is not allowed.");
                exit(-1);
            }

            // Push buffer into sentence
            if (buffer.size >= 1)
            {
                *head = buffer.list[0];
            }
            if (buffer.size >= 2)
            {
                s.subj.noun = buffer.list[0];
                s.subj.adjp = (TokenList) {
                    .list = buffer.list + 1,
                    .size = buffer.size - 1};
            }

            ppl->list[ppl->size].prep = *p;

            // Switch mode, reset buffer
            mode = PHRASE_PREP;
            buffer.list = malloc(10 * sizeof(Token));
            buffer.size = 0;            
        }
        else if (is_keyword(*p, KEYWORD_E))
        {
            // Handle some invalid cases
            if (mode == PHRASE_EN)
            {
                fprintf(
                    stderr,
                    "Error, invalid word order SO");
                exit(-1);
                // wait till nasin kijete flag is implemented!
            }

            // Push buffer into sentence
            if (buffer.size >= 1)
            {
                *head = buffer.list[0];
            }
            if (buffer.size >= 2)
            {
                *tail = (TokenList) {
                    .list = buffer.list + 1,
                    .size = buffer.size - 1};
            }

            // Switch mode, reset buffer
            mode = PHRASE_E;
            buffer.list = malloc(10 * sizeof(Token));
            buffer.size = 0;
        }
        else if (is_seperator(*p, SEPARATOR_PERIOD))
        {
            // Push buffer into sentence
            if (buffer.size >= 1)
            {
                *head = buffer.list[0];
            }
            if (buffer.size >= 2)
            {
                *tail = (TokenList) {
                    .list = buffer.list + 1,
                    .size = buffer.size - 1};
            }

            // Switch mode, reset buffer
            mode = PHRASE_EN;  // set PHRASE_EN here for next sentence
            buffer.list = malloc(10 * sizeof(Token));
            buffer.size = 0;

            // Start new sentence
            sl[size] = s;
            s = SENTENCE_DEFAULT;
            ++size;
        }
        // Identifiers/Literals, add to the buffer
        else
        {
            buffer.list[buffer.size] = *p;
            ++buffer.size;
        }

        ++p;  // Go to next Token
    }

    if (buffer.list != NULL)
    {
        free(buffer.list);
    }

    return (SentenceList) {
        .list = sl,
        .size = size};
}


// COMPILING

/* These are helper functions which help significantly reduce the necessary code
 * for inputing lines of ASM into `SectionData` and `SectionText`.
 */
void write_into_data(SectionData *data, const char *assembly, ...)
{
    // Find final length of formatted string
    va_list args; 
    va_list argscopy;
    va_start(args, assembly); 
    va_copy(argscopy, args);

    size_t formatted_length = (size_t) vsnprintf(NULL, 0, assembly, args) + 1; 

    char *line = malloc(formatted_length * sizeof(char));
    vsnprintf(line, formatted_length, assembly, argscopy);

    data->lines[data->size++] = line;

    va_end(args);
    va_end(argscopy);
}

void write_into_text(SectionText *text, const char *assembly, ...)
{
    // Find final length of formatted string
    va_list args; 
    va_list argscopy;
    va_start(args, assembly); 
    va_copy(argscopy, args);

    size_t formatted_length = (size_t) vsnprintf(NULL, 0, assembly, args) + 1; 

    char *line = malloc(formatted_length * sizeof(char));
    vsnprintf(line, formatted_length, assembly, argscopy);

    text->lines[text->size++] = line;

    va_end(args);
    va_end(argscopy);
}

/* Inputs data retrieved from `Sentence` into provided `SectionData` and
 * `SectionText` pointers by generating ASM.
 */
void compile_sentence(Sentence s, SectionData *data, SectionText *text)
{
    if (s.subj.noun.type == TOKEN_NULL)
    {
        // TODO: better error handling
        if (s.pred.verb.type == TOKEN_NULL)
        {
            fprintf(
                stderr,
                "Missing verb in sentence.\n");
            exit(-1);
            // else if (declarative) { error }
            // could really be a nice succinct switch statement probably
        }
        else if (is_keyword((s.pred.verb), KEYWORD_SITELEN))
        {
            if (s.pred.obj.noun.type == TOKEN_IDENTIFIER)
            {
                // TODO: what about different types for different idents?
                // Generate text
                write_into_text(text, "push    dword VARIABLE_%s",
                                (char *) s.pred.obj.noun.value);
                write_into_text(text, "push    dword formatString");
                write_into_text(text, "call    _printf");
                write_into_text(text, "add     esp, byte 8");
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_STRING))
            {
                // Generate data
                write_into_data(data, "LITERAL_%d db \"%s\", 0",
                                data->literals,
                                (StringLiteralType) s.pred.obj.noun.value);

                // Generate text
                write_into_text(text, "push    dword LITERAL_%d",
                                data->literals);
                write_into_text(text, "push    dword formatString");
                write_into_text(text, "call    _printf");
                write_into_text(text, "add     esp, byte 8");

                ++data->literals;
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_INTEGER))
            {
                write_into_text(text, "push    %d",
                                * (IntegerLiteralType *) s.pred.obj.noun.value);
                write_into_text(text, "push    dword formatInteger");
                write_into_text(text, "call    _printf");
                write_into_text(text, "add     esp, byte 8");
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_FLOAT))
            {
                write_into_text(text, "push    %d",
                                * (FloatLiteralType *) s.pred.obj.noun.value);
                write_into_text(text, "push    dword formatFloat");
                write_into_text(text, "call    _printf");
                write_into_text(text, "add     esp, byte 8");
            }
            else
            {
                fprintf(
                    stderr,
                    "Incorrect object for verb 'sitelen'.\n");
                exit(-1);
            }
        }
    }
    else  // extant subject
    {
        if (s.pred.verb.type == TOKEN_NULL)
        {
            return;  // Do nothing, we have an lone NP
        }
        else if (is_keyword(s.pred.verb, KEYWORD_KAMA))
        {
            if (s.subj.noun.type != TOKEN_IDENTIFIER)
            {
                fprintf(
                    stderr,
                    "Subject must be identifier in assignment.\n");
                exit(-1);
            }
            if (s.pred.obj.noun.type == TOKEN_NULL)
            {
                fprintf(
                    stderr,
                    "Assignment statement needs object.\n");
                exit(-1);
            }

            // Generate data
            if (is_literal(s.pred.obj.noun, LITERAL_STRING))
            {
                write_into_data(
                    data, "VARIABLE_%s db \"%s\", 0",
                    (char *) s.subj.noun.value,
                    (StringLiteralType) s.pred.obj.noun.value
                    );
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_INTEGER))
            {
                write_into_data(
                    data, "VARIABLE_%s dq %d",
                    (char *) s.subj.noun.value,
                    * (IntegerLiteralType *) s.pred.obj.noun.value
                    );
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_FLOAT))
            {
                write_into_data(
                    data, "VARIABLE_%s dq %f",
                    (char *) s.subj.noun.value,
                    * (FloatLiteralType *) s.pred.obj.noun.value
                    );
            }
        }
    }
}

/* Writes data from `SectionData` and `SectionText` into an assembly file.
 */
void write(const char *outfile, const SectionData *sd, const SectionText *st)
{
    char *fname = malloc(80 * sizeof(char));
    strcpy(fname, outfile);
    strcat(fname, ".asm");

    FILE *fptr = fopen(fname, "w");

    if (fptr == NULL)
    {
        fprintf(
            stderr,
            "Failed to create ASM file.\n"
            "  %s",
            strerror(errno));

        exit(-1);
    }

    // opening boilerplate
    fprintf(fptr,
            "    global _main\n"
            "    extern _printf\n\n"
            "section .text\n"
            "    _main:\n");

    // _main:
    for (int i = 0; i < st->size; ++i)
    {
        fprintf(fptr, "        %s\n", st->lines[i]);
    }

    // end .text boilerplate
    fprintf(fptr,
            "        ret\n\n");

    // .data boilerplate
    fprintf(fptr,
            "section .data\n"
            "    formatString db \"%%s\", 10, 0\n"
            "    formatInteger db \"%%d\", 10, 0\n"
            "    formatFloat db \"%%f\", 10, 0\n");

    // .data:
    for (int i = 0; i < sd->size; ++i)
    {
        fprintf(fptr, "    %s\n", sd->lines[i]);
    }

    fclose(fptr);
}

/* Makes the assembly file into an `.exe`.
 */
void make(const char *outfile)
{
    char *cmd1 = malloc(80 * sizeof(const char));
    char *cmd2 = malloc(80 * sizeof(const char));
    sprintf(cmd1, "nasm -f win32 %s.asm", outfile);
    sprintf(cmd2, "gcc %s.obj -o %s", outfile, outfile);
    system(cmd1);
    system(cmd2);
    free(cmd1);
    free(cmd2);

#if DELETE_INTERMEDIATE
    char *cmd3 = malloc(80 * sizeof(const char));
    char *cmd4 = malloc(80 * sizeof(const char));
    sprintf(cmd3, "del %s.asm", outfile);
    sprintf(cmd4, "del %s.obj", outfile);
    system(cmd3);
    system(cmd4);
    free(cmd3);
    free(cmd4);
#endif
}

/* Compiles every sentence, writes to an ASM file, and then makes the file.
 */
void compile(const char *outfile, SentenceList input)
{

    SectionData *sd = malloc(sizeof(SectionData));
    sd->literals = 0;
    SectionText *st = malloc(sizeof(SectionData));

    sd->lines = malloc(100 * sizeof(const char *));
    sd->size = 0;
    st->lines = malloc(100 * sizeof(const char *));
    st->size = 0;

    // Convert SentenceList to SectionData and SectionText
    for (int i = 0; i < input.size; ++i)
    {
        compile_sentence(input.list[i], sd, st);
    }

    write(outfile, sd, st);
    make(outfile);
}


// MAIN

int main(int argc, const char *argv[])
{
    const char *fname;
    const char *outfname;

    // check number of arguments
    if (argc == 3)
    {
        fname = argv[1];
        outfname = argv[2];
    } 
    else if (argc == 2)
    {
        fname = argv[1];
        outfname = DEFAULT_OUTPUT_FILENAME;
    }
    else if (argc == 1)
    {
        fname = DEFAULT_INPUT_FILENAME;
        outfname = DEFAULT_OUTPUT_FILENAME;
    }
    else
    {
        fprintf(
            stderr,
            "Incorrect number of arguments (expected 0, 1, or 2, got %d)."
            "Setting arguments to defaults and continuing\n",
            argc - 1);
        fname = DEFAULT_INPUT_FILENAME;
        outfname = DEFAULT_OUTPUT_FILENAME;
    }

    // open file
    FILE *fptr = fopen(fname, "rb");

    // check file existed
    if (fptr == NULL)
    {
        fprintf(
            stderr,
            "File \"%s\" not found.\n"
            "  %s",
            fname, strerror(errno));

        exit(-1);
    }

    // read file
    // from Nils Pipenbrinck on stack overflow
    fseek(fptr, 0, SEEK_END);
    long length = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    char *buffer = malloc((length + 1) * sizeof(char));
    if (buffer)
    {
        fread(buffer, 1, length, fptr);
        buffer[length] = '\0';
    }

    if (buffer == NULL)
    {
        fprintf(
            stderr,
            "Something went wrong reading file!"
        );
        exit(-1);
    }

    LexemeList lexemes = scan(buffer);
    TokenList tokens = evaluate(lexemes);
    SentenceList sentences = parse(tokens);
    compile(outfname, sentences);

    fclose(fptr);

    exit(0);
}

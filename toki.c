// Copyright 2023 Leo James Peckham (padril)

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FLAG DEFINITIONS

/* Lets the program know if command line arguments are going to be provided,
 * or if the default values should be used. Useful for the debugger.
 */
#define REQUIRE_COMMAND_LINE_ARGS 1
#if !(REQUIRE_COMMAND_LINE_ARGS)
    #define DEFAULT_INPUT_FILENAME "./examples/hello_var.toki"
    #define DEFAULT_OUTPUT_FILENAME "hello_var"
#endif

/* When set to 1, will delete generated .asm and .obj files. Turn off if
 * you want to keep the files (e.g. to debug ASM code)
 */
#define DELETE_INTERMEDIATE 1

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
    KEYWORD_SAMA,
} Keyword;

const char *KEYWORDS[] = {
    "e",
    "o",
    "sitelen",
    "toki",
    "li",
    "sama"
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
    LITERAL_STRING,
} Literal;

const char *LITERALS[] = {
    "String",
};

const int LITERAL_COUNT = sizeof(LITERALS) / sizeof(const char *);

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
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_SEPARATOR,
    TOKEN_LITERAL,
} TokenType;

typedef struct Token
{
    TokenType type;
    void *value;
    size_t size;
} Token;

#define TOKEN_DEFAULT ((Token) {\
    .type = TOKEN_NULL, .size = 0, .value = NULL\
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
 * NP holds N (AdjP+)
 * VP holds V (AdvP+) (NP)
 * S  holds (NP) (VP)  (N.B a lone NP or doesn't do anything! could be useful
 *                      for a REPL in the future though...)
 * 
 * These use a lot of short forms because in later code it gets verbose.
 */
typedef struct NounPhrase
{
    Token noun;       // noun
    TokenList adjp;   // adjective phrase
} NounPhrase;

#define NOUNPHRASE_DEFAULT ((NounPhrase) {\
    .noun = TOKEN_DEFAULT,\
    .adjp = TOKENLIST_DEFAULT\
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
           (* (Literal *) t.value == lt);
}

// LEXICAL ANALYSIS

/* Begins the process of Lexical Analysis by generating a list of Lexemes
 * from the input string.
 * (https://en.wikipedia.org/wiki/Lexical_analysis)
 */
LexemeList scan(const char *input)
{
    // Match denotes what the current Lexeme being analyzed could match
    typedef enum Match
    {
        UNKNOWN,
        TEXT,
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
            else
            {
                fprintf(stderr,
                        "Unknown lexeme %s at Ln %d, Col %d.\n",
                        *p, ln, col);
                exit(1);
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
                curr.size = sizeof(Keyword);
                curr.value = malloc(curr.size);
                *(Keyword *) curr.value = (Keyword) kw;
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
            curr.size = sizeof(char) * strlen(lexemes[lex]);
            curr.value = malloc(curr.size);
            strncpy((char *) curr.value, lexemes[lex], curr.size);
            ((char *) curr.value)[curr.size] = '\0';
            tokens[tokens_size] = curr;
            ++tokens_size;
            continue;
        }

        // literals

        if (lexemes[lex][0] == '"')
        {
            curr.type = TOKEN_LITERAL;
            const char *p = lexemes[lex] + 1, *q = lexemes[lex] + 1;
            while (*p != '"')
            {
                ++p;
            }
            int str_size = p - q; // no +1 to get rid of end quote
            // +1 to make room for type byte
            curr.size = sizeof(Literal) + (str_size) * sizeof(char);
            curr.value = malloc(curr.size);
            strncpy((char *) ((Literal *) curr.value + 1), q, str_size);
            ((Literal *) curr.value)[0] = LITERAL_STRING;
            ((char *) curr.value)[curr.size] = '\0';
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
                curr.size = sizeof(Separator);
                curr.value = malloc(curr.size);
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
        PHRASE_E
    } Mode;

    Mode mode = PHRASE_EN;

    Sentence s = SENTENCE_DEFAULT;
    TokenList block;
    block.list = malloc(10 * sizeof(Token));
    block.size = 0;

    Sentence *sl = malloc(50 * sizeof(Sentence));
    size_t size = 0;

    Token *p = input.list;

    // We use modes, and switch between them, updating as we go, to go with
    // the arbitrary way toki pona phrases can be ordered.
    // still need support for something like
    // >>> e "Hello, " o sitelen e "world!".
    for (int i = 0; i < input.size; ++i)
    {
        if (is_keyword(*p, KEYWORD_O) || is_keyword(*p, KEYWORD_LI))
        {
            if (mode == PHRASE_EN)
            {
                if (block.size == 0)
                {
                    s.subj.noun = TOKEN_DEFAULT;
                    s.subj.adjp = TOKENLIST_DEFAULT;
                }
                else if (block.size == 1)
                {
                    s.subj.noun = block.list[0];
                    s.subj.adjp = TOKENLIST_DEFAULT;
                }
                else
                {
                    s.subj.noun = block.list[0];
                    s.subj.adjp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }
            else if (mode == PHRASE_E)
            {
                ;
                // TODO: multiple predicates
                // e.g. jan [VP li moku e kili] [VP li wile e kasi]
            }
            mode = PHRASE_O;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;
        }
        else if (is_keyword(*p, KEYWORD_E))
        {
            if (mode == PHRASE_EN)
            {
                ;
                // ERROR: skipping verb, ungrammatical!
                // wait till nasin kijete flag is implemented!
            }
            else if (mode == PHRASE_O)
            {
                if (block.size == 0)
                {
                    s.pred.verb = TOKEN_DEFAULT;
                    s.pred.advp = TOKENLIST_DEFAULT;
                }
                else if (block.size == 1)
                {
                    s.pred.verb = block.list[0];
                    s.pred.advp = TOKENLIST_DEFAULT;
                }
                else
                {
                    s.pred.verb = block.list[0];
                    s.pred.advp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }
            mode = PHRASE_E;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;
        }
        else if (is_seperator(*p, SEPARATOR_PERIOD))
        {
            if (mode == PHRASE_O)
            {
                if (block.size == 0)
                {
                    s.pred.verb = TOKEN_DEFAULT;
                    s.pred.advp = TOKENLIST_DEFAULT;
                }
                else if (block.size == 1)
                {
                    s.pred.verb = block.list[0];
                    s.pred.advp = TOKENLIST_DEFAULT;
                }
                else
                {
                    s.pred.verb = block.list[0];
                    s.pred.advp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }
            else if (mode == PHRASE_E)
            {
                if (block.size == 0)
                {
                    s.pred.obj.noun = TOKEN_DEFAULT;
                    s.pred.obj.adjp = TOKENLIST_DEFAULT;
                }
                else if (block.size == 1)
                {
                    s.pred.obj.noun = block.list[0];
                    s.pred.obj.adjp = TOKENLIST_DEFAULT;
                }
                else
                {
                    s.pred.obj.noun = block.list[0];
                    s.pred.obj.adjp = (TokenList) {
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }

            mode = PHRASE_EN;  // set PHRASE_EN here for next sentence
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;

            sl[size] = s;
            s = SENTENCE_DEFAULT;
            ++size;
        }
        else  // identifiers, literals
        {
            block.list[block.size] = *p;
            ++block.size;
        }
        ++p;
    }

    if (block.list != NULL)
    {
        free(block.list);
    }

    return (SentenceList) {
        .list = sl,
        .size = size};
}


// COMPILING

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
            exit(1);
            // else if (declarative) { error }
            // could really be a nice succinct switch statement probably
        }
        else if (is_keyword((s.pred.verb), KEYWORD_SITELEN))
        {
            if (s.pred.obj.noun.type == TOKEN_IDENTIFIER)
            {
                // Generate
                char *textline1 = malloc(81 * sizeof(char));
                char *textline2 = malloc(81 * sizeof(char));
                char *textline3 = malloc(81 * sizeof(char));
                sprintf(textline1,
                        "push    VARIABLE_%s",
                        (char *) s.pred.obj.noun.value);
                sprintf(textline2,
                        "call    _printf");
                sprintf(textline3,
                        "add     esp, 4");
                text->lines[text->size] = textline1;
                ++text->size;
                text->lines[text->size] = textline2;
                ++text->size;
                text->lines[text->size] = textline3;
                ++text->size;
            }
            else if (is_literal(s.pred.obj.noun, LITERAL_STRING))
            {
                // Generate data
                char *dataline = malloc(81 * sizeof(char));
                sprintf(dataline,
                        "LITERAL_%d db \"%s\", 0",
                        data->literals,
                        (char *) ((Literal *) s.pred.obj.noun.value + 1)
                        );
                data->lines[data->size] = dataline;
                ++data->size;

                // Generate
                char *textline1 = malloc(81 * sizeof(char));
                char *textline2 = malloc(81 * sizeof(char));
                char *textline3 = malloc(81 * sizeof(char));
                sprintf(textline1,
                        "push    LITERAL_%d",
                        data->literals);
                sprintf(textline2,
                        "call    _printf");
                sprintf(textline3,
                        "add     esp, 4");
                text->lines[text->size] = textline1;
                ++text->size;
                text->lines[text->size] = textline2;
                ++text->size;
                text->lines[text->size] = textline3;
                ++text->size;

                ++data->literals;
            }
            else
            {
                fprintf(
                    stderr,
                    "Incorrect object for verb 'sitelen'.\n");
                exit(1);
            }
        }
    }
    else  // extant subject
    {
        if (s.pred.verb.type == TOKEN_NULL)
        {
            return;  // Do nothing, we have an lone NP
        }
        else if (is_keyword(s.pred.verb, KEYWORD_SAMA))
        {
            if (s.subj.noun.type != TOKEN_IDENTIFIER)
            {
                fprintf(
                    stderr,
                    "Subject must be identifier in assignment.\n");
                exit(1);
            }
            if (s.pred.obj.noun.type == TOKEN_NULL)
            {
                fprintf(
                    stderr,
                    "Assignment statement needs object.\n");
                exit(1);
            }

            // Generate data
            char *dataline = malloc(81 * sizeof(char));
            if (is_literal(s.pred.obj.noun, LITERAL_STRING))
            {
                sprintf(dataline,
                    "VARIABLE_%s db \"%s\", 0",
                    (char *) s.subj.noun.value,
                    (char *) ((Literal *) s.pred.obj.noun.value + 1)
                );
            }
            data->lines[data->size] = dataline;
            ++data->size;
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
            "section .data\n");

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

// DISPLAY
// (for printing debug information, probably only temporary)

void printToken(Token t)
{
    switch (t.type)
    {
    case TOKEN_KEYWORD:
        printf("Type: %d, Value: %s\n",
               t.type,
               KEYWORDS[*(Keyword *)t.value]);
        break;
    case TOKEN_LITERAL:
        switch (*(char *)t.value)
        {
        case LITERAL_STRING:
            printf("Type: %d, Value: \"%s\" : String\n",
                   t.type,
                   (char *) ((Literal *) t.value + 1));
            break;
        }
        break;
    case TOKEN_SEPARATOR:
        printf("Type: %d, Value: %s\n",
               t.type,
               SEPARATORS[*(Separator *)t.value]);
        break;
    }
}

/*
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
}*/

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
        case TOKEN_SEPARATOR:
            printf("Type: %d, Value: %s\n",
                tokens.list[i].type,
                separators[* (Separator*) tokens.list[i].value]);
            break;
    }
}
*/

// MAIN

int main(int argc, char *argv[])
{
#if REQUIRE_COMMAND_LINE_ARGS
    // check number of arguments
    if (argc != 3)
    {
        fprintf(
            stderr,
            "Incorrect number of arguments (expected 2, got %d).\n",
            argc - 1);

        exit(1);
    }
    // asm file
    const char *fname = argv[1];
    const char *outfname = argv[2];
#else
    const char *outfname = DEFAULT_OUTPUT_FILENAME;
    const char *fname = DEFAULT_INPUT_FILENAME;
#endif

    // open file
    FILE *fptr = fopen(fname, "rb");

    // check file existed
    if (fptr == NULL)
    {
        fprintf(
            stderr,
            "File \"%s\" not found.\n",
            fname);

        exit(1);
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
    }

    LexemeList lexemes = scan(buffer);
    TokenList tokens = evaluate(lexemes);
    SentenceList sentences = parse(tokens);
    compile(outfname, sentences);

    fclose(fptr);

    exit(0);
}

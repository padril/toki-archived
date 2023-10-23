// Copyright 2023 Leo James Peckham (padril)

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FLAG DEFINITIONS

/* Lets the program know if command line arguments are going to be provided,
 * or if the default values should be used. Useful for the debugger.
 */
#define REQUIRE_COMMAND_LINE_ARGS 1
#if !(REQUIRE_COMMAND_LINE_ARGS)
    #define DEFAULT_INPUT_FILENAME "hello.c"
    #define DEFAULT_OUTPUT_FILENAME "hello"
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
} Keyword;

const char *KEYWORDS[] = {
    "e",
    "o",
    "sitelen",
    "toki",
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

const Token Token_default = (Token) {
    .type = TOKEN_NULL, .size = 0, .value = NULL
};

typedef struct TokenList
{
    Token *list;
    size_t size;
} TokenList;

const TokenList TokenList_default = (TokenList) {
    .list = NULL,
    .size = 0
};

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
 */
typedef struct NounPhrase
{
    Token noun;
    TokenList adjp;
} NounPhrase;

typedef struct VerbPhrase
{
    Token verb;
    TokenList advp;
    NounPhrase object;
} VerbPhrase;

typedef struct Sentence
{
    NounPhrase subject;
    VerbPhrase predicate;
    // tense? aspect? mood? li vs o distinction might go here later
} Sentence;

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

// LEXICAL ANALYSIS

/* Begins the process of Lexical Analysis by generating a list of Lexemes
 * from the input string.
 * (https://en.wikipedia.org/wiki/Lexical_analysis)
 */
LexemeList scan(const char *input)
{
    typedef enum Mode
    {
        UNKNOWN,
        TEXT,
        STRING,
        END,
    } Mode;

    Lexeme *lexemes = malloc(1000 * sizeof(Lexeme));
    int lexemes_size = 0;
    Lexeme lex;
    Mode mode = UNKNOWN;
    const char *p = input, *q = input;
    int ln = 0;
    int col = 0;

    // parse tokens
    while (*(p - 1) != '\0')  // use p - 1 in case file ends early
    {
        if (mode == UNKNOWN)
        {
            if (isspace(*p) || iscntrl(*p))
            {
                ; // do nothing!
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
            if (isspace(*p) || iscntrl(*p))
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

                // clean
                mode = UNKNOWN;
                q = p + 1; // p will be incremented by one later
            }
            else if (isalnum(*p))
            {
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
                int str_size = p - q + 1; // plut one to include close quote
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

    return (LexemeList){.list = lexemes, .size = lexemes_size};
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
        TokenType type = TOKEN_NULL;
        void *value = NULL;
        size_t size = 0;

        // keyword and identifier
        // TODO: replace with binsearch, or hash

        for (int kw = 0; kw < KEYWORD_COUNT; ++kw)
        {
            if (!strcmp(lexemes[lex], KEYWORDS[kw]))
            {
                type = TOKEN_KEYWORD;
                size = sizeof(Keyword);
                value = malloc(size);
                *(Keyword *)value = (Keyword)kw;
                break;
            }
        }
        if (type != TOKEN_NULL)
        {
            tokens[tokens_size] = (Token){
                .type = type,
                .value = value,
                .size = size};
            ++tokens_size;
            continue;
        }

        // literals

        if (lexemes[lex][0] == '"')
        {
            type = TOKEN_LITERAL;
            const char *p = lexemes[lex] + 1, *q = lexemes[lex] + 1;
            while (*p != '"')
            {
                ++p;
            }
            int str_size = p - q; // no +1 to get rid of end quote
            // +1 to make room for type byte
            size = sizeof(Literal) + (str_size) * sizeof(char);
            value = malloc(size);
            strncpy((char *) ((Literal *) value + 1), q, str_size);
            ((Literal *) value)[0] = LITERAL_STRING;
            ((char *)value)[size] = '\0';
            tokens[tokens_size] = (Token){
                .type = type,
                .value = value,
                .size = size};
            ++tokens_size;
            continue;
        }

        // separators

        for (int sp = 0; sp < SEPARATOR_COUNT; ++sp)
        {
            if (!strcmp(lexemes[lex], SEPARATORS[sp]))
            {
                type = TOKEN_SEPARATOR;
                size = sizeof(Separator);
                value = malloc(size);
                *(Separator *)value = (Separator)sp;
                break;
            }
        }
        if (type != TOKEN_NULL)
        {
            tokens[tokens_size] = (Token){
                .type = type,
                .value = value,
                .size = size};
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

    Sentence s;
    TokenList block;
    block.list = NULL;
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
        if (p->type == TOKEN_KEYWORD &&
            *(Keyword *)p->value == KEYWORD_O)
        {
            if (mode == PHRASE_EN)
            {
                if (block.size == 0)
                {
                    s.subject.noun = Token_default;
                    s.subject.adjp = TokenList_default;
                }
                else if (block.size == 1)
                {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = TokenList_default;
                }
                else
                {
                    s.subject.noun = block.list[0];
                    s.subject.adjp = (TokenList){
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
        else if (p->type == TOKEN_KEYWORD &&
                 *(Keyword *)p->value == KEYWORD_E)
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
                    s.predicate.verb = Token_default;
                    s.predicate.advp = TokenList_default;
                }
                else if (block.size == 1)
                {
                    s.predicate.verb = block.list[0];
                    s.predicate.advp = TokenList_default;
                }
                else
                {
                    s.predicate.verb = block.list[0];
                    s.predicate.advp = (TokenList){
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }
            mode = PHRASE_E;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;
        }
        else if (p->type == TOKEN_SEPARATOR &&
                 *(Separator *)p->value == SEPARATOR_PERIOD)
        {

            if (mode == PHRASE_E)
            {
                if (block.size == 0)
                {
                    s.predicate.object.noun = Token_default;
                    s.predicate.object.adjp = TokenList_default;
                }
                else if (block.size == 1)
                {
                    s.predicate.object.noun = block.list[0];
                    s.predicate.object.adjp = TokenList_default;
                }
                else
                {
                    s.predicate.object.noun = block.list[0];
                    s.predicate.object.adjp = (TokenList){
                        .list = block.list + 1,
                        .size = block.size - 1};
                }
            }

            mode == PHRASE_EN;
            block.list = malloc(10 * sizeof(Token));
            block.size = 0;

            // definitely need to reset s at some point here
            sl[size] = s;
            ++size;
        }
        else
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

    return (SentenceList){
        .list = sl,
        .size = size};
}


// COMPILING

/* Inputs data retrieved from `Sentence` into provided `SectionData` and
 * `SectionText` pointers by generating ASM.
 */
void compile_sentence(Sentence s, SectionData *data, SectionText *text)
{
    if (s.subject.noun.type == TOKEN_NULL)
    {
        // TODO: better error handling
        if (s.predicate.verb.type == TOKEN_NULL)
        {
            fprintf(
                stderr,
                "Missing verb in sentence.\n");
            exit(1);
            // else if (declarative) { error }
            // could really be a nice succinct switch statement probably
        }
        else if (s.predicate.verb.type == TOKEN_KEYWORD &&
                 (*(Keyword *)s.predicate.verb.value == KEYWORD_SITELEN))
        {
            if (s.predicate.object.noun.type == TOKEN_LITERAL &&
                (*(Literal *)s.predicate.object.noun.value == LITERAL_STRING))
            {
                // Generate data
                char *dataline = malloc(81 * sizeof(char));
                sprintf(dataline,
                        "LITERAL_%d db \"%s\", 0",
                        data->literals,
                        (char *) ((Literal *) s.predicate.object.noun.value + 1) );
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
    const char *fname = DEFAULT_OUTPUT_FILENAME;
#endif

    // open file
    FILE *fptr = fopen(fname, "r");

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
    char str[101];
    fgets(str, 100, fptr);
    str[100] = '\0';

    LexemeList lexemes = scan(str);
    TokenList tokens = evaluate(lexemes);
    SentenceList sentences = parse(tokens);
    compile(outfname, sentences);

    fclose(fptr);

    exit(0);
}

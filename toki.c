#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

const char* keywords[] = {
    "o",
    "sitelen",
    "toki",
    0
};

typedef enum Lexeme
{
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_SEPERATOR,
    TOKEN_O,
    TOKEN_LITERAL,
    TOKEN_SITELEN,
    TOKEN_END_PHRASE,
    TOKEN_END_TOKENS,
} Lexeme;

typedef struct Token
{
    Lexeme type;
    const char* value;
} Token;

Token* tokenize(const char* input)
{
    typedef enum Mode {
        UNKNOWN,
        TEXT,
        STRING,
        END,
    } Mode;

    Token* tokens = malloc(1000 * sizeof(Token));
    int tokens_pos = 0;
    Token curr;
    Mode mode = UNKNOWN;
    const char *p = input, *q = input;
    int ln = 0;
    int col = 0;

    // parse tokens
    while (*(p - 1) != '\0') {
        if (mode == UNKNOWN) {
            if (isspace(*p) || iscntrl(*p)) {
                // do nothing!
            } else if (isalpha(*p)) {
                mode = TEXT;
            } else {
                fprintf(stderr,
                        "Unknown token %s at Ln %d, Col %d.\n",
                        *p, ln, col);
                exit(1);
            }
        } else if (mode == TEXT) {
            if (isspace(*p) || iscntrl(*p)) {
                // construct string
                int size = p - q;
                char* text = malloc((size + 1) * sizeof(char));
                strncpy(text, q, size);
                text[size] = '\0';

                // construct token
                curr.type = TOKEN_IDENTIFIER;
                curr.value = text;

                // TODO: replace with binsearch
                int i = 0;
                while (keywords[i]) {
                    const char* x = keywords[i];
                    if (!strcmp(text, keywords[i])) {
                        curr.type = TOKEN_KEYWORD;
                        break;
                    }
                    ++i;
                }

                // add token
                tokens[tokens_pos] = curr;
                ++tokens_pos;

                // clean
                mode = UNKNOWN;
                q = p + 1;  // p will be incremented by one later
            } else if (isalnum(*p)) {
            }
        }

        ++p;
    }

    curr.type = TOKEN_END_TOKENS;
    curr.value = NULL;
    tokens[tokens_pos] = curr;

    return tokens;
}


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

    Token* tokens = tokenize(str);

    int i = 0;
    while (tokens[i].type != TOKEN_END_TOKENS) {
        printf("Type: %d, Value: %s\n",
               tokens[i].type,
               tokens[i].value);
        ++i;
    }

    exit(0);
}

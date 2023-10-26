# TODO

## Next Refactor/Cleanup Milestone

- [ ] Functioning variables.toki
    - [ ] Variables
    - [ ] Numbers
    - [ ] Operators
        - [ ] Plus operator
            - [ ] On numbers and strings
    - [ ] Keyword `toki` (for converting to )
- [ ] Functioning fizzbuzz.toki
    - [ ] Numbers
    - [ ] Modulus operator
    - [ ] Loops
- [ ] Implement unit testing
    - [ ] Add unit tests
- [ ] Implement end-to-end testing
    - [ ] Add end to end-to-end tests
- [ ] Implement compile-time errors
    - [ ] Add compile-time errors where they need to be

## Short term goals

### Functionality

- [ ] Implement run-time exceptions
    - [ ] Catchable
        - [ ] Arithmetic, IO, user-defined
    - [ ] Uncatchable
        - [ ] Segfaults, memory errors, stack overflows
- [ ] Should support multiple ASM architectures and be more portable. Maybe a
flag in the future or some type of `#ifdef __WIN32__` type of thing.
- [ ] Just add more keywords and functionality
- [ ] Still need to figure out how to do boolean "ands", since "en" will be used
to conjoin subjects for APL like array processing shit.
- [ ] Add functionality for "la" phrases, clauses, subphrases etc. to be
contained in `Sentence` (or `ComplementPhrase`?)
- [ ] Do we want empty Sentences (`.`, `o sitelen e "Hello. .`, etc.) to be
syntactically valid? Should S hold (NP) (VP) or hold {(NP) VP, NP}? Could be
cute syntactic sugar for `...` or something?
- [ ] TAM / *li* vs *o* distinction might go in `Sentence` later.
- [ ] Some sort of preprocessor will be useful for comments, before the
scan step.

### Style / Improvements

- [ ] Compiling is overly verbose, the inputing of ASM should be shortened into
a function.
- [ ] Create some type of simple static analysis to ensure enum/string list
pairs are correctly ordered.
- [ ] Parsing is overly complicated giving absence of nasin kijete, it should be
shortened down. **PRIORITIZE THIS, ITS REALLY BAD**
- [ ] Literal Tokens kind of suck right now, the first part of "value"
represents type information, and then the rest represents the actual value. I
feel like this could either be simpler, use less memory, or something else if
just done better.
- [ ] Remove `Token_default` and `TokenList_default` by using better logic in
the related sections of the code.
- [ ] `compile_sentence()` could probably return a struct holding `SectionData`
and `SectionText`
- [ ] Should `compile()` really do all the steps inside of it?
- [ ] DISPLAY section should really be concerned with more toString() type
functions, and maybe creating a log file.

## Long term goals

- [ ] Start documentation
- [ ] README.md
- [ ] Add "nasin kijete" (free word order) flag.
- [ ] Currently `SectionData` and `SectionText` can just keep lines as strings,
but in the future it might be better to keep more abstract data to be dealt with
in `write()`. This might allow type inference if we want that?
- [ ] Improve abstraction in `SectionText`. Maybe subdivide it into each
seperate label? That could make it easier to organize readable ASM and include
library functions and default parameters in the future.

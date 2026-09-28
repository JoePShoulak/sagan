---
title: Grammar and syntax
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Grammar and syntax
The implementation is authoritative for the current concrete grammar. The
simplified productions below describe every implemented declaration and
statement family; the expression precedence table completes the expression
grammar.

**Implemented:** the tokenizer supplies tokens and newline boundaries. The
parser implements `let` declarations, blocks, assignment and expression
statements, `if`/`else`, named block-bodied functions with typed parameters and
return annotations, `for`/`in`, `while`, and `until` loops, loop control,
returns, yields, block-bodied `match`/`case`, primary and string expressions, the
`hope`/`unless`/`finally`/`scream` exception grammar, settled precedence table,
postfix chains, collection literals, top-level face/class/enum declarations,
module/import/export declarations, and declaration documentation.

**Concrete conventions:** declarations use words such as `let`, `fun`, `face`,
`class`, and `enum`; braces delimit bodies; newlines normally terminate
statements; `=>` introduces expression bodies; and collection-like delimiters
have the forms shown in the language tour.

The implemented collection grammar is equivalent to this simplified notation:

```text
program     := newline* (documented_declaration newline+)* documented_declaration? newline*
declaration := let_declaration | function_declaration | face_declaration
             | class_declaration | enum_declaration | module_declaration
             | import_declaration | export_declaration
statement   := let_declaration | assignment_statement | expression_statement
             | if_statement | for_statement | condition_loop | loop_control
             | return_statement | yield_statement | match_statement
             | hope_statement | scream_statement
let_declaration := "let" identifier (":" type)? ("=" expression)?
array       := "[" (expression ("," expression)* ","?)? "]"
dictionary  := "{" (dictionary_entry ("," dictionary_entry)* ","?)? "}"
dictionary_entry := expression ":" expression | spread
vector      := "<" expression "," expression ("," expression)* ","? ">"
coordinate  := "(" expression "," expression ("," expression)* ","? ")"
spread      := "..." expression
group       := "(" expression ")"
block       := "{" newline* (statement newline+)* statement? newline* "}"
if_statement := "if" expression block ("else" (if_statement | block))?
for_statement := "for" identifier "in" expression block
condition_loop := ("while" | "until") expression block
loop_control := "break" | "continue"
return_statement := "return" expression?
yield_statement := "yield" expression?
match_statement := "match" expression "{" newline* match_case
                   (newline+ match_case)* newline* "}"
match_case  := "case" (expression | "else") block
hope_statement := "hope" block ("unless" expression block)* ("finally" block)?
scream_statement := "scream" expression
assignment_statement := expression ("=" | "+=" | "-=" | "*=" | "/=" | "%=" | "^=") expression
expression_statement := expression
function_declaration := "fun" identifier "(" parameters? ")" (":" type)? function_body
function_body := block | "=>" expression
lambda := "fun" "(" parameters? ")" (":" type)? "=>" expression
method_signature := "fun" identifier "(" parameters? ")" (":" type)?
face_declaration := "face" identifier composition? "{" face_member* "}"
face_member := method_signature function_body?
class_declaration := "class" identifier composition? "{" class_member* "}"
class_member := let_declaration | "fun" "."? identifier "(" parameters? ")" (":" type)? function_body
composition := ("is" | "has") identifier ("," identifier)*
enum_declaration := "enum" identifier "{" enum_members? "}"
enum_members := documented_enum_member ((newline+ | ",") documented_enum_member)* ","?
documented_enum_member := documentation_comment* identifier
module_declaration := "module" identifier
import_declaration := "import" identifier ("from" identifier)? ("as" identifier)?
export_declaration := "export" identifier ("as" identifier)?
documented_declaration := documentation_comment* declaration
parameters  := parameter ("," parameter)* ","?
parameter   := identifier (":" type)?
```

Dictionary-versus-block interpretation is grammatical: `{...}` in an
expression position is a dictionary, while a brace following a statement form
that requires a body will be a block. Vector-versus-comparison interpretation
is likewise positional. A `<` where a primary expression must begin opens a
vector; after a left operand it is a comparison. At a vector element's top
level, `>` closes the vector, so a greater-than comparison there must be
parenthesized.

Blocks may be empty. Statements inside them are newline-separated; semicolons do
not terminate ordinary statements. `else`, `unless`, and `finally` currently
follow the preceding `}` without an intervening logical newline.

The program root accepts declarations only. Executable statements and control
flow belong inside function bodies. A program may have at most one module
declaration, and it must be the first declaration when present.

Documentation comments occupy their own logical lines and attach to the next
declaration. They are accepted on top-level declarations, face/class methods,
class fields, local `let` declarations, and enum members. They cannot attach to
executable statements. Orphaned documentation comments are syntax errors.

**Semantic questions:** validation of returned and yielded values,
match-pattern binding, type tests,
destructuring, and exhaustiveness are semantic questions rather than established
behavior.

The parser currently stops after the first syntax error. Multi-error recovery
is a future diagnostic enhancement, not an omitted grammar production.

Implemented parser demos are accepted parser input but are not yet executable;
semantic analysis and code generation remain future stages.

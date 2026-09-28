# Step 1
## Define the Language
- Make sure you know what it is and what it's for

# Step 2
## Tokenizer
- In Lex.cpp (and .hpp) define my own token ids and token text (the id is just an identifier and the text is what's in the quotes)
- We get token errors

# Step 3
## Parser (Syntax tree)
- Turning tokens into actual executable syntax trees
- Form and structure, logic flow, etc
- Here we get syntax errors

# Step 4
## Semantic Analysis 
- The syntax is valid, run through the tree and check it's actually valid (no duplicate names, etc)
- Semantic errors, type conflics, etc. 

# Step 5
## Code generation (translation)
- Actually turn the syntax tree into the the relevant executable code to be compiled and therefore run

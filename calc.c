// Berkay Samet Aygunduz 251ADB082
// https://github.com/mohammond/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: geeksforgeeks.org; learn-c.org; OpenAI ChatGPT; GNU C Library Manual.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

// A wrong command is a program error, so the message goes to stderr.
// Returning 1 gives the non-zero exit code required by the task.
int usage(void)
{
    fprintf(stderr, "Usage: calc [-d DIR] [-o OUTDIR] [input.txt]\n");
    return 1;
}

// Prompt: "my input file can have very long lines how can i read it with malloc realloc and fgetc also i need to skip empty lines and lines starting with # but still count every character including comments and newline for error positions can you show small example"
// Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964
// Additional reading: https://www.learn-c.org/en/Dynamic_allocation

// The buffer doubles when full, so large files need fewer allocations.
// The parser uses the same text without copying each line or token.
// Keeping the original text means comments and newlines still count.
char *read_file(char *file, size_t *length)
{
    FILE *input = fopen(file, "rb");

    if (input == NULL)
    {
        perror(file);
        return NULL;
    }

    size_t capacity = 4096;
    char *text = malloc(capacity);
    *length = 0;

    if (text == NULL)
    {
        fprintf(stderr, "Not enough memory\n");
        fclose(input);
        return NULL;
    }

    int c;

    while ((c = fgetc(input)) != EOF)
    {
        if (*length == capacity - 1)
        {
            // Check before doubling so the memory size cannot wrap around.
            if (capacity > ((size_t)-1) / 2)
            {
                fprintf(stderr, "File is too large for memory\n");
                free(text);
                fclose(input);
                return NULL;
            }

            char *temp = realloc(text, capacity * 2);

            if (temp == NULL)
            {
                fprintf(stderr, "Not enough memory\n");
                free(text);
                fclose(input);
                return NULL;
            }

            text = temp;
            capacity = capacity * 2;
        }

        text[*length] = (char)c;
        (*length)++;
    }

    if (ferror(input))
    {
        perror(file);
        free(text);
        fclose(input);
        return NULL;
    }

    if (fclose(input) != 0)
    {
        perror(file);
        free(text);
        return NULL;
    }

    text[*length] = '\0';
    return text;
}

// Prompt: "how can i handle integers and floats, read txt files from a folder and save results in c can you show small examples"
// Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964

// The type is kept with the value so integer division still works as in C.
// A calculation uses double only when at least one operand is a float.
typedef struct
{
    int is_float;
    long long integer;
    double real;
} Value;

// Indexes use the original file, including comments and newlines.
// Adding one gives the positions required by the task.
char *text_data;
size_t pos;
size_t line_end;
size_t error_pos;

enum { TOKEN_END, TOKEN_NUMBER, TOKEN_POWER, TOKEN_INVALID };

int token;
Value token_value;
size_t token_pos;

Value parse_expression(void);

// Only spaces and tabs are skipped because a newline ends an expression.
void skip_spaces(void)
{
    while (pos < line_end &&
           (text_data[pos] == ' ' || text_data[pos] == '\t'))
    {
        pos++;
    }
}

// Prompt: "how can i make tokenizer and parser in c with operator priority and error positions"
// Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964

// Saving the token start lets errors use it after the parser moves forward.
// Numbers are checked before conversion so .5, 5. and 1.5e3 are not accepted.
void next_token(void)
{
    skip_spaces();
    token_pos = pos;

    if (pos == line_end)
    {
        token = TOKEN_END;
        return;
    }

    if (text_data[pos] >= '0' && text_data[pos] <= '9')
    {
        token = TOKEN_NUMBER;
        token_value = (Value){0, 0, 0.0};

        while (pos < line_end &&
               text_data[pos] >= '0' && text_data[pos] <= '9')
        {
            pos++;
        }

        if (pos < line_end && text_data[pos] == '.')
        {
            size_t dot = pos;
            pos++;

            if (pos == line_end ||
                text_data[pos] < '0' || text_data[pos] > '9')
            {
                token = TOKEN_INVALID;
                error_pos = dot + 1;
                return;
            }

            token_value.is_float = 1;

            while (pos < line_end &&
                   text_data[pos] >= '0' && text_data[pos] <= '9')
            {
                pos++;
            }
        }

        if (token_value.is_float)
        {
            // Stop strtod at the checked number, then restore the character.
            // This avoids copying tokens and stops it from reading an exponent.
            char saved = text_data[pos];
            text_data[pos] = '\0';
            token_value.real = strtod(text_data + token_pos, NULL);
            text_data[pos] = saved;

            if (!isfinite(token_value.real))
            {
                token = TOKEN_INVALID;
                error_pos = token_pos + 1;
            }
        }
        else
        {
            // Integer tokens stay long long so large integers keep their value.
            for (size_t i = token_pos; i < pos; i++)
            {
                token_value.integer = token_value.integer * 10
                                      + (text_data[i] - '0');
            }
        }

        return;
    }

    char c = text_data[pos];
    pos++;

    if (c == '*' && pos < line_end && text_data[pos] == '*')
    {
        pos++;
        token = TOKEN_POWER;
    }
    else if (c == '+' || c == '-' || c == '*' || c == '/' ||
             c == '(' || c == ')')
    {
        token = c;
    }
    else
    {
        token = TOKEN_INVALID;
        error_pos = token_pos + 1;
    }
}

// Conversion happens only when a float operation needs it.
// Integer-only calculations do not lose precision through double.
double as_double(Value value)
{
    if (value.is_float)
    {
        return value.real;
    }

    return (double)value.integer;
}

// Checked pow for floating-point powers and isfinite for invalid results.
// https://sourceware.org/glibc/manual/latest/html_node/Exponents-and-Logarithms.html
// https://sourceware.org/glibc/manual/latest/html_node/Floating-Point-Classes.html

// Integer operands keep integer arithmetic; either float makes a double result.
// Zero division uses the right operand's start, including an opening bracket.
// A non-finite float result uses the operator because that operation failed.
Value calculate(Value left, Value right, int op,
                size_t operator_start, size_t right_start)
{
    Value result = {0, 0, 0.0};

    if (op == '/' &&
        ((right.is_float && right.real == 0.0) ||
         (!right.is_float && right.integer == 0)))
    {
        error_pos = right_start + 1;
        return result;
    }

    if (left.is_float || right.is_float)
    {
        double a = as_double(left);
        double b = as_double(right);
        result.is_float = 1;

        if (op == '+')
        {
            result.real = a + b;
        }
        else if (op == '-')
        {
            result.real = a - b;
        }
        else if (op == '*')
        {
            result.real = a * b;
        }
        else if (op == '/')
        {
            result.real = a / b;
        }
        else
        {
            result.real = pow(a, b);
        }

        if (!isfinite(result.real))
        {
            error_pos = operator_start + 1;
        }

        return result;
    }

    if (op == '+')
    {
        result.integer = left.integer + right.integer;
    }
    else if (op == '-')
    {
        result.integer = left.integer - right.integer;
    }
    else if (op == '*')
    {
        result.integer = left.integer * right.integer;
    }
    else if (op == '/')
    {
        result.integer = left.integer / right.integer;
    }
    else
    {
        if (right.integer < 0)
        {
            error_pos = right_start + 1;
            return result;
        }

        long long base = left.integer;
        long long exponent = right.integer;
        result.integer = 1;

        // Squaring the base needs fewer steps than multiplying exponent times.
        // Starting with 1 also handles x**0, including 0**0.
        while (exponent > 0)
        {
            if (exponent % 2 == 1)
            {
                result.integer = result.integer * base;
            }

            exponent = exponent / 2;

            // Skip the last unused square, which could overflow needlessly.
            if (exponent > 0)
            {
                base = base * base;
            }
        }
    }

    return result;
}

// Brackets call the full expression parser so their contents are grouped.
// Operators cannot start a primary because unary operators are not allowed.
Value parse_primary(void)
{
    Value value = {0, 0, 0.0};

    if (error_pos != 0)
    {
        return value;
    }

    if (token == TOKEN_NUMBER)
    {
        value = token_value;
        next_token();
        return value;
    }

    if (token == '(')
    {
        next_token();
        value = parse_expression();

        if (error_pos != 0)
        {
            return value;
        }

        if (token != ')')
        {
            error_pos = token_pos + 1;
            return value;
        }

        next_token();
        return value;
    }

    // At line end this is the newline position, or file length + 1 at EOF.
    error_pos = token_pos + 1;
    return value;
}

// Calling parse_power for the exponent makes 2**3**2 mean 2**(3**2).
// Saved positions are needed because reading the exponent moves the tokenizer.
Value parse_power(void)
{
    Value value = parse_primary();

    if (error_pos != 0)
    {
        return value;
    }

    if (token == TOKEN_POWER)
    {
        size_t operator_start = token_pos;
        next_token();
        size_t right_start = token_pos;
        Value right = parse_power();

        if (error_pos == 0)
        {
            value = calculate(value, right, TOKEN_POWER,
                              operator_start, right_start);
        }
    }

    return value;
}

// Powers are read first so ** has higher priority than * and /.
// The loop handles * and / from left to right.
Value parse_term(void)
{
    Value value = parse_power();

    while (error_pos == 0 && (token == '*' || token == '/'))
    {
        int op = token;
        size_t operator_start = token_pos;
        next_token();
        size_t right_start = token_pos;
        Value right = parse_power();

        if (error_pos == 0)
        {
            value = calculate(value, right, op, operator_start, right_start);
        }
    }

    return value;
}

// Terms are read first so * and / have higher priority than + and -.
// The loop makes subtraction and addition work from left to right.
Value parse_expression(void)
{
    Value value = parse_term();

    while (error_pos == 0 && (token == '+' || token == '-'))
    {
        int op = token;
        size_t operator_start = token_pos;
        next_token();
        size_t right_start = token_pos;
        Value right = parse_term();

        if (error_pos == 0)
        {
            value = calculate(value, right, op, operator_start, right_start);
        }
    }

    return value;
}

// The first pass checks all lines so a later error leaves no partial results.
// The second pass writes answers without keeping a separate result list.
// Both passes use the same text, so large lines are not copied repeatedly.
int evaluate_text(char *text, size_t length, FILE *output)
{
    text_data = text;

    for (int pass = 0; pass < 2; pass++)
    {
        pos = 0;
        error_pos = 0;

        while (pos < length)
        {
            line_end = pos;

            while (line_end < length && text_data[line_end] != '\n')
            {
                line_end++;
            }

            skip_spaces();

            // Skipped lines stay in the text so their characters still count.
            if (pos < line_end && text_data[pos] != '#')
            {
                next_token();
                Value result = parse_expression();

                if (error_pos == 0 && token != TOKEN_END)
                {
                    // This also reports an extra ')' at its own position.
                    error_pos = token_pos + 1;
                }

                if (error_pos != 0)
                {
                    if (fprintf(output, "ERROR:%zu\n", error_pos) < 0)
                    {
                        perror("Output");
                        return 1;
                    }

                    // An expression error is a normal result, so return zero.
                    return 0;
                }

                if (pass == 1)
                {
                    int written;

                    if (result.is_float)
                    {
                        written = fprintf(output, "%.15g\n", result.real);
                    }
                    else
                    {
                        written = fprintf(output, "%lld\n", result.integer);
                    }

                    if (written < 0)
                    {
                        perror("Output");
                        return 1;
                    }
                }
            }

            pos = line_end;

            if (pos < length)
            {
                pos++;
            }
        }
    }

    return 0;
}

// Prompt: "how can i handle integers and floats, read txt files from a folder and save results in c can you show small examples"
// Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964

// Adds my name and ID to follow the required output file format.
// Input is read before output is opened, so writing cannot change the parsed text.
// Replacing output removes results left from an earlier run.
int process_file(char *file, char *out)
{
    char base[4096];
    char output_path[4096];

    char *name = strrchr(file, '/');

    if (name == NULL)
    {
        name = file;
    }
    else
    {
        name++;
    }

    if (strlen(name) >= sizeof(base))
    {
        fprintf(stderr, "Input file name is too long\n");
        return 1;
    }

    strcpy(base, name);

    size_t length = strlen(base);

    if (length >= 4 && strcmp(base + length - 4, ".txt") == 0)
    {
        base[length - 4] = '\0';
    }

    int count = snprintf(output_path, sizeof(output_path),
                         "%s/%s_berkay_samet_aygunduz_251ADB082.txt",
                         out, base);

    if (count < 0 || (size_t)count >= sizeof(output_path))
    {
        fprintf(stderr, "Output path is too long\n");
        return 1;
    }

    char *text = read_file(file, &length);

    if (text == NULL)
    {
        return 1;
    }

    FILE *output = fopen(output_path, "wb");

    if (output == NULL)
    {
        perror(output_path);
        free(text);
        return 1;
    }

    int status = evaluate_text(text, length, output);

    // Buffered writes can fail when closing, so fclose is checked too.
    if (fclose(output) != 0)
    {
        perror(output_path);
        status = 1;
    }

    free(text);
    return status;
}

// Prompt: "how can i handle integers and floats, read txt files from a folder and save results in c can you show small examples"
// Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964

// Documentation checked during review: GNU C Library Manual.
// Checked readdir error handling and why file names need their own copies.
// https://sourceware.org/glibc/manual/2.42/html_node/Reading_002fClosing-Directory.html
//
// Additional reading: GeeksforGeeks, listing files in a directory.
// https://www.geeksforgeeks.org/c/c-program-list-files-sub-directories-directory/

// Keeps only regular .txt files directly inside the input folder.
// Saves paths before writing so new output files are not processed again.
// The list grows when needed, so there is no fixed limit on file count.
int process_directory(char *dir, char *out)
{
    DIR *folder = opendir(dir);

    if (folder == NULL)
    {
        perror(dir);
        return 1;
    }

    char **files = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int status = 0;

    while (1)
    {
        // Reset errno so normal directory end is different from a read error.
        errno = 0;
        struct dirent *entry = readdir(folder);

        if (entry == NULL)
        {
            if (errno != 0)
            {
                perror(dir);
                status = 1;
            }

            break;
        }

        size_t length = strlen(entry->d_name);

        if (length < 4 ||
            strcmp(entry->d_name + length - 4, ".txt") != 0)
        {
            continue;
        }

        char path[4096];

        int size = snprintf(path, sizeof(path),
                            "%s/%s", dir, entry->d_name);

        if (size < 0 || (size_t)size >= sizeof(path))
        {
            fprintf(stderr, "Input path is too long\n");
            status = 1;
            break;
        }

        struct stat info;

        if (stat(path, &info) != 0)
        {
            perror(path);
            status = 1;
            break;
        }

        // A folder may also end with .txt, so its type must be checked.
        if (!S_ISREG(info.st_mode))
        {
            continue;
        }

        if (count == capacity)
        {
            size_t new_capacity = 16;

            if (capacity != 0)
            {
                if (capacity > ((size_t)-1) / 2 / sizeof(*files))
                {
                    fprintf(stderr, "Too many input files\n");
                    status = 1;
                    break;
                }

                new_capacity = capacity * 2;
            }

            char **temp = realloc(files,
                                  new_capacity * sizeof(*files));

            if (temp == NULL)
            {
                fprintf(stderr, "Not enough memory\n");
                status = 1;
                break;
            }

            files = temp;
            capacity = new_capacity;
        }

        // readdir reuses its memory, so each path needs its own copy.
        files[count] = malloc(strlen(path) + 1);

        if (files[count] == NULL)
        {
            fprintf(stderr, "Not enough memory\n");
            status = 1;
            break;
        }

        strcpy(files[count], path);
        count++;
    }

    if (closedir(folder) != 0)
    {
        perror(dir);
        status = 1;
    }

    for (size_t i = 0; i < count && status == 0; i++)
    {
        status = process_file(files[i], out);
    }

    // Free all saved paths even if one file could not be processed.
    for (size_t i = 0; i < count; i++)
    {
        free(files[i]);
    }

    free(files);
    return status;
}

// One input mode is accepted so a file and a directory cannot conflict.
// Without -o, the output folder uses the input name and is created here.
// Existing folders are accepted, but an existing regular file is an error.
int main(int argc, char *argv[])
{
    char *file = NULL;
    char *dir = NULL;
    char *out = NULL;

    // Prompt: "i want to make arithmetic parser in c my program need to read input filename and -d or --dir for input folder and -o or --output-dir for output folder how can i use argc argv and strcmp for this can you explain with small example"
    // Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--dir") == 0)
        {
            if (i + 1 >= argc || argv[i + 1][0] == '-')
            {
                return usage();
            }

            i++;
            dir = argv[i];
        }
        else if (strcmp(argv[i], "-o") == 0 ||
                 strcmp(argv[i], "--output-dir") == 0)
        {
            if (i + 1 >= argc || argv[i + 1][0] == '-')
            {
                return usage();
            }

            i++;
            out = argv[i];
        }
        else if (argv[i][0] == '-')
        {
            return usage();
        }
        else
        {
            if (file != NULL)
            {
                return usage();
            }

            file = argv[i];
        }
    }

    if (file == NULL && dir == NULL)
    {
        return usage();
    }

    if (file != NULL && dir != NULL)
    {
        return usage();
    }

    struct stat info;

    if (file != NULL)
    {
        if (stat(file, &info) != 0 || !S_ISREG(info.st_mode))
        {
            fprintf(stderr, "Invalid input file: %s\n", file);
            return 1;
        }
    }
    else
    {
        DIR *folder = opendir(dir);

        if (folder == NULL)
        {
            perror(dir);
            return 1;
        }

        if (closedir(folder) != 0)
        {
            perror(dir);
            return 1;
        }
    }

    char base[4096];
    char default_out[4096];

    // Prompt: "if -o is missing i need to create output folder using input filename without .txt and my name and student id for -d i need to use folder name how can i do this with strlen strrchr and snprintf also folder path can end with /"
    // Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964
    if (out == NULL)
    {
        char *source = file;

        if (dir != NULL)
        {
            source = dir;
        }

        if (strlen(source) >= sizeof(base))
        {
            fprintf(stderr, "Input path is too long\n");
            return 1;
        }

        strcpy(base, source);

        size_t length = strlen(base);

        // labs/ and labs should produce the same default folder name.
        while (length > 1 && base[length - 1] == '/')
        {
            length--;
            base[length] = '\0';
        }

        char *name = strrchr(base, '/');

        if (name == NULL)
        {
            name = base;
        }
        else
        {
            name++;
        }

        length = strlen(name);

        if (file != NULL && length >= 4)
        {
            if (strcmp(name + length - 4, ".txt") == 0)
            {
                name[length - 4] = '\0';
            }
        }

        if (dir != NULL && name[0] == '\0')
        {
            name = "root";
        }

        int count = snprintf(default_out, sizeof(default_out),
                             "%s_berkay_samet_251ADB082", name);

        if (count < 0 || (size_t)count >= sizeof(default_out))
        {
            fprintf(stderr, "Output directory name is too long\n");
            return 1;
        }

        out = default_out;
    }

    // Prompt: "bro how can i use fopen to check input file and opendir to check input folder in c also how can i create output folder with mkdir and use it if it already exists can you show small example with error checks"
    // Link: https://chatgpt.com/share/6ae0ecea-dd94-83eb-ac86-16d7f3797964
    if (mkdir(out, 0755) != 0 && errno != EEXIST)
    {
        perror(out);
        return 1;
    }

    if (stat(out, &info) != 0 || !S_ISDIR(info.st_mode))
    {
        fprintf(stderr, "Invalid output directory: %s\n", out);
        return 1;
    }

    if (file != NULL)
    {
        return process_file(file, out);
    }

    return process_directory(dir, out);
}
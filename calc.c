// Berkay Samet Aygunduz 251ADB082
// https://github.com/mohammond/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: geeksforgeeks.org, https://www.learn-c.org/, Chatgpt 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// A wrong command is a program error, so the message goes to stderr.
// Returning 1 gives the non-zero exit code required by the task.
int usage(void)
{
    fprintf(stderr, "Usage: calc [-d DIR] [-o OUTDIR] [input.txt]\n");
    return 1;
}

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

// Positions are indexes in the original file and start from zero.
// Error messages add one because the task uses positions starting from one.
char *text_data;
size_t pos;
size_t line_end;
size_t error_pos;

enum { TOKEN_END, TOKEN_NUMBER, TOKEN_POWER, TOKEN_INVALID };

int token;

// This version uses long long for all numbers.
// Float support will be added in the next step.
long long token_value;

size_t token_pos;

long long parse_expression(void);

// Only spaces and tabs are skipped because a newline ends an expression.
// Skipping changes the position but does not remove characters from the text.
void skip_spaces(void)
{
    while (pos < line_end &&
           (text_data[pos] == ' ' || text_data[pos] == '\t'))
    {
        pos++;
    }
}

// The token start is saved before reading its characters.
// Errors can use this position after the parser moves to the next token.
// Two stars are read together because ** is one operator.
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
        token_value = 0;

        while (pos < line_end &&
               text_data[pos] >= '0' && text_data[pos] <= '9')
        {
            token_value = token_value * 10 + (text_data[pos] - '0');
            pos++;
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
        // An unknown character is where reading cannot continue.
        token = TOKEN_INVALID;
        error_pos = token_pos + 1;
    }
}

// Brackets call parse_expression so everything inside is calculated first.
// A missing ')' is reported at the next token or the end of the line.
// An operator cannot start here because unary operators are not allowed.
long long parse_primary(void)
{
    if (error_pos != 0)
    {
        return 0;
    }

    if (token == TOKEN_NUMBER)
    {
        long long value = token_value;
        next_token();
        return value;
    }

    if (token == '(')
    {
        next_token();
        long long value = parse_expression();

        if (error_pos != 0)
        {
            return 0;
        }

        if (token != ')')
        {
            error_pos = token_pos + 1;
            return 0;
        }

        next_token();
        return value;
    }

    // At line end, this gives the newline position.
    // Without a final newline, it gives file length + 1.
    error_pos = token_pos + 1;
    return 0;
}

// Calling parse_power for the exponent makes ** right-associative.
// This means 2**3**2 is calculated as 2**(3**2).
// A negative exponent is reported at its saved starting position.
long long parse_power(void)
{
    long long value = parse_primary();

    if (error_pos != 0)
    {
        return 0;
    }

    if (token == TOKEN_POWER)
    {
        next_token();
        size_t exponent_start = token_pos;
        long long exponent = parse_power();

        if (error_pos != 0)
        {
            return 0;
        }

        if (exponent < 0)
        {
            error_pos = exponent_start + 1;
            return 0;
        }

        // Starting with 1 also gives 1 for x**0, including 0**0.
        long long result = 1;

        // Squaring the base reduces the number of multiplications.
        while (exponent > 0)
        {
            if (exponent % 2 == 1)
            {
                result = result * value;
            }

            exponent = exponent / 2;

            if (exponent > 0)
            {
                value = value * value;
            }
        }

        return result;
    }

    return value;
}

// Powers are read first, so ** has higher priority than * and /.
// The loop works from left to right, so 20/2/2 means (20/2)/2.
// Both values are long long, so division follows C integer division.
long long parse_term(void)
{
    long long value = parse_power();

    while (error_pos == 0)
    {
        if (token != '*' && token != '/')
        {
            break;
        }

        int op = token;
        next_token();

        // Save this because parsing the right operand moves forward.
        size_t right_start = token_pos;
        long long right = parse_power();

        if (error_pos != 0)
        {
            return 0;
        }

        if (op == '*')
        {
            value = value * right;
        }
        else
        {
            if (right == 0)
            {
                // Zero division is reported at the right operand's start.
                error_pos = right_start + 1;
                return 0;
            }

            value = value / right;
        }
    }

    return value;
}

// Reading terms first gives * and / higher priority than + and -.
// The loop makes addition and subtraction work from left to right.
long long parse_expression(void)
{
    long long value = parse_term();

    while (error_pos == 0)
    {
        if (token != '+' && token != '-')
        {
            break;
        }

        int op = token;
        next_token();

        long long right = parse_term();

        if (error_pos != 0)
        {
            return 0;
        }

        if (op == '+')
        {
            value = value + right;
        }
        else
        {
            value = value - right;
        }
    }

    return value;
}

// The first pass checks expressions without writing any results.
// This prevents partial results when a later expression has an error.
// The second pass writes answers without storing a separate result list.
// Both passes move forward through the text, so the work stays linear.
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

            // Blank and comment lines give no result.
            // Their characters still count because the text is not changed.
            if (pos < line_end && text_data[pos] != '#')
            {
                next_token();
                long long result = parse_expression();

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

                    // An expression error is a normal result.
                    return 0;
                }

                if (pass == 1)
                {
                    if (fprintf(output, "%lld\n", result) < 0)
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
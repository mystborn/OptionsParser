#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "option_parser.h"

#define check_flag(flags, flag) (((flags) & (flag)) == (flag))
#define ERROR_BUFFER_SIZE 256

OptionParser* oparser_init(OptionHandler handler, ParserFlags flags, void* data) {
    OptionParser* parser = malloc(sizeof(OptionParser));
    if(!parser)
        return NULL;
    parser->options = malloc(sizeof(Option) * 2);
    if(!parser->options) {
        free(parser);
        return NULL;
    }
    if(check_flag(flags, PF_ALLOW_REMAINDER)) {
        parser->remainder = malloc(sizeof(char*) * 2);
        if(!parser->remainder) {
            free(parser->options);
            free(parser);
            return NULL;
        }
        parser->remainder_capacity = 2;
    } else {
        parser->remainder = NULL;
        parser->remainder_capacity = 0;
    }
    parser->option_count = 0;
    parser->option_capacity = 2;
    parser->flags = flags;
    parser->remainder_count = 0;
    parser->handler = handler;
    parser->data = data;
    return parser;
}

static void option_free(Option* option) {
    if(option->sub_options != NULL) {
        free(option->sub_options->options);
        free(option->sub_options);
    }
}

void oparser_free(OptionParser* parser) {
    for(int i = 0; i < parser->option_count; i++)
        option_free(parser->options + i);

    if(parser->remainder != NULL)
        free(parser->remainder);

    free(parser->options);
    free(parser);
}

static bool verify_flags(OptionFlags flags) {
    static OptionFlags value_required_check = OF_VALUE_REQUIRED | OF_VALUE_NOT_ALLOWED;

    if(check_flag(flags, value_required_check))
        return false;

    return true;
}

static bool is_option_char(char character) {
    return character != '=' && character != '\0';
}

Option* oparser_add_option(OptionParser* parser, char* option_name, int alias, OptionFlags flags, char* doc_string) {
    if(!verify_flags(flags))
        return NULL;

    if(parser->option_count == parser->option_capacity) {
        parser->option_capacity *= 2;
        parser->options = realloc(parser->options, parser->option_capacity * sizeof(Option));
    }
    Option* option = parser->options + parser->option_count++;
    option->sub_options = NULL;

    struct OptionBase* base = (struct OptionBase*)option;
    base->name = option_name;
    base->alias = alias;
    base->name_length = strlen(option_name);
    base->flags = flags;
    base->doc_string = doc_string;

    return option;
}

OptionSubParser* osubparser_init(Option* option, OptionHandler handler, ParserFlags flags, void* data) {
    OptionSubParser* parser = malloc(sizeof(OptionSubParser));
    if(!parser)
        return NULL;
    parser->options = malloc(sizeof(SubOption) * 2);
    if(!parser->options){
        free(parser);
        return NULL;
    }
    parser->option_capacity = 2;
    parser->option_count = 0;
    parser->handler = handler;
    parser->flags = flags;
    parser->data = data;
    option->sub_options = parser;
    return parser;
}

SubOption* osubparser_add_option(OptionSubParser* parser, char* option_name, int alias, OptionFlags flags, char* doc_string) {
    if(!verify_flags(flags))
        return NULL;

    if(parser->option_count == parser->option_capacity) {
        parser->option_capacity *= 2;
        parser->options = realloc(parser->options, parser->option_capacity * sizeof(SubOption));
        if(!parser->options)
            return NULL;
    }
    SubOption* option = parser->options + parser->option_count++;

    struct OptionBase* base = (struct OptionBase*)option;
    base->name = option_name;
    base->alias = alias;
    base->name_length = strlen(option_name);
    base->flags = flags;
    base->doc_string = doc_string;

    return option;
}

static int scan_for_name(char* name, struct OptionBase* options, int option_size, int option_count) {
    // Needs the size of the option struct so the the pointer can be incremented correctly.
    // Without it, it would increment the memory address by the sizeof(OptionBase),
    // which would be incorrect for Option.
    for(int i = 0; i < option_count; i++) {
        if(strncmp(options->name, name, options->name_length) == 0)
            return i;
        options = (struct OptionBase*)((char*)options + option_size);
    }
    return -1;
}

static int scan_for_alias(char alias, struct OptionBase* options, int option_size, int option_count) {
    // Needs the size of the option struct so the the pointer can be incremented correctly.
    // Without it, it would increment the memory address by the sizeof(OptionBase),
    // which would be incorrect for Option.
    for(int i = 0; i < option_count; i++) {
        if(options->alias == alias)
            return i;
        options = (struct OptionBase*)((char*)options + option_size);
    }
    return -1;
}

static bool option_encounter_is_valid(struct OptionBase* options) {
    if(!check_flag(options->flags, OF_DUPLICATES_ALLOWED) && check_flag(options->flags, ___OF_ENCOUNTERED))
        return false;
    options->flags |= ___OF_ENCOUNTERED;
    return true;
}

static struct OptionBase* verify_required_options(struct OptionBase* options, int option_size, int option_count) {
    // Needs the size of the option struct so the the pointer can be incremented correctly.
    // Without it, it would increment the memory address by the sizeof(OptionBase),
    // which would be incorrect for Option.
    for(int i = 0; i < option_count; i++) {
        if((options->flags & (___OF_ENCOUNTERED | OF_REQUIRED)) == OF_REQUIRED)
            return options;
        else
            options->flags &= ~___OF_ENCOUNTERED;

        options = (struct OptionBase*)((char*)options + option_size);
    }
    return NULL;
}

static void parse_sub_options(OptionSubParser* parser, char* parent_name, ParseResult* result, int* i, char** argv, int argc) {
    while(*i + 1 < argc) {
        char* option_string = argv[*i + 1];
        int count = 0;
        while(is_option_char(option_string[count]))
            count++;

        if(count == 0) {
            struct OptionBase* invalid = verify_required_options((struct OptionBase*)parser->options, sizeof(SubOption), parser->option_count);
            if(invalid != NULL) {
                result->error = PE_REQUIRED_MISSING;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, invalid->name);
                return;
            }
            return;
        }

        int option_index = scan_for_name(option_string, (struct OptionBase*)parser->options, sizeof(SubOption), parser->option_count);
        if(option_index == -1) {
            struct OptionBase* invalid = verify_required_options((struct OptionBase*)parser->options, sizeof(SubOption), parser->option_count);
            if(invalid != NULL) {
                result->error = PE_REQUIRED_MISSING;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, invalid->name);
                return;
            }
            return;
        }
        (*i)++;
        SubOption* option = parser->options + option_index;
        if(!option_encounter_is_valid((struct OptionBase*)option)) {
            result->error = PE_DUPLICATE;
            snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, option->base.name);
            return;
        }

        switch(option_string[count]) {
            case '\0':
                if(check_flag(option->base.flags, OF_VALUE_REQUIRED)) {
                    result->error = PE_VALUE_MISSING;
                    snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, option->base.name);
                    return;
                }
                parser->handler(option->base.name, option->base.alias, NULL, parser->data);
                break;
            case '=':
                if(check_flag(option->base.flags, OF_VALUE_NOT_ALLOWED)) {
                    result->error = PE_VALUE_GIVEN;
                    snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, option->base.name);
                    return;
                }
                int length = strlen(option_string + count + 1);
                if(length == 0) {
                    result->error = PE_VALUE_INVALID;
                    snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, option->base.name);
                    return;
                }
                parser->handler(option->base.name, option->base.alias, option_string + count + 1, parser->data);
                break;
            default:
                result->error = PE_INVALID_NAME_TOKEN;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name,  option_string);
                return;
        }
    }

    struct OptionBase* final = verify_required_options((struct OptionBase*)parser->options, sizeof(SubOption), parser->option_count);
    if(final != NULL) {
        result->error = PE_REQUIRED_MISSING;
        snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s.%s", parent_name, final->name);
        return;
    }
}

static void parse_name(OptionParser* parser, ParseResult* result, char* option_string, int start_index, int* i, char** argv, int argc) {
    int count = 0;
    while(is_option_char(option_string[start_index + count]))
        count++;

    int option_index = scan_for_name(option_string + start_index, (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);

    if(option_index == -1 && count == 1 && check_flag(parser->flags, PF_ALWAYS_CHECK_FOR_ALIAS))
        option_index = scan_for_alias(option_string[start_index], (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);

    if(option_index == -1) {
        result->error = PE_INVALID_NAME;
        snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option_string + start_index);
        return;
    }

    Option* option = parser->options + option_index;
    if(!option_encounter_is_valid((struct OptionBase*)option)) {
        result->error = PE_DUPLICATE;
        snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
        return;
    }

    switch(option_string[start_index + count]) {
        case '\0':
            if(check_flag(option->base.flags, OF_VALUE_REQUIRED)) {
                result->error = PE_VALUE_MISSING;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
                return;
            }
            parser->handler(option->base.name, option->base.alias, NULL, parser->data);
            break;
        case '=':
            if(check_flag(option->base.flags, OF_VALUE_NOT_ALLOWED)) {
                result->error = PE_VALUE_GIVEN;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
                return;
            }
            start_index += count + 1;
            int length = strlen(option_string + start_index);
            if(length == 0) {
                result->error = PE_VALUE_INVALID;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
                return;
            }
            parser->handler(option->base.name, option->base.alias, option_string + start_index, parser->data);
            break;
        default:
            result->error = PE_INVALID_NAME_TOKEN;
            snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option_string + start_index);
            return;
    }

    if(option->sub_options != NULL)
        parse_sub_options(option->sub_options, option->base.name, result, i, argv, argc);
}

static void parse_alias(OptionParser* parser, ParseResult* result, char* option_string, int start_index, int* i, char** argv, int argc) {
    int count = 0;
    while(isalnum(option_string[start_index + count])) {
        int option_index = scan_for_alias(option_string[start_index + count], (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
        if(option_index == -1) {
            result->error = PE_INVALID_ALIAS;
            sprintf(result->error_value, "%c", option_string[start_index + count]);
            return;
        }

        Option* option = parser->options + option_index;

        if (!option_encounter_is_valid((struct OptionBase*)option)) {
            result->error = PE_DUPLICATE;
            snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
            return;
        }

        if(check_flag(option->base.flags, OF_VALUE_REQUIRED)) {
            result->error = PE_VALUE_MISSING;
            snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
            return;
        }

        if(count == 0 && check_flag(parser->flags, PF_SETTABLE_FLAGS) && option_string[start_index + 1] == '=') {
            if(check_flag(option->base.flags, OF_VALUE_NOT_ALLOWED)) {
                result->error = PE_VALUE_GIVEN;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
                return;
            }
            start_index += count + 1;
            int length = strlen(option_string + start_index);
            if(length == 0) {
                result->error = PE_VALUE_INVALID;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option->base.name);
                return;
            }
            parser->handler(option->base.name, option->base.alias, option_string + start_index, parser->data);
            if(option->sub_options != NULL)
                parse_sub_options(option->sub_options, option->base.name, result, i, argv, argc);
        } else {
            parser->handler(option->base.name, option->base.alias, NULL, parser->data);
            result->options_parsed++;
            if(option->sub_options != NULL)
                parse_sub_options(option->sub_options, option->base.name, result, i, argv, argc);
            count++;
        }
    }

    switch(option_string[start_index + count]) {
        case '\0':
            return;
        default:
            result->error = PE_INVALID_ALIAS_TOKEN;
            snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option_string);
            return;
    }
}

static void parse_string(OptionParser* parser, ParseResult* result, char* option_string, int start_index, int* i, char** argv, int argc) {
    switch(option_string[start_index]) {
        case '-':
            if(option_string[start_index + 1] == '-') {
                parse_name(parser, result, option_string, start_index + 2, i, argv, argc);
            } else {
                if(check_flag(parser->flags, PF_TREAT_DASH_AS_FULL_OPTION))
                    parse_name(parser, result, option_string, start_index + 1, i, argv, argc);
                else
                    parse_alias(parser, result, option_string, 1, i, argv, argc);
            }
            break;
        case '/':
            parse_name(parser, result, option_string, start_index + 1, i, argv, argc);
            break;
        default:
            if(!check_flag(parser->flags, PF_ALLOW_REMAINDER)) {
                result->error = PE_REMAINDER;
                snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", option_string);
                return;
            }

            if(parser->remainder_count == parser->remainder_capacity) {
                parser->remainder_capacity *= 2;
                parser->remainder = realloc(parser->remainder, parser->remainder_capacity * sizeof(char*));
            }
            parser->remainder[parser->remainder_count++] = argv[*i];
            break;
    }
}

ParseResult* oparser_parse(OptionParser* parser, char** argv, int argc) {
    if(parser->remainder_count > 0)
        parser->remainder_count = 0;

    ParseResult* result = malloc(sizeof(ParseResult));
    result->error = PE_NONE;
    result->options_parsed = 0;
    for(int i = 1; i < argc; i++) {
        parse_string(parser, result, argv[i], 0, &i, argv, argc);
        if(result->error != PE_NONE)
            return result;
    }
    struct OptionBase* invalid = verify_required_options((struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
    if(invalid != NULL) {
        result->error = PE_REQUIRED_MISSING;
        snprintf(result->error_value, ERROR_BUFFER_SIZE, "%s", invalid->name);
        return result;
    }
    return result;
}

static char* error_string = NULL;

char* oparser_get_error_string(ParseResult* result) {
    if(result->error == PE_NONE)
        return NULL;

    if(error_string == NULL)
        error_string = malloc(512);

    switch(result->error) {
        case PE_INVALID_NAME:
            sprintf(error_string, "Encountered invalid option: %s", result->error_value);
            break;
        case PE_INVALID_ALIAS:
            sprintf(error_string, "Encountered invalid option alias: %s", result->error_value);
            break;
        case PE_INVALID_NAME_TOKEN:
            sprintf(error_string, "Encountered invalid token in option: %s", result->error_value);
            break;
        case PE_INVALID_ALIAS_TOKEN:
            sprintf(error_string, "Encountered invalid token in alias list: %s", result->error_value);
            break;
        case PE_DUPLICATE:
            sprintf(error_string, "Encountered an invalid duplicate option: %s", result->error_value);
            break;
        case PE_REQUIRED_MISSING:
            sprintf(error_string, "Missing required option: %s", result->error_value);
            break;
        case PE_VALUE_INVALID:
            sprintf(error_string, "Missing value after equals sign for option: %s", result->error_value);
            break;
        case PE_VALUE_MISSING:
            sprintf(error_string, "Expected value for option: %s", result->error_value);
            break;
        case PE_VALUE_GIVEN:
            sprintf(error_string, "Cannot set option: %s", result->error_value);
            break;
        case PE_REMAINDER:
            sprintf(error_string, "Cannot accept non-option value: %s", result->error_value);
            break;
        default:
            sprintf(error_string, "Encountered unknown error: %d", result->error);
            break;
    }
    return error_string;
}

void oparser_result_free(ParseResult* result) {
    if(error_string != NULL) {
        free(error_string);
        error_string = NULL;
    }
    free(result);
}

static char* suboption_doc_string(SubOption* option, int doc_start) {
    int size = 256;
    int offset = 0;
    char* buffer = malloc(256);
    if(!buffer)
        return NULL;

    bool required = check_flag(option->base.flags, OF_REQUIRED);

    offset += snprintf(buffer + offset, size - offset, "  ");

    if(check_flag(option->base.flags, OF_REQUIRED)) {
        offset += snprintf(buffer + offset, size - offset, "[%s]", option->base.name);
    } else {
        offset += snprintf(buffer + offset, size - offset, "%s", option->base.name);
    }

    offset += snprintf(buffer + offset, size - offset, "%-*s %s\n", doc_start - offset, "", option->base.doc_string);
    return buffer;
}

static char* option_doc_string(Option* option, int doc_start) {
    int size;
    if(option->sub_options != NULL)
        size = 256 + option->sub_options->option_count * 256;
    else
        size = 256;
    int offset = 0;

    char* buffer = malloc(size);
    if(!buffer)
        return NULL;
    bool required = check_flag(option->base.flags, OF_REQUIRED);

    offset += snprintf(buffer + offset, size - offset, "  ");

    if(!required)
        offset += snprintf(buffer + offset, size - offset, "[");

    offset += snprintf(buffer + offset, size - offset, "--%s", option->base.name);

    if(isascii(option->base.alias))
        offset += snprintf(buffer + offset, size - offset, "|-%c", option->base.alias);

    if(!required)
        offset += snprintf(buffer + offset, size - offset, "]");

    offset += snprintf(buffer + offset, size - offset, "%-*s %s\n", doc_start - offset, "", option->base.doc_string);

    if(option->sub_options != NULL) {
        for(int i = 0; i < option->sub_options->option_count; i++) {
            char* sub_help = suboption_doc_string(option->sub_options->options + i, doc_start - 2);
            if(!sub_help) {
                free(buffer);
                return NULL;
            }
            offset += snprintf(buffer + offset, size - offset, "  %s", sub_help);
            free(sub_help);
        }
    }
    return buffer;
}

char* oparser_help(OptionParser* parser) {
    int size = 0;
    int offset = 0;
    int doc_start = 0;
    for(int i = 0; i < parser->option_count; i++) {
        if(parser->options[i].base.name_length + 10 > doc_start)
            doc_start = parser->options[i].base.name_length + 10;

        size += 256;
        if(parser->options[i].sub_options != NULL) {
            OptionSubParser* subparser = parser->options[i].sub_options;
            for(int j = 0; j < subparser->option_count; j++) {
                size += 256;
                if(subparser->options[j].base.name_length + 6 > doc_start)
                    doc_start = subparser->options[j].base.name_length + 6;
            }
        }
    }
    char* buffer = malloc(size);
    if(!buffer)
        return NULL;
    for(int i = 0; i < parser->option_count; i++) {
        char* option_string = option_doc_string(parser->options + i, doc_start);
        if(!option_string) {
            free(buffer);
            return NULL;
        }
        offset += snprintf(buffer + offset, size - offset, "%s", option_string);
        free(option_string);
    }
    return buffer;
}

char* oparser_option_help(OptionParser* parser, char* option_name) {
    int option_index = scan_for_name(option_name, (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
    if(option_index == -1)
        return NULL;

    int doc_start = parser->options[option_index].base.name_length + 10;

    if(parser->options[option_index].sub_options != NULL) {
        OptionSubParser* subparser = parser->options[option_index].sub_options;
        for(int i = 0; i < subparser->option_count; i++) {
            if(subparser->options[i].base.name_length + 6 > doc_start)
                doc_start = subparser->options[i].base.name_length + 6;
        }
    }

    return option_doc_string(parser->options + option_index, doc_start);
}

char* oparser_suboption_help(OptionParser* parser, char* option_name, char* suboption_name) {
    int option_index = scan_for_name(option_name, (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
    if(option_index == -1)
        return NULL;

    Option* option = parser->options + option_index;
    if(option->sub_options == NULL)
        return NULL;

    option_index = scan_for_name(suboption_name, (struct OptionBase*)option->sub_options->options, sizeof(SubOption), option->sub_options->option_count);
    if(option_index == -1)
        return NULL;

    return suboption_doc_string(option->sub_options->options + option_index, option->sub_options->options[option_index].base.name_length + 4);
}

char* oparser_option_docstring(OptionParser* parser, char* option_name) {
    int option_index = scan_for_name(option_name, (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
    if(option_index == -1)
        return NULL;

    return parser->options[option_index].base.doc_string;
}

char* oparser_suboption_docstring(OptionParser* parser, char* option_name, char* suboption_name) {
    int option_index = scan_for_name(option_name, (struct OptionBase*)parser->options, sizeof(Option), parser->option_count);
    if(option_index == -1)
        return NULL;

    OptionSubParser* subparser = parser->options[option_index].sub_options;
    if(subparser == NULL)
        return NULL;

    option_index = scan_for_name(suboption_name, (struct OptionBase*)subparser->options, sizeof(SubOption), subparser->option_count);
    if(option_index == -1)
        return NULL;

    return subparser->options[option_index].base.doc_string;
}

char** oparser_remainder(OptionParser* parser, int* count) {
    if(!check_flag(parser->flags, PF_ALLOW_REMAINDER))
        return NULL;
    
    *count = parser->remainder_count;
    return parser->remainder;
}
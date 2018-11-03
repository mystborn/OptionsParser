#ifndef OPTIONS_PARSER_OPTION_PARSER_H
#define OPTIONS_PARSER_OPTION_PARSER_H

#include <stdbool.h>

// Define Option Types

// All options types must have a OptionBase as their first member
// so that they can be be properly casted to OptionBase.


typedef void (*OptionHandler)(char*, int, char*, void*);

// Defines flags that modify Option behaviour.
typedef enum OptionFlags {
    // default behaviour.
    OF_NONE = 0,

    // Determines if the option has been encountered. Should not be used by user.
    ___OF_ENCOUNTERED = 1,

    // Determines if the option is required.
    OF_REQUIRED = 2,

    // Determines if the option requires a value.
    OF_VALUE_REQUIRED = 4,

    // Determines if the option must not have a value.
    OF_VALUE_NOT_ALLOWED = 8,

    // Determines if the option is allowed to appear multiple times.
    OF_DUPLICATES_ALLOWED = 16
} OptionFlags;

// Defines flags that modify Parser behaviour.
typedef enum ParserFlags {
    // Default behaviour.
    PF_NONE = 0,

    // Determines if the parser allows non-option values to be included.
    PF_ALLOW_REMAINDER = 1,

    // Determines if the parser tries to match an option alias if the name check fails when using '--' or '/'
    PF_ALWAYS_CHECK_FOR_ALIAS = 2,

    // Determines if the string after a single dash is processed as an option name or as a set of flags.
    PF_TREAT_DASH_AS_FULL_OPTION = 4,

    // Allows standalone flags to be set (e.g. -f=flag_value)
    PF_SETTABLE_FLAGS = 8
} ParserFlags;


// Defines the possible error states of the OptionParser.
typedef enum ParseError {
    // No error.
    PE_NONE,

    // Tried to use an option that doesn't exist.
    PE_INVALID_NAME,

    // Tried to use an alias that doesn't exist.
    PE_INVALID_ALIAS,

    // There was an invalid character in an option name.
    PE_INVALID_NAME_TOKEN,

    // There was an invalid character in an alias set.
    PE_INVALID_ALIAS_TOKEN,

    // There was a duplicate option that didn't have the OF_DUPLICATES_ALLOWED flag.
    PE_DUPLICATE,

    // There was an option missing that was required.
    PE_REQUIRED_MISSING,

    // There was no string after an equals sign.
    PE_VALUE_INVALID,

    // An option that requires a value didn't get one.
    PE_VALUE_MISSING,

    // An option that doesn't allow a value got one.
    PE_VALUE_GIVEN,

    // There was a non-option value.
    PE_REMAINDER,
} ParseError;

// Contains the result of the parser.
typedef struct ParseResult {
    // The error that was encountered, or PE_NONE if the parse was successful.
    ParseError error;

    // Contains a string related to the error that can be used to generate an error message.
    char error_value[256];

    // Determines how many options were successfully parsed.
    int options_parsed;
} ParseResult;

// The base type for an Option. 
struct OptionBase {
    // The name of the option.
    char* name;

    // The help string of the option.
    char* doc_string;

    // The length of the option name.
    // Memoized for performance reasons.
    int name_length;

    // The alias of the option. If this is a char, it can be matched by the parser as additional way to parse this option.
    // This value is passed to the OptionHandler.
    int alias;

    // The flags that determine the option behaviour.
    OptionFlags flags;
};

// The options processed by OptionSubParser.
typedef struct SubOption {
    struct OptionBase base;
} SubOption;

// Parses additional values for an option.
typedef struct OptionSubParser {
    // The additional options.
    SubOption* options;

    // The number of additional options.
    int option_count;

    // The number of additional options that can be held before reallocating memory.
    int option_capacity;

    // The flags that determine parser behaviour.
    ParserFlags flags;

    // A data object that is passed to the OptionHandler.
    void* data;

    // The function that is invoked on a successful option parse.
    OptionHandler handler;
} OptionSubParser;

// The option type processed by OptionParser.
typedef struct Option {
    // Contains basic option fields.
    struct OptionBase base;

    // A special parser used to configure additional values related to this option.
    OptionSubParser* sub_options;
} Option;

// Parses options from the command line.
typedef struct OptionParser {
    // The options to be parsed.
    Option* options;

    // The number of options.
    int option_count;

    // The number of options that can be held before reallocating memory.
    int option_capacity;

    // The flags that determine parser behaviour.
    ParserFlags flags;

    // If flags has PF_ALLOW_REMAINDER, contains the non-option values used to start the program.
    int remainder_count;

    // If flags has PF_ALLOW_REMAINDER, contains the number of non-option values that can be held before reallocating memory.
    int remainder_capacity;

    // If flags has PF_ALLOW_REMAINDER, the non-option values used to start the program.
    char** remainder;

    // A data object that is passed to the OptionHandler.
    void* data;

    // The function that is invoked on a successful option parse.
    OptionHandler handler;
} OptionParser;

// Initializes a new option parser.
// @arg handler: The function to invoke when an option is parsed.
// @arg flags: The PF_* flags used to determine parser behaviour.
// @arg data: A data object that is passed to handler on a successful parse.
// @return: A new OptionParser if successful, NULL if there isn't enough memory.
OptionParser* oparser_init(OptionHandler handler, ParserFlags flags, void* data);

// Deallocates the memory used by an OptionParser.
// @arg parser: The parser to free.
void oparser_free(OptionParser* parser);

// Adds an option to an OptionParser.
// @arg parser: The parser to add the option to.
// @arg option_name: The name of the option.
// @arg alias: The alias of the option. If this is an ascii value, it can used as an additional means to parse the option.
// @arg flags: The OF_* flags that determine the option behaviour.
// @arg doc_string: The documentation related to this option.
// @return: A new Option is successful, NULL if flags had conflicting values or there isn't enough memory.
Option* oparser_add_option(OptionParser* parser, char* option_name, int alias, OptionFlags flags, char* doc_string);

// Adds a subparser to an option that can be used to process additional values related to the option.
// @arg option: The option to add the subparser to.
// @arg handler: The function to invoke when an option is parsed.
// @arg flags: The PF_* flags used to determine the parser behaviour.
// @arg data: A data object that is passed to handler on a successful parse.
// @return: A new OptionSubParser if successful, NULL if there isn't enough memory.
OptionSubParser* osubparser_init(Option* option, OptionHandler handler, ParserFlags flags, void* data);

// Adds an option to a subparser.
// @arg parser: The subparser to add an option to.
// @arg option_name: The name of the option.
// @arg alias: The alias of the option. Ignored by the parser, but passed to the handler.
// @arg flags: The OF_* flags that determine the option behaviour.
// @arg doc_string: The documentation related to this option.
// @return: A new SubOption if successful, NULL if flags had conflicting values or there isn't enough memory.
SubOption* osubparser_add_option(OptionSubParser* parser, char* option_name, int alias, OptionFlags flags, char* doc_string);

// Parses the values used to start the program.
// @arg parser: The parser used to parse the program arguments.
// @arg argv: The program arguments.
// @arg argc: The number of program arguments.
// @return: The result of the parse. Must be freed by the caller with 'oparser_result_free'.
ParseResult* oparser_parse(OptionParser* parser, char** argv, int argc);

// Gets an error string from a ParseResult. Returns NULL if there is no error.
// The string returned from this function should not be deallocated.
// @arg result: The result from a parse.
// @return: A string that contains the encountered error, or NULL if there was no error.
char* oparser_get_error_string(ParseResult* result);

// Frees the result of a parse.
// @arg result: The result to free.
void oparser_result_free(ParseResult* result);

// Gets a nicely formatted help string for all options in the parser. Must be freed by the caller.
// @arg parser: The parser to get the documentation of.
// @return: A help string that must be freed by the caller if successful, or NULL if there wasn't enough memory.
char* oparser_help(OptionParser* parser);

// Gets a nicely formatted help string for a specific option. Must be free by the caller.
// @arg parser: The parser that contains the option.
// @arg option_name: The name of the option to get the documentation of.
// @return: A help string that must be freed by the caller, or NULL if the option didn't exist or there wasn't enough memory.
char* oparser_option_help(OptionParser* parser, char* option_name);

// Gets a nicely formatted help string for a specific sub-option. Must be freed by the caller.
// @arg parser: The parser that contains the option.
// @arg option_name: The name of the option that contains the sub-option.
// @arg suboption_name: The name of the sub-option to get the documentation of.
// @return: A help string that must be freed by the caller, or NULL if either option didn't exist of there wasn't enough memory.
char* oparser_suboption_help(OptionParser* parser, char* option_name, char* suboption_name);

// Gets the docstring for a specific option.
// @arg parser: The parser that contains the option.
// @arg option_name: The name of the option to get the docstring of.
// @return: The docstring used to create the option, or NULL if the option didn't exist.
char* oparser_option_docstring(OptionParser* parser, char* option_name);


// Gets the docstring for a specific sub-option.
// @arg parser: The parser that contains the option.
// @arg option_name: The name of the option that contains the sub-option.
// @arg suboption_name: The name of the sub-option to get the docstring of.
// @return: The docstring used to create the sub-option, or NULL if either option didn't exist.
char* oparser_suboption_docstring(OptionParser* parser, char* option_name, char* suboption_name);

// Gets the non-option values used to start the program.
// @arg parser: The parser used to parse the program arguments.
// @arg count: A pointer to an integer value that will be set to the number of non-option program arguments.
// @return: An array of non-option program arguments if successful, NULL if the parser was created without the PF_ALLOW_REMAINDER flag.
char** oparser_remainder(OptionParser* parser, int* count);

#endif
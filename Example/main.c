#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <option_parser.h>

static void command_handler(char* name, int alias, char* value, void* data) {
    printf("%s\n", name);
    switch(alias) {
        case 't':
            time_t raw_time = time(NULL);
            char* time_string = ctime(&raw_time);
            char current_time[9];
            strncpy(current_time, time_string + 11, 8);
            current_time[8] = 0;
            printf("    Time: %s\n", current_time);
            break;
        case 'd':
            time_t raw_date = time(NULL);
            char* date_string = ctime(&raw_date);
            char date[12];
            strncpy(date, date_string + 4, 7);
            strncpy(date + 7, date_string + 20, 4);
            date[11] = 0;
            printf("    Date: %s\n", date);
            break;
        case 'n':
            puts("    Project Name: demo");
            break;
        case 'e':
            printf("    %s\n", value);
            break;
        case 'P':
            break;
        default:
            break;
    }
}

static void property_handler(char* name, int alias, char* value, void* data) {
    switch(alias) {
        case 'c':
            printf("    %s = %s\n", name, value);
            break;
        case 'n':
            printf("    %s = %s\n", name, value);
            break;
        default:
            break;
    }
}

int main(int argc, char** argv) {
    OptionParser* parser = oparser_init(command_handler, PF_ALLOW_REMAINDER | PF_ALWAYS_CHECK_FOR_ALIAS | PF_TREAT_DASH_AS_FULL_OPTION | PF_SETTABLE_FLAGS, NULL);

    oparser_add_option(parser, "time", 't', OF_VALUE_NOT_ALLOWED, "Gets the current time.");
    oparser_add_option(parser, "date", 'd', OF_VALUE_NOT_ALLOWED, "Gets the current date.");
    oparser_add_option(parser, "name", 'n', OF_VALUE_NOT_ALLOWED, "Gets the project name.");
    oparser_add_option(parser, "echo", 'e', OF_DUPLICATES_ALLOWED | OF_VALUE_REQUIRED, "Echos the specified value.");
    Option* properties = oparser_add_option(parser, "Properties", 'P', OF_VALUE_NOT_ALLOWED, "Specifies the following properties:");
    OptionSubParser* sub_parser = osubparser_init(properties, property_handler, PF_NONE, NULL);
    osubparser_add_option(sub_parser, "config", 'c', OF_VALUE_REQUIRED | OF_REQUIRED, "Specifies the config file.");
    osubparser_add_option(sub_parser, "name", 'n', OF_VALUE_REQUIRED, "Specifies the project name.");

    char* help = oparser_help(parser);
    puts(help);
    free(help);

    help = oparser_option_help(parser, "Properties");
    printf("\n\n\n%s\n", help);
    free(help);

    help = oparser_suboption_help(parser, "Properties", "name");
    printf("\n\n\n%s\n", help);
    free(help);

    ParseResult* result = oparser_parse(parser, argv, argc);
    if(result->error != PE_NONE) {
        printf("%s\n", oparser_get_error_string(result));
        oparser_result_free(result);
        oparser_free(parser);
        return -1;
    }

    int remainder_count;
    char** remainder = oparser_remainder(parser, &remainder_count);

    if(remainder_count > 0) {
        printf("Remainder:\n");
        for(int i = 0; i < remainder_count; i++)
            printf("    %s\n", remainder[i]);
    }
    
    oparser_result_free(result);
    oparser_free(parser);
    return 0;
}

int arg_test(int argc, char** argv) {
    if(argc == 1) {
        puts("No command line arguments.");
        return 0;
    }

    puts("Option Line Arguments:");
    for(int i = 1; i < argc; i++)
        puts(argv[i]);

    return 0;
}
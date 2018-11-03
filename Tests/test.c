#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../Source/option_parser.h"

typedef struct Message {
    char* message;
} Message;

OptionParser* simple_parser;
OptionParser* advance_parser;
Message* simple_message;

void reset_message() {
    simple_message->message = NULL;
}

void simple_handler(char* name, int alias, char* value, void* data) {
    Message* message = (Message*)data;
    switch(alias) {
        case 'n':
            message->message = "name";
            break;
        case 't':
            message->message = "time";
            break;
        case 'a':
            message->message = "any";
            break;
        default:
            break;
    }
}

void advance_handler(char* name, int alias, char* value, void* data) {
}

void advance_subhandler(char* name, int alias, char* value, void* data) {
    Message* message = (Message*)data;
    switch(alias) {
        case 'a':
            message->message = "animal";
            break;
        case 't':
            message->message = "tree";
            break;
        default:
            break;
    }
}

void parser_setup(void) {
    simple_message = malloc(sizeof(Message));
    simple_parser = oparser_init(simple_handler, PF_ALWAYS_CHECK_FOR_ALIAS, simple_message);
    oparser_add_option(simple_parser, "name", 'n', OF_VALUE_REQUIRED, "Sets the name");
    oparser_add_option(simple_parser, "time", 't', OF_VALUE_NOT_ALLOWED, "Gets the time");
    oparser_add_option(simple_parser, "any", 'a', OF_NONE, "Gets or sets any value");

    advance_parser = oparser_init(advance_handler, PF_ALLOW_REMAINDER | PF_TREAT_DASH_AS_FULL_OPTION, simple_message);
    oparser_add_option(advance_parser, "required", 'r', OF_REQUIRED, "A required option");
    oparser_add_option(advance_parser, "duplicate", 'd', OF_DUPLICATES_ALLOWED, "An option that allows duplicates");
    Option* option = oparser_add_option(advance_parser, "sub", 's', OF_NONE, "An option with suboptions");
    OptionSubParser* subparser = osubparser_init(option, advance_subhandler, PF_NONE, simple_message);
    osubparser_add_option(subparser, "animal", 'a', OF_REQUIRED | OF_VALUE_REQUIRED, "Sets the name of an animal");
    osubparser_add_option(subparser, "tree", 't', OF_NONE, "Sets the name of a tree");
}

void parser_teardown(void) {
    oparser_free(simple_parser);
    oparser_free(advance_parser);
    free(simple_message);
}

START_TEST(test_parser_create) {
    OptionParser* parser = oparser_init(simple_handler, PF_NONE, NULL);
    ck_assert_msg(parser != NULL, "Failed to initialize an OptionParser");
    oparser_free(parser);
}
END_TEST

START_TEST(test_parser_required_value) {
    reset_message();
    char* args[] = { NULL, "--name=Moo" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    ck_assert_msg(strcmp(simple_message->message, "name") == 0, "Required handler not invoked.");
    oparser_result_free(result);

    reset_message();
    args[1] = "--any=value";
    result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    ck_assert(strcmp(simple_message->message, "any") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_value_not_allowed) {
    reset_message();
    char* args[] = { NULL, "--time" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    ck_assert(strcmp(simple_message->message, "time") == 0);
    oparser_result_free(result);

    reset_message();
    args[1] = "--any";
    result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    ck_assert(strcmp(simple_message->message, "any") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_sub_option) {
    reset_message();
    char* args[] = { NULL, "--required", "--sub", "animal=cow", "tree" };
    ParseResult* result = oparser_parse(advance_parser, args, 4);
    ck_assert(result->error == PE_NONE);
    ck_assert(strcmp(simple_message->message, "tree"));
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_duplicates_allowed) {
    reset_message();
    char* args[] = { NULL, "--required", "-duplicate", "/duplicate" };
    ParseResult* result = oparser_parse(advance_parser, args, 4);
    ck_assert(result->error == PE_NONE);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_allow_remainder) {
    reset_message();
    char* args[] = { NULL, "--required", "remaining", "values" };
    ParseResult* result = oparser_parse(advance_parser, args, 4);
    ck_assert(result->error == PE_NONE);
    int count;
    char** remainder = oparser_remainder(advance_parser, &count);
    ck_assert(count == 2);
    ck_assert(strcmp(remainder[0], "remaining") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_always_check_alias) {
    reset_message();
    char* args[] = { NULL, "/t" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    ck_assert(strcmp(simple_message->message, "time") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_treat_dash_as_full_option) {
    reset_message();
    char* args[] = { NULL, "-required" };
    ParseResult* result = oparser_parse(advance_parser, args, 2);
    ck_assert(result->error == PE_NONE);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_invalid_name) {
    reset_message();
    char* args[] = { NULL, "--nonoption" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_INVALID_NAME);
    ck_assert(strcmp(oparser_get_error_string(result), "Encountered invalid option: nonoption") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_invalid_alias) {
    reset_message();
    char* args[] = { NULL, "-p" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_INVALID_ALIAS);
    ck_assert(strcmp(oparser_get_error_string(result), "Encountered invalid option alias: p") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_invalid_alias_token) {
    reset_message();
    char* args[] = { NULL, "-a+" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_INVALID_ALIAS_TOKEN);
    ck_assert(strcmp(oparser_get_error_string(result), "Encountered invalid token in alias list: -a+") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_invalid_duplicate) {
    reset_message();
    char* args[] = { NULL, "--any", "--any" };
    ParseResult* result = oparser_parse(simple_parser, args, 3);
    ck_assert(result->error == PE_DUPLICATE);
    ck_assert(strcmp(oparser_get_error_string(result), "Encountered an invalid duplicate option: any") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_required_missing) {
    reset_message();
    char* args[] = { NULL, "--duplicate" };
    ParseResult* result = oparser_parse(advance_parser, args, 2);
    ck_assert(result->error == PE_REQUIRED_MISSING);
    ck_assert(strcmp(oparser_get_error_string(result), "Missing required option: required") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_value_invalid) {
    reset_message();
    char* args[] = { NULL, "--any=" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_VALUE_INVALID);
    ck_assert(strcmp(oparser_get_error_string(result), "Missing value after equals sign for option: any") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_value_missing) {
    reset_message();
    char* args[] = { NULL, "--name" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_VALUE_MISSING);
    ck_assert(strcmp(oparser_get_error_string(result), "Expected value for option: name") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_value_given) {
    reset_message();
    char* args[] = { NULL, "--time=value" };
    ParseResult* result = oparser_parse(simple_parser, args, 2);
    ck_assert(result->error == PE_VALUE_GIVEN);
    ck_assert(strcmp(oparser_get_error_string(result), "Cannot set option: time") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_remainder_error) {
    reset_message();
    char* args[] = { NULL, "--time", "moo" };
    ParseResult* result = oparser_parse(simple_parser, args, 3);
    ck_assert(result->error == PE_REMAINDER);
    ck_assert(strcmp(oparser_get_error_string(result), "Cannot accept non-option value: moo") == 0);
    oparser_result_free(result);
}
END_TEST

START_TEST(test_parser_help) {
    char* help = oparser_help(simple_parser);
    ck_assert(help);
    free(help);
}
END_TEST

START_TEST(test_parser_option_help) {
    char* help = oparser_option_help(simple_parser, "any");
    ck_assert(help);
    free(help);
    help = oparser_option_help(simple_parser, "nonoption");
    ck_assert(help == NULL);
}
END_TEST

START_TEST(test_parser_suboption_help) {
    char* help = oparser_suboption_help(advance_parser, "sub", "animal");
    ck_assert(help);
    free(help);
    help = oparser_suboption_help(advance_parser, "sub", "flora");
    ck_assert(help == NULL);
    help = oparser_suboption_help(advance_parser, "what", "fauna");
    ck_assert(help == NULL);
}
END_TEST

START_TEST(test_parser_option_docstring) {
    char* doc = oparser_option_docstring(simple_parser, "any");
    ck_assert(strcmp(doc, "Gets or sets any value") == 0);
    doc = oparser_option_docstring(simple_parser, "nonoption");
    ck_assert(doc == NULL);
}
END_TEST

START_TEST(test_parser_suboption_docstring) {
    char* doc = oparser_suboption_docstring(advance_parser, "sub", "animal");
    ck_assert(strcmp(doc, "Sets the name of an animal") == 0);
    doc = oparser_suboption_docstring(advance_parser, "sub", "flora");
    ck_assert(doc == NULL);
    doc = oparser_suboption_docstring(advance_parser, "what", "fauna");
    ck_assert(doc == NULL);
}
END_TEST

Suite* oparser_suite(void) {
    Suite* s;
    TCase* tests;

    s = suite_create("Option Parser");
    tests = tcase_create("OptionParser");
    tcase_add_checked_fixture(tests, parser_setup, parser_teardown);
    tcase_add_test(tests, test_parser_create);
    tcase_add_test(tests, test_parser_required_value);
    tcase_add_test(tests, test_parser_value_not_allowed);
    tcase_add_test(tests, test_parser_sub_option);
    tcase_add_test(tests, test_parser_duplicates_allowed);
    tcase_add_test(tests, test_parser_allow_remainder);
    tcase_add_test(tests, test_parser_always_check_alias);
    tcase_add_test(tests, test_parser_treat_dash_as_full_option);
    tcase_add_test(tests, test_parser_invalid_name);
    tcase_add_test(tests, test_parser_invalid_alias);
    tcase_add_test(tests, test_parser_invalid_alias_token);
    tcase_add_test(tests, test_parser_invalid_duplicate);
    tcase_add_test(tests, test_parser_required_missing);
    tcase_add_test(tests, test_parser_value_invalid);
    tcase_add_test(tests, test_parser_value_missing);
    tcase_add_test(tests, test_parser_value_given);
    tcase_add_test(tests, test_parser_remainder_error);
    tcase_add_test(tests, test_parser_help);
    tcase_add_test(tests, test_parser_option_help);
    tcase_add_test(tests, test_parser_suboption_help);
    tcase_add_test(tests, test_parser_option_docstring);
    tcase_add_test(tests, test_parser_suboption_docstring);

    suite_add_tcase(s, tests);

    return s;
}

int main(void) {
    int number_failed;
    Suite* s;
    SRunner* sr;
    s = oparser_suite();
    sr = srunner_create(s);

    printf("starting\n");

    srunner_run_all(sr, CK_NORMAL);

    printf("finished\n");
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { /*"dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
                                     "Initialize: dummy init",
                                     "Test: dummy test",
                                     "Test, require button press: dummy test -b",
                                     "Integer, value required: dummy -i 123",
                                     "Create/write/read file: dummy -f dummy.txt",
                                     "Kitchen sink: dummy test -b -i 123 -f dummy.txt"*/ };

// This is a struct of help strings for each option/flag/variable the command accepts
// Record type 1 is a section header
// Record type 0 is a help item displayed as: "command" "help text"
// This system uses the T_ constants defined in translation/ to display the help text in the user's preferred language
// To add a new T_ constant:
//      1. open the master translation en-us.h
//      2. add a T_ tag and the help text
//      3. Run json2h.py, which will rebuild the translation files, adding defaults where translations are missing
//      values
//      4. Use the new T_ constant in the help text for the command
static const struct ui_help_options options[] = {
/*    { 1, "", T_HELP_DUMMY_COMMANDS },    // section heading
    { 0, "init", T_HELP_DUMMY_INIT },    // init is an example we'll find by position
    { 0, "test", T_HELP_DUMMY_TEST },    // test is an example we'll find by position
    { 1, "", T_HELP_DUMMY_FLAGS },       // section heading for flags
    { 0, "-b", T_HELP_DUMMY_B_FLAG },    //-a flag, with no optional string or integer
    { 0, "-i", T_HELP_DUMMY_I_FLAG },    //-b flag, with optional integer
    { 0, "-f", T_HELP_DUMMY_FILE_FLAG }, //-f flag, a file name string*/
};

void cert_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    //openssl req -x509 -newkey rsa:4096 -nodes \
          -out ./nginx/config/cert.pem \
          -keyout ./nginx/config/key.pem -days 365

    const char *cert_pem = "-----BEGIN CERTIFICATE-----\n\
MIIF3TCCA8WgAwIBAgIUPNMkwDcT1pkDihebaQfebG/5CxMwDQYJKoZIhvcNAQEL\n\
BQAwfjELMAkGA1UEBhMCVVMxDTALBgNVBAgMBElPV0ExEjAQBgNVBAcMCU1VU0NB\n\
VElORTEXMBUGA1UECgwOV0hFUkUgTEFCUyBMTEMxEzARBgNVBAsMCkJVUyBQSVJB\n\
VEUxHjAcBgNVBAMMFWh0dHBzOi8vYnVzcGlyYXRlLmNvbTAeFw0yNTAxMjkxNDI5\n\
NTZaFw0yNjAxMjkxNDI5NTZaMH4xCzAJBgNVBAYTAlVTMQ0wCwYDVQQIDARJT1dB\n\
MRIwEAYDVQQHDAlNVVNDQVRJTkUxFzAVBgNVBAoMDldIRVJFIExBQlMgTExDMRMw\n\
EQYDVQQLDApCVVMgUElSQVRFMR4wHAYDVQQDDBVodHRwczovL2J1c3BpcmF0ZS5j\n\
b20wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCmpTSnVyvhIyEJEhUP\n\
mjxdOewyT3cmZE1X7LcnEqGYoFa2KUtj9b6z+h2zfgsqbIt14Zv/eDK3/SW9mmp8\n\
20kRx5cE2bUzNXVz343nBia16eO/qn1DEmfGFKTkt48yhWvzFM4CScWOlLDv7b6h\n\
XDFa3iX1i/0xvi4SN1o6t3PoSEE4AjdDKlN8g50lsHEdMQmV+J9OvL68P+Iy/A7f\n\
nUeuyF3SegZ2T/9pa5vgnliPCVAzMTl4OWWRhr5XsfydGhaUcU2W8LpfSh1cERjl\n\
72EiSMkFfQa2li1rS2iHcCf9AT05CJnU5WNMd9/q4gyZ7K7yoaqsknVYYVIglbIw\n\
KHK4n/TUZQnmNSo+491hkxLkhrLC7LjLVtT5LC7Sp8ZbYWs51Tp4x4/qBnEzCXr9\n\
OrIuIOacvDjGybl5G5Hvy1PauyGYpqrEb/dIsBKm8VlUJCEsN8rbxcGfGTuBusVv\n\
6v65EdzsxCWuNNslJvVl5f6amzWkTLNW3McFv7ID90y/iQPfybod5CKYUo3zfLA5\n\
kHJVR9P5vW9gKuIWUZE75Q9cXkSq+t9ExHc8jOhWZpSvFNd/qhkRi5B4ZxITUA3b\n\
5aQRHZuHFEFrNx8oRBtvEXIACN5INFhURuybKieC0zKFa0K+vR6zAHTzv/Tq4QmU\n\
qDGbeOychtn3TWUWABwhArKrhwIDAQABo1MwUTAdBgNVHQ4EFgQUSjnuX4w0PMfn\n\
hHu6UvC5MqdfplAwHwYDVR0jBBgwFoAUSjnuX4w0PMfnhHu6UvC5MqdfplAwDwYD\n\
VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAgEALZH+P93QP7zjczdifdt4\n\
/hOBrFHjtsktMq5pDBNxrLPdTEJ0EK6IWN+Tj51P31Kn1yJn6xqYFrykMIT0JXBG\n\
YF8/iGvq+vDnc4FWuxrOVtpyeUGxxW0gblvlgrqDPA+y08lCvnZvzjsZAzYZiEry\n\
o29EKqhp/VvR2DWJUSkta4bDqn+EAH3JbKVa/avQp8eNVizLfTEVb8nRLNtwV+lE\n\
i+KyYHwQhVyAFjdSwaT18kP0fFw9kBCDNOmJuPyH8kB3trQMcy2TpDd+3379m+mv\n\
YfHFpm9NXdQXnJcGmA7nV2REG6CDgF9g5mPDFEGYh6z25zcwEzVUL9BfrZ73fhf2\n\
fjdhOH7N2zjOdPX8IwuG/SJbUC4UNH5fTt80hQsMo33cL7ukbn//SraCQuP479WI\n\
QoiUHOXZO8hDLnCs31iE6BTlrDaLyrd6PgTlXmlfhzGeHtQ9/alv+j2WEtWm661Q\n\
K1w26UicqvVfLbtqCy0PiDgMGE2Wz3bct1v3qy698+sNSrTP/MNmwvTxvIrhgNh3\n\
GOPbbu5h9DOtZdim5+KGnEk9JQRI90AOuCcW2eiu8Z0JZhRV3v6MMmIFvE4odB0x\n\
ogeb1DKjKhB3iCbU23yiyN0wxRmKEPTxlxoEsisx8/7PXtqY48K41m//1yavuLeY\n\
wrK6PPSRSHUXxZmvLVLMf0M=\n\
-----END CERTIFICATE-----";

    const char *public_key_pem = "-----BEGIN PRIVATE KEY-----\n\
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\n\
BTD2tLHZW6/ljMbUQy2lsFdL++ZlJg==\n\
-----END PRIVATE KEY-----";


    mbedtls_x509_crt cert;
    mbedtls_pk_context public_key;

    mbedtls_x509_crt_init(&cert);
    mbedtls_pk_init(&public_key);

    int ret = mbedtls_x509_crt_parse(&cert, (const unsigned char *)cert_pem, strlen(cert_pem) + 1);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to parse certificate: %s\r\n", error_buf);
        return;
    }

    ret = mbedtls_pk_parse_public_key(&public_key, (const unsigned char *)public_key_pem, strlen(public_key_pem) + 1);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to parse public key: %s\r\n", error_buf);
        return;
    }

    uint32_t flags;
    ret = mbedtls_x509_crt_verify(&cert, &cert, NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to verify certificate: %s\r\n", error_buf);
        return;
    }

    printf("Certificate verified successfully\n");

    mbedtls_x509_crt_free(&cert);
    mbedtls_pk_free(&public_key);    

}
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
#include "mbedtls/md_internal.h"
#include "cert.h"
#include "mbedtls/md.h"


    const char *cert_pem = "-----BEGIN CERTIFICATE-----\n\
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\n\
wrK6PPSRSHUXxZmvLVLMf0M=\n\
-----END CERTIFICATE-----";

    const char *public_key_pem = "-----BEGIN PUBLIC KEY-----\n\
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEApqU0p1cr4SMhCRIVD5o8\n\
XTnsMk93JmRNV+y3JxKhmKBWtilLY/W+s/ods34LKmyLdeGb/3gyt/0lvZpqfNtJ\n\
EceXBNm1MzV1c9+N5wYmtenjv6p9QxJnxhSk5LePMoVr8xTOAknFjpSw7+2+oVwx\n\
Wt4l9Yv9Mb4uEjdaOrdz6EhBOAI3QypTfIOdJbBxHTEJlfifTry+vD/iMvwO351H\n\
rshd0noGdk//aWub4J5YjwlQMzE5eDllkYa+V7H8nRoWlHFNlvC6X0odXBEY5e9h\n\
IkjJBX0GtpYta0toh3An/QE9OQiZ1OVjTHff6uIMmeyu8qGqrJJ1WGFSIJWyMChy\n\
uJ/01GUJ5jUqPuPdYZMS5Iaywuy4y1bU+Swu0qfGW2FrOdU6eMeP6gZxMwl6/Tqy\n\
LiDmnLw4xsm5eRuR78tT2rshmKaqxG/3SLASpvFZVCQhLDfK28XBnxk7gbrFb+r+\n\
uRHc7MQlrjTbJSb1ZeX+mps1pEyzVtzHBb+yA/dMv4kD38m6HeQimFKN83ywOZBy\n\
VUfT+b1vYCriFlGRO+UPXF5EqvrfRMR3PIzoVmaUrxTXf6oZEYuQeGcSE1AN2+Wk\n\
ER2bhxRBazcfKEQbbxFyAAjeSDRYVEbsmyongtMyhWtCvr0eswB087/06uEJlKgx\n\
m3jsnIbZ901lFgAcIQKyq4cCAwEAAQ==\n\
-----END PUBLIC KEY-----";

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

void print_x509_info(const mbedtls_x509_crt *cert) {
    char buf[1024];


    // Print the subject name
    mbedtls_x509_dn_gets(buf, sizeof(buf), &cert->subject);
    printf("Subject: %s\r\n", buf);

    // Print the issuer name
    mbedtls_x509_dn_gets(buf, sizeof(buf), &cert->issuer);
    printf("Issuer: %s\r\n", buf);

    // Print the validity period
    const mbedtls_x509_time *valid_from = &cert->valid_from;
    const mbedtls_x509_time *valid_to = &cert->valid_to;
    printf("Valid from: %04d-%02d-%02d %02d:%02d:%02d\r\n",
           valid_from->year, valid_from->mon, valid_from->day,
           valid_from->hour, valid_from->min, valid_from->sec);
    printf("Valid to: %04d-%02d-%02d %02d:%02d:%02d\r\n",
           valid_to->year, valid_to->mon, valid_to->day,
           valid_to->hour, valid_to->min, valid_to->sec);

    // Print the serial number
    mbedtls_x509_serial_gets(buf, sizeof(buf), &cert->serial);
    printf("Serial Number: %s\r\n", buf);

    // Print the public key
    //mbedtls_pk_write_pubkey_pem(&cert->pk, (unsigned char *)buf, sizeof(buf));
    //printf("Public Key: %s\n", buf);
}

void cert_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    mbedtls_x509_crt cert;
    mbedtls_pk_context public_key;
    unsigned char hash[32];

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
/*
    uint32_t flags;
    ret = mbedtls_x509_crt_verify(&cert, &cert, NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to verify certificate: %s\r\n", error_buf);
        return;
    }
*/

    // Compute the SHA-256 hash of the TBS (to-be-signed) part of the certificate
    printf("Verifying the SHA-256 signature");
    const mbedtls_md_info_t *mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    ret = mbedtls_md(
        //mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        mdinfo,
        cert.tbs.p, cert.tbs.len, hash);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to create hash: %s\r\n", error_buf);
        return;
    }

    printf("Hash done\r\n");

    // Verify the certificate signature using the public key
    ret = mbedtls_pk_verify(
        &public_key,
        mdinfo->type,
        hash, 0,
        cert.sig.p, cert.sig.len
    );
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Failed to verify: %s\r\n", error_buf);
        return;
    }

    print_x509_info(&cert); 

    printf("Certificate verified successfully\n");

    mbedtls_x509_crt_free(&cert);
    mbedtls_pk_free(&public_key);    

}
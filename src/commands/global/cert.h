#ifndef CERT_H
#define CERT_H
void cert_handler(struct command_result* res);

    //openssl req -x509 -newkey rsa:4096 -nodes \
          -out ./nginx/config/cert.pem \
          -keyout ./nginx/config/key.pem -days 365
    // openssl rsa -in key.pem -pubout > key.pub




#if 0
    const char *cert_pem = "-----BEGIN CERTIFICATE-----\n\
MIIDQDCCAiigAwIBAgICEzcwDQYJKoZIhvcNAQELBQAwaTELMAkGA1UEBhMCVVMx\n\
DTALBgNVBAgMBElvd2ExEjAQBgNVBAcMCU11c2NhdGluZTEXMBUGA1UECgwOV2hl\n\
cmUgTGFicyBMTEMxHjAcBgNVBAMMFWh0dHBzOi8vYnVzcGlyYXRlLmNvbTAgFw0y\n\
NTAxMzAxMzU3NDdaGA8yMTI1MDEwNjEzNTc0N1owWzELMAkGA1UEBhMCQ04xEjAQ\n\
BgNVBAgMCUd1YW5nZG9uZzERMA8GA1UEBwwIU2hlbnpoZW4xFTATBgNVBAoMDEJ1\n\
cyBQaXJhdGUgNjEOMAwGA1UEAwwFUmV2IDIwggEiMA0GCSqGSIb3DQEBAQUAA4IB\n\
DwAwggEKAoIBAQDaHELT0tUjADSGcMuV4qqfkbgYyvVjb9txdlR2Dsag88fp5Zhx\n\
vrqHVj1hPsU6DPk7umBlj7bHLUCW4/oajgTwA2ZGRC6kQOgVhVDJJAUdQOY1m5Yu\n\
P25ykRM6P/FiJKpIjPHNXaD3/20CVT7y6VhmreOdxxYMgfeLnCNTh4aVGaHJYVkP\n\
icl2OePLKal8uGjQQndL7BLCKM304UpaN1yF+cfzb03uUyfZiC1cQKr8+9gliTv7\n\
WifwmywMelj6u23tLuOnEUoSMGCP2iUPwaQR7K27tbRNsboblqTB3PpO7aVDrOBP\n\
mp0E1sLPKpBuHu9e9hbEqkAkPPsVMnbLCMVjAgMBAAEwDQYJKoZIhvcNAQELBQAD\n\
ggEBACHuDj025NNLsm2hHoM53l+5D3ueD7eBegL0jJaZT2df0N0hC6mkyc7BBTzp\n\
yCk7qa0rwAAIZAICNpSDKbxsBq653L18BCAhfA60rERBIXGhISP/ETjPTOePrxVU\n\
8Uo+eEbu9dGTsGTDIpe8Gnz5+drWO3c1p9TI67Imhq+Dk+Ciiq+blNDjQuYOuu0O\n\
MyhqXvqBLbXURmic+8hgqoSUlYqmRwxBaIPUyqgBEi/GppbvnP0OIyywKwnxIGsi\n\
uY6ZVz1pN0OEW/14wfnQ7rstp5dBHeB1a1XtYHS8T8FWYt56joA/0t23HF/UKSQV\n\
qpA0zpkeYly/skdPdOQnF86n1YU=\n\
-----END CERTIFICATE-----";


    const char *public_key_pem = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2hxC09LVIwA0hnDLleKq\n\
n5G4GMr1Y2/bcXZUdg7GoPPH6eWYcb66h1Y9YT7FOgz5O7pgZY+2xy1AluP6Go4E\n\
8ANmRkQupEDoFYVQySQFHUDmNZuWLj9ucpETOj/xYiSqSIzxzV2g9/9tAlU+8ulY\n\
Zq3jnccWDIH3i5wjU4eGlRmhyWFZD4nJdjnjyympfLho0EJ3S+wSwijN9OFKWjdc\n\
hfnH829N7lMn2YgtXECq/PvYJYk7+1on8JssDHpY+rtt7S7jpxFKEjBgj9olD8Gk\n\
Eeytu7W0TbG6G5akwdz6Tu2lQ6zgT5qdBNbCzyqQbh7vXvYWxKpAJDz7FTJ2ywjF\n\
YwIDAQAB\n\
-----END PUBLIC KEY-----";
#endif
#endif

#ifndef FALAIO_H
#define FALAIO_H

// Global variables
extern const char falaio_name[];

// Function declarations
void falaio_setup(void);
void falaio_setup_message(void);
void falaio_cleanup(void);
void falaio_notify(void);
void falaio_service(void);

#endif // FALAIO_H

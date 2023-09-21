#ifndef _UI_COMMAND_H
#define _UI_COMMAND_H
struct command_response
{
    bool error;
    uint32_t data;
};

struct command_attributes
{
    uint8_t command;    //the actual command called
    bool has_dot;
    bool has_colon;
    uint32_t dot;       // value after .
    uint32_t colon;     // value after :
};

struct _command
{
    bool allow_hiz;
    void (*command)( struct command_attributes *attributes, struct command_response *response);

};
#endif

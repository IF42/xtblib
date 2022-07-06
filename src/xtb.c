/**
** @file xtb.c
** @author Petr Horáček
** @brief 
*/

#include "xtb.h"


typedef enum
{
    CmdTimeInterval
}CmdStat;


typedef struct 
{
    bool is_value;
    
    union
    {
        CmdStat right;      
    }value;
}EitherCmd;



EitherCmd
_client_send_command(
    Client * self
    , char * cmd);

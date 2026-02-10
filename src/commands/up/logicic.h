/*
    logic test for 74xx and 40xx chip for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

typedef struct up_logic {
  char    name[16];
  uint16_t start, end;
} up_logic;

// Single descriptor for all logic tables
typedef struct {
  const up_logic* table;
  size_t count;
  int numpins;
} up_logic_table_desc_t;

void up_logic_handler(struct command_result* res);



/*
    logic test for 74xx and 40xx chip for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// Single descriptor for all logic tables
typedef struct {
  const up_logic* table;
  size_t count;
  int numpins;
} up_logic_table_desc_t;

static const up_logic_table_desc_t up_logic_tables[] = {
  {logicic14, count_of(logicic14), 14},
  {logicic16, count_of(logicic16), 16},
  {logicic20, count_of(logicic20), 20},
  {logicic24, count_of(logicic24), 24},
  {logicic28, count_of(logicic28), 28},
  {logicic40, count_of(logicic40), 40},
};

static bool up_logic_find(const char* type, int* numpins, uint16_t* starttest, uint16_t* endtest);
static void print_logic_types(void);
static void testlogicic(int numpins, uint16_t logicteststart, uint16_t logictestend);


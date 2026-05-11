// DacMan custom matrix scanner
// Rows 0-2: left MCP23017 half
// Rows 3-5: right RP2040 half
// Cols 0-5: shared logical columns

#include "quantum.h"
#include "matrix.h"
#include "i2c_master.h"

void matrix_init_custom(void) {
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    return false;
}

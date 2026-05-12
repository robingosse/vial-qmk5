#include "quantum.h"
#include "matrix.h"
#include "wait.h"

static const pin_t row_pins[3] = { GP0, GP2, GP4 };
static const pin_t col_pins[6] = { GP6, GP8, GP10, GP12, GP14, GP26 };

void matrix_init_custom(void) {
    for (uint8_t row = 0; row < 3; row++) {
        setPinInputHigh(row_pins[row]);
    }

    for (uint8_t col = 0; col < 6; col++) {
        setPinInputHigh(col_pins[col]);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;
    matrix_row_t next[MATRIX_ROWS] = {0};

    for (uint8_t row = 0; row < 3; row++) {
        for (uint8_t r = 0; r < 3; r++) {
            setPinInputHigh(row_pins[r]);
        }

        setPinOutput(row_pins[row]);
        writePinLow(row_pins[row]);

        wait_us(30);

        for (uint8_t col = 0; col < 6; col++) {
            if (!readPin(col_pins[col])) {
                next[row + 3] |= (1 << col);
            }
        }
    }

    for (uint8_t row = 0; row < 3; row++) {
        setPinInputHigh(row_pins[row]);
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        if (current_matrix[row] != next[row]) {
            current_matrix[row] = next[row];
            changed = true;
        }
    }

    return changed;
}

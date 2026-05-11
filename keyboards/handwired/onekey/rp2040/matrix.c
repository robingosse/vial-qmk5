// DacMan custom matrix scanner
// Rows 0-2: left MCP23017 half
// Rows 3-5: right RP2040 half
// Cols 0-5: shared logical columns

#include "quantum.h"
#include "matrix.h"
#include "i2c_master.h"
#include "wait.h"

// MCP23017 I2C address.
// QMK usually expects the 7-bit address shifted left by 1.
#define MCP23017_ADDR 0x40

// MCP23017 registers, BANK = 0 default addressing.
#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA  0x0C
#define MCP_GPPUB  0x0D
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13
#define MCP_OLATA  0x14
#define MCP_OLATB  0x15

// Right half direct RP2040 pins.
static const pin_t row_pins[3] = { GP0, GP2, GP4 };
static const pin_t col_pins[6] = { GP6, GP8, GP10, GP12, GP14, GP26 };

// Left half MCP23017 mapping:
// Rows: A0, A1, A2
// Cols: A3, A4, A5, A6, A7, B7
static const uint8_t left_row_bits[3] = { 0, 1, 2 };

// QMK stores pressed keys as 1 bits in current_matrix[row].
static void select_left_row(uint8_t row) {
    // Expander setup for ROW2COL:
    // - One selected row is driven LOW as output.
    // - Other row pins float as inputs with pullups.
    // - Column pins are inputs with pullups.
    //
    // A port:
    // A0-A2 rows.
    // A3-A7 columns.
    uint8_t iodira = 0xFF;
    uint8_t olata  = 0xFF;

    iodira &= ~(1 << left_row_bits[row]);  // selected row becomes output
    olata  &= ~(1 << left_row_bits[row]);  // selected row driven low

    i2c_writeReg(MCP23017_ADDR, MCP_IODIRA, &iodira, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_OLATA,  &olata,  1, 100);

    // B7 is thumb column input. Everything else on B remains input.
    uint8_t iodirb = 0xFF;
    uint8_t olatb  = 0xFF;
    i2c_writeReg(MCP23017_ADDR, MCP_IODIRB, &iodirb, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_OLATB,  &olatb,  1, 100);
}

static matrix_row_t read_left_cols(void) {
    uint8_t gpioa = 0xFF;
    uint8_t gpiob = 0xFF;

    i2c_readReg(MCP23017_ADDR, MCP_GPIOA, &gpioa, 1, 100);
    i2c_readReg(MCP23017_ADDR, MCP_GPIOB, &gpiob, 1, 100);

    matrix_row_t row = 0;

    // Columns A3-A7 become logical cols 0-4.
    // Active low: pressed means the input reads 0.
    if (!(gpioa & (1 << 3))) row |= (1 << 0);
    if (!(gpioa & (1 << 4))) row |= (1 << 1);
    if (!(gpioa & (1 << 5))) row |= (1 << 2);
    if (!(gpioa & (1 << 6))) row |= (1 << 3);
    if (!(gpioa & (1 << 7))) row |= (1 << 4);

    // Column B7 becomes logical col 5.
    if (!(gpiob & (1 << 7))) row |= (1 << 5);

    return row;
}

static void select_right_row(uint8_t row) {
    for (uint8_t i = 0; i < 3; i++) {
        setPinInputHigh(row_pins[i]);
    }

    setPinOutput(row_pins[row]);
    writePinLow(row_pins[row]);
}

static matrix_row_t read_right_cols(void) {
    matrix_row_t row = 0;

    for (uint8_t col = 0; col < 6; col++) {
        if (!readPin(col_pins[col])) {
            row |= (1 << col);
        }
    }

    return row;
}

void matrix_init_custom(void) {
    i2c_init();

    // MCP23017 initial state:
    // all pins input
    uint8_t all_inputs = 0xFF;
    uint8_t all_high   = 0xFF;

    i2c_writeReg(MCP23017_ADDR, MCP_IODIRA, &all_inputs, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_IODIRB, &all_inputs, 1, 100);

    // Enable pullups on all MCP pins.
    i2c_writeReg(MCP23017_ADDR, MCP_GPPUA, &all_high, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_GPPUB, &all_high, 1, 100);

    // Set output latch high before any pin becomes output.
    i2c_writeReg(MCP23017_ADDR, MCP_OLATA, &all_high, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_OLATB, &all_high, 1, 100);

    // Right half local matrix pins.
    for (uint8_t i = 0; i < 3; i++) {
        setPinInputHigh(row_pins[i]);
    }

    for (uint8_t i = 0; i < 6; i++) {
        setPinInputHigh(col_pins[i]);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    // Left half on MCP23017:
    // logical rows 0, 1, 2
    for (uint8_t row = 0; row < 3; row++) {
        select_left_row(row);
        wait_us(30);

        matrix_row_t new_row = read_left_cols();

        if (current_matrix[row] != new_row) {
            current_matrix[row] = new_row;
            changed = true;
        }
    }

    // Return MCP rows to all-inputs after scan.
    uint8_t all_inputs = 0xFF;
    uint8_t all_high   = 0xFF;
    i2c_writeReg(MCP23017_ADDR, MCP_IODIRA, &all_inputs, 1, 100);
    i2c_writeReg(MCP23017_ADDR, MCP_OLATA,  &all_high,   1, 100);

    // Right half on RP2040:
    // logical rows 3, 4, 5
    for (uint8_t row = 0; row < 3; row++) {
        select_right_row(row);
        wait_us(30);

        matrix_row_t new_row = read_right_cols();
        uint8_t logical_row = row + 3;

        if (current_matrix[logical_row] != new_row) {
            current_matrix[logical_row] = new_row;
            changed = true;
        }
    }

    // Return local rows to idle.
    for (uint8_t i = 0; i < 3; i++) {
        setPinInputHigh(row_pins[i]);
    }

    return changed;
}

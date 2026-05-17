#include "quantum.h"
#include "matrix.h"
#include "i2c_master.h"
#include "wait.h"

#define MCP23017_ADDR 0x40

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA  0x0C
#define MCP_GPPUB  0x0D
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13
#define MCP_OLATA  0x14
#define MCP_OLATB  0x15

static const pin_t right_row_pins[3] = { GP0, GP2, GP4 };
static const pin_t right_col_pins[6] = { GP6, GP8, GP10, GP12, GP14, GP26 };

static bool mcp_available = false;

static bool mcp_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    return i2c_transmit(MCP23017_ADDR, data, 2, 5) == I2C_STATUS_SUCCESS;
}

static bool mcp_read(uint8_t reg, uint8_t *value) {
    *value = 0xFF;

    if (i2c_transmit(MCP23017_ADDR, &reg, 1, 5) != I2C_STATUS_SUCCESS) {
        return false;
    }

    if (i2c_receive(MCP23017_ADDR, value, 1, 5) != I2C_STATUS_SUCCESS) {
        *value = 0xFF;
        return false;
    }

    return true;
}

static void mcp_idle(void) {
    if (!mcp_available) {
        return;
    }

    mcp_write(MCP_IODIRA, 0xFF);
    mcp_write(MCP_IODIRB, 0xFF);
    mcp_write(MCP_OLATA, 0xFF);
    mcp_write(MCP_OLATB, 0xFF);
}

static void select_left_col(uint8_t col) {
    if (!mcp_available) {
        return;
    }

    uint8_t iodira = 0xFF;
    uint8_t iodirb = 0xFF;
    uint8_t olata  = 0xFF;
    uint8_t olatb  = 0xFF;

    if (col < 5) {
        uint8_t bit = col + 3;
        iodira &= ~(1 << bit);
        olata  &= ~(1 << bit);
    } else {
        iodirb &= ~(1 << 7);
        olatb  &= ~(1 << 7);
    }

    mcp_write(MCP_IODIRA, iodira);
    mcp_write(MCP_IODIRB, iodirb);
    mcp_write(MCP_OLATA, olata);
    mcp_write(MCP_OLATB, olatb);
}

static uint8_t read_left_rows(void) {
    if (!mcp_available) {
        return 0;
    }

    uint8_t gpioa = 0xFF;

    if (!mcp_read(MCP_GPIOA, &gpioa)) {
        mcp_available = false;
        return 0;
    }

    uint8_t rows = 0;

    if (!(gpioa & (1 << 0))) rows |= (1 << 0);
    if (!(gpioa & (1 << 1))) rows |= (1 << 1);
    if (!(gpioa & (1 << 2))) rows |= (1 << 2);

    return rows;
}

static void select_right_col(uint8_t col) {
    for (uint8_t i = 0; i < 6; i++) {
        setPinInputHigh(right_col_pins[i]);
    }

    setPinOutput(right_col_pins[col]);
    writePinLow(right_col_pins[col]);
}

static uint8_t read_right_rows(void) {
    uint8_t rows = 0;

    for (uint8_t row = 0; row < 3; row++) {
        if (!readPin(right_row_pins[row])) {
            rows |= (1 << row);
        }
    }

    return rows;
}

void matrix_init_custom(void) {
    i2c_init();

    // 1. Initialize RP2040 Pins (Right Half) - [ROW2COL LOGIC]
    for (uint8_t i = 0; i < 6; i++) {
        setPinInputLow(col_pins[i]); // Columns pulled DOWN to GND
    }
    for (uint8_t i = 0; i < 3; i++) {
        setPinOutput(row_pins[i]);
        writePinLow(row_pins[i]);    // Rows default to LOW
    }

    // 2. Initialize MCP23017 (Left Half) - [COL2ROW LOGIC]
    // We use a 10ms timeout during init. If it fails, it just skips.
    uint8_t data[2];
    
    // Set Port A (Cols 0-5) as inputs (1), Port B (Rows 0-2) as outputs (0)
    data[0] = 0x3F; 
    i2c_writeReg(I2C_EXPANDER_ADDR, IODIRA, &data[0], 1, 10);
    data[0] = 0xF8; 
    i2c_writeReg(I2C_EXPANDER_ADDR, IODIRB, &data[0], 1, 10);

    // Enable internal pull-ups on Port A (Columns)
    data[0] = 0x3F;
    i2c_writeReg(I2C_EXPANDER_ADDR, GPPUA, &data[0], 1, 10);

    // Set Port B (Rows) default state to High
    data[0] = 0x07;
    i2c_writeReg(I2C_EXPANDER_ADDR, GPIOB, &data[0], 1, 10);
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool matrix_has_changed = false;

    for (uint8_t row = 0; row < 3; row++) {
        matrix_row_t last_row_value = current_matrix[row];
        matrix_row_t current_row_value = 0;

        // --- SCAN LEFT SIDE (MCP23017 via I2C) - [COL2ROW] ---
        // Set current row LOW on GPB
        uint8_t pb_out = 0x07 & ~(1 << row);
        
        // Use a strict 1ms timeout. If TRRS is unplugged, it aborts instantly.
        if (i2c_writeReg(I2C_EXPANDER_ADDR, GPIOB, &pb_out, 1, 1) == I2C_STATUS_SUCCESS) {
            uint8_t col_data = 0xFF;
            if (i2c_readReg(I2C_EXPANDER_ADDR, GPIOA, &col_data, 1, 1) == I2C_STATUS_SUCCESS) {
                // Read columns (GPA0-5). Look for LOW signal.
                for (uint8_t col = 0; col < 6; col++) {
                    if (!(col_data & (1 << col))) {
                        current_row_value |= (1 << col);
                    }
                }
            }
            // Restore expander row to HIGH
            pb_out = 0x07;
            i2c_writeReg(I2C_EXPANDER_ADDR, GPIOB, &pb_out, 1, 1);
        }

        // --- SCAN RIGHT SIDE (RP2040 Direct) - [ROW2COL] ---
        // Set current row HIGH on RP2040
        writePinHigh(row_pins[row]);
        
        // Short delay to allow matrix to settle
        wait_us(30);

        // Read columns (Cols 6-11 mapped to our logical matrix)
        // Look for HIGH signal.
        for (uint8_t col = 0; col < 6; col++) {
            if (readPin(col_pins[col]) == 1) { 
                current_row_value |= ((matrix_row_t)1 << (col + 6));
            }
        }
        
        // Restore RP2040 row to LOW
        writePinLow(row_pins[row]);

        // Evaluate changes
        if (last_row_value != current_row_value) {
            current_matrix[row] = current_row_value;
            matrix_has_changed = true;
        }
    }
    return matrix_has_changed;
}

    for (uint8_t col = 0; col < 6; col++) {
        select_right_col(col);
        wait_us(30);

        uint8_t rows = read_right_rows();

        for (uint8_t row = 0; row < 3; row++) {
            if (rows & (1 << row)) {
                next[row + 3] |= (1 << col);
            }
        }
    }

    for (uint8_t col = 0; col < 6; col++) {
        setPinInputHigh(right_col_pins[col]);
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        if (current_matrix[row] != next[row]) {
            current_matrix[row] = next[row];
            changed = true;
        }
    }

    return changed;
}

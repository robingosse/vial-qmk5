#include "quantum.h"
#include "i2c_master.h"

// MCP23017 Default Address
#define I2C_EXPANDER_ADDR (0x20 << 1)

// MCP23017 Registers
#define IODIRA 0x00
#define IODIRB 0x01
#define GPPUA  0x0C
#define GPPUB  0x0D
#define GPIOA  0x12
#define GPIOB  0x13

// ==========================================
// USER CONFIGURATION: Right Half RP2040 Pins
// ==========================================
static const pin_t row_pins[3] = { GP0, GP1, GP2 }; 
static const pin_t col_pins[6] = { GP3, GP4, GP5, GP6, GP7, GP17 }; 
// ==========================================

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
    uint8_t data[2];
    
    // Set Port A (Cols 0-5) as inputs (1), Port B (Rows 0-2) as outputs (0)
    data[0] = 0x3F; 
    i2c_write_register(I2C_EXPANDER_ADDR, IODIRA, &data[0], 1, 10);
    data[0] = 0xF8; 
    i2c_write_register(I2C_EXPANDER_ADDR, IODIRB, &data[0], 1, 10);

    // Enable internal pull-ups on Port A (Columns)
    data[0] = 0x3F;
    i2c_write_register(I2C_EXPANDER_ADDR, GPPUA, &data[0], 1, 10);

    // Set Port B (Rows) default state to High
    data[0] = 0x07;
    i2c_write_register(I2C_EXPANDER_ADDR, GPIOB, &data[0], 1, 10);
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool matrix_has_changed = false;

    for (uint8_t row = 0; row < 3; row++) {
        matrix_row_t last_row_value = current_matrix[row];
        matrix_row_t current_row_value = 0;

        // --- SCAN LEFT SIDE (MCP23017 via I2C) - [COL2ROW] ---
        // Set current row LOW on GPB
        uint8_t pb_out = 0x07 & ~(1 << row);
        
        // 1ms timeout. If TRRS is unplugged, it skips seamlessly.
        if (i2c_write_register(I2C_EXPANDER_ADDR, GPIOB, &pb_out, 1, 1) == I2C_STATUS_SUCCESS) {
            uint8_t col_data = 0xFF;
            if (i2c_read_register(I2C_EXPANDER_ADDR, GPIOA, &col_data, 1, 1) == I2C_STATUS_SUCCESS) {
                // Read columns (GPA0-5). Look for LOW signal.
                for (uint8_t col = 0; col < 6; col++) {
                    if (!(col_data & (1 << col))) {
                        current_row_value |= (1 << col);
                    }
                }
            }
            // Restore expander row to HIGH
            pb_out = 0x07;
            i2c_write_register(I2C_EXPANDER_ADDR, GPIOB, &pb_out, 1, 1);
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

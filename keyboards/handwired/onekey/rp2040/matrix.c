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

static bool mcp_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    return i2c_transmit(MCP23017_ADDR, data, 2, 100) == I2C_STATUS_SUCCESS;
}

static bool mcp_read(uint8_t reg, uint8_t *value) {
    *value = 0xFF;

    if (i2c_transmit(MCP23017_ADDR, &reg, 1, 100) != I2C_STATUS_SUCCESS) {
        return false;
    }

    if (i2c_receive(MCP23017_ADDR, value, 1, 100) != I2C_STATUS_SUCCESS) {
        *value = 0xFF;
        return false;
    }

    return true;
}

static void mcp_all_idle(void) {
    mcp_write(MCP_IODIRA, 0xFF);
    mcp_write(MCP_IODIRB, 0xFF);
    mcp_write(MCP_OLATA, 0xFF);
    mcp_write(MCP_OLATB, 0xFF);
}

static void select_left_col(uint8_t col) {
    uint8_t iodira = 0xFF;
    uint8_t iodirb = 0xFF;
    uint8_t olata  = 0xFF;
    uint8_t olatb  = 0xFF;

    if (col < 5) {
        uint8_t bit = col + 3;      // cols 0-4 = A3-A7
        iodira &= ~(1 << bit);
        olata  &= ~(1 << bit);
    } else {
        iodirb &= ~(1 << 7);        // col 5 = B7
        olatb  &= ~(1 << 7);
    }

    mcp_write(MCP_IODIRA, iodira);
    mcp_write(MCP_IODIRB, iodirb);
    mcp_write(MCP_OLATA, olata);
    mcp_write(MCP_OLATB, olatb);
}

static uint8_t read_left_rows(void) {
    uint8_t gpioa = 0xFF;

    if (!mcp_read(MCP_GPIOA, &gpioa)) {
        return 0x00;
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

    mcp_write(MCP_IODIRA, 0xFF);
    mcp_write(MCP_IODIRB, 0xFF);
    mcp_write(MCP_GPPUA, 0xFF);
    mcp_write(MCP_GPPUB, 0xFF);
    mcp_write(MCP_OLATA, 0xFF);
    mcp_write(MCP_OLATB, 0xFF);

    for (uint8_t row = 0; row < 3; row++) {
        setPinInputHigh(right_row_pins[row]);
    }

    for (uint8_t col = 0; col < 6; col++) {
        setPinInputHigh(right_col_pins[col]);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    matrix_row_t next[MATRIX_ROWS] = {0};

    for (uint8_t col = 0; col < 6; col++) {
        select_left_col(col);
        wait_us(30);

        uint8_t left_rows = read_left_rows();

        for (uint8_t row = 0; row < 3; row++) {
            if (left_rows & (1 << row)) {
                next[row] |= (1 << col);
            }
        }
    }

    mcp_all_idle();

    for (uint8_t col = 0; col < 6; col++) {
        select_right_col(col);
        wait_us(30);

        uint8_t right_rows = read_right_rows();

        for (uint8_t row = 0; row < 3; row++) {
            if (right_rows & (1 << row)) {
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

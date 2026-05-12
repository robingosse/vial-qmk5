// DacMan custom matrix scanner

#include "quantum.h"
#include "matrix.h"
#include "i2c_master.h"
#include "wait.h"

#define MCP23017_ADDR 0x20

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA  0x0C
#define MCP_GPPUB  0x0D
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13
#define MCP_OLATA  0x14
#define MCP_OLATB  0x15

static const pin_t row_pins[3] = { GP0, GP2, GP4 };
static const pin_t col_pins[6] = { GP6, GP8, GP10, GP12, GP14, GP26 };

static void mcp_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    i2c_transmit(MCP23017_ADDR, data, 2, 100);
}

static uint8_t mcp_read(uint8_t reg) {
    uint8_t value = 0;

    i2c_transmit(MCP23017_ADDR, &reg, 1, 100);
    i2c_receive(MCP23017_ADDR, &value, 1, 100);

    return value;
}

static void select_left_row(uint8_t row) {
    uint8_t iodira = 0xFF;
    uint8_t olata  = 0xFF;

    iodira &= ~(1 << row);
    olata  &= ~(1 << row);

    mcp_write(MCP_IODIRA, iodira);
    mcp_write(MCP_OLATA, olata);

    mcp_write(MCP_IODIRB, 0xFF);
    mcp_write(MCP_OLATB, 0xFF);
}

static matrix_row_t read_left_cols(void) {
    uint8_t gpioa = mcp_read(MCP_GPIOA);
    uint8_t gpiob = mcp_read(MCP_GPIOB);

    matrix_row_t row = 0;

    if (!(gpioa & (1 << 3))) row |= (1 << 0);
    if (!(gpioa & (1 << 4))) row |= (1 << 1);
    if (!(gpioa & (1 << 5))) row |= (1 << 2);
    if (!(gpioa & (1 << 6))) row |= (1 << 3);
    if (!(gpioa & (1 << 7))) row |= (1 << 4);

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

    mcp_write(MCP_IODIRA, 0xFF);
    mcp_write(MCP_IODIRB, 0xFF);

    mcp_write(MCP_GPPUA, 0xFF);
    mcp_write(MCP_GPPUB, 0xFF);

    mcp_write(MCP_OLATA, 0xFF);
    mcp_write(MCP_OLATB, 0xFF);

    for (uint8_t i = 0; i < 3; i++) {
        setPinInputHigh(row_pins[i]);
    }

    for (uint8_t i = 0; i < 6; i++) {
        setPinInputHigh(col_pins[i]);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    // LEFT HALF (rows 0-2)
    for (uint8_t row = 0; row < 3; row++) {
        select_left_row(row);

        wait_us(30);

        matrix_row_t new_row = read_left_cols();

        if (current_matrix[row] != new_row) {
            current_matrix[row] = new_row;
            changed = true;
        }
    }

    mcp_write(MCP_IODIRA, 0xFF);
    mcp_write(MCP_OLATA, 0xFF);

    // RIGHT HALF (rows 3-5)
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

    for (uint8_t i = 0; i < 3; i++) {
        setPinInputHigh(row_pins[i]);
    }

    return changed;
}

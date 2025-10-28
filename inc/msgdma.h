/* Scarter Gather DMA*/

#define MSGDMA0_DESC_OFST (MSGDMA0_DESC_BASE - MSGDMA0_CSR_BASE)
#define MSGDMA1_DESC_OFST (MSGDMA1_DESC_BASE - MSGDMA1_CSR_BASE)

/* Scarter Gather DMA Registers*/
/* SGDMA CSR REGS*/

/*
  Enhanced features off:

  Bytes     Access Type     Description
  -----     -----------     -----------
  0-3       R/Clr           Status(1)
  4-7       R/W             Control(2)
  8-12      R               Descriptor Fill Level(write fill level[15:0], read
                            fill level[15:0])
  13-15     R               Response Fill Level[15:0]
  16-31     N/A             <Reserved>


  Enhanced features on:DESC1_READ_ADDRESS_REG

  Bytes     Access Type     Description
  -----     -----------     -----------
  0-3       R/Clr           Status(1)
  4-7       R/W             Control(2)
  8-12      R               Descriptor Fill Level (write fill level[15:0], read
                            fill level[15:0])
  13-15     R               Response Fill Level[15:0]
  16-20     R               Sequence Number (write sequence number[15:0], read
                            sequence number[15:0])
  21-31     N/A             <Reserved>

  (1)  Writing a '1' to the interrupt bit of the status register clears the
       interrupt bit (when applicable), all other bits are unaffected by writes.
  (2)  Writing to the software reset bit will clear the entire register
       (as well as all the registers for the entire msgdma).

  Status Register:

  Bits      Description
  ----      -----------
  0         Busy
  1         Descriptor Buffer Empty
  2         Descriptor Buffer Full
  3         Response Buffer Empty
  4         Response Buffer Full
  5         Stop State
  6         Reset State
  7         Stopped on Error
  8         Stopped on Early Termination
  9         IRQ
  10-31     <Reserved>

  Control Register:

  Bits      Description
  ----      -----------
  0         Stop (will also be set if a stop on error/early termination
            condition occurs)
  1         Software Reset
  2         Stop on Error
  3         Stop on Early Termination
  4         Global Interrupt Enable Mask
  5         Stop dispatcher (stops the dispatcher from issuing more read/write
            commands)
  6-31      <Reserved>
*/



#define CSR_STATUS_REG                          0x0
#define CSR_CONTROL_REG                         0x1
#define CSR_DESCRIPTOR_FILL_LEVEL_REG           0x2
#define CSR_RESPONSE_FILL_LEVEL_REG             0x3
/* this register only exists when the enhanced features are enabled */
#define CSR_SEQUENCE_NUMBER_REG                 0x4


/* masks for the status register bits */
#define CSR_BUSY_MASK                           1
#define CSR_BUSY_OFFSET                         0
#define CSR_DESCRIPTOR_BUFFER_EMPTY_MASK        (1 << 1)
#define CSR_DESCRIPTOR_BUFFER_EMPTY_OFFSET      1
#define CSR_DESCRIPTOR_BUFFER_FULL_MASK         (1 << 2)
#define CSR_DESCRIPTOR_BUFFER_FULL_OFFSET       2
#define CSR_RESPONSE_BUFFER_EMPTY_MASK          (1 << 3)
#define CSR_RESPONSE_BUFFER_EMPTY_OFFSET        3
#define CSR_RESPONSE_BUFFER_FULL_MASK           (1 << 4)
#define CSR_RESPONSE_BUFFER_FULL_OFFSET         4
#define CSR_STOP_STATE_MASK                     (1 << 5)
#define CSR_STOP_STATE_OFFSET                   5
#define CSR_RESET_STATE_MASK                    (1 << 6)
#define CSR_RESET_STATE_OFFSET                  6
#define CSR_STOPPED_ON_ERROR_MASK               (1 << 7)
#define CSR_STOPPED_ON_ERROR_OFFSET             7
#define CSR_STOPPED_ON_EARLY_TERMINATION_MASK   (1 << 8)
#define CSR_STOPPED_ON_EARLY_TERMINATION_OFFSET 8
#define CSR_IRQ_SET_MASK                        (1 << 9)
#define CSR_IRQ_SET_OFFSET                      9

/* masks for the control register bits */
#define CSR_STOP_MASK                           1
#define CSR_STOP_OFFSET                         0
#define CSR_RESET_MASK                          (1 << 1)
#define CSR_RESET_OFFSET                        1
#define CSR_STOP_ON_ERROR_MASK                  (1 << 2)
#define CSR_STOP_ON_ERROR_OFFSET                2
#define CSR_STOP_ON_EARLY_TERMINATION_MASK      (1 << 3)
#define CSR_STOP_ON_EARLY_TERMINATION_OFFSET    3
#define CSR_GLOBAL_INTERRUPT_MASK               (1 << 4)
#define CSR_GLOBAL_INTERRUPT_OFFSET             4
#define CSR_STOP_DESCRIPTORS_MASK               (1 << 5)
#define CSR_STOP_DESCRIPTORS_OFFSET             5

/* masks for the FIFO fill levels and sequence number */
#define CSR_READ_FILL_LEVEL_MASK                0xFFFF
#define CSR_READ_FILL_LEVEL_OFFSET              0
#define CSR_WRITE_FILL_LEVEL_MASK               0xFFFF0000
#define CSR_WRITE_FILL_LEVEL_OFFSET             16
#define CSR_RESPONSE_FILL_LEVEL_MASK            0xFFFF
#define CSR_RESPONSE_FILL_LEVEL_OFFSET          0
#define CSR_READ_SEQUENCE_NUMBER_MASK           0xFFFF
#define CSR_READ_SEQUENCE_NUMBER_OFFSET         0
#define CSR_WRITE_SEQUENCE_NUMBER_MASK          0xFFFF0000
#define CSR_WRITE_SEQUENCE_NUMBER_OFFSET        16


#define START_MSGDMA_MASK	( DESCRIPTOR_CONTROL_GO_MASK | DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_MASK)

/*
  Descriptor formats:

  Standard Format:

  Offset         |    3                 2                 1                   0
  ------------------------------------------------------------------------------
   0x0           |                      Read Address[31..0]
   0x4           |                      Write Address[31..0]
   0x8           |                      Length[31..0]
   0xC           |                      Control[31..0]

  Extended Format:

Offset|   3                  2                  1                  0
 ------------------------------------------------------------------------------
 0x0  |                      Read Address[31..0]
 0x4  |                      Write Address[31..0]
 0x8  |                      Length[31..0]
 0xC  |Write Burst Count[7..0] | Read Burst Count[7..0] | Sequence Number[15..0]
 0x10 | Write Stride[15..0]           |            Read Stride[15..0]
 0x14 |                      Read Address[63..32]
 0x18 |                      Write Address[63..32]
 0x1C |                      Control[31..0]

  Note:  The control register moves from offset 0xC to 0x1C depending on the
         format used

*/

/* SGDMA DESCRITOR REGS*/
#define DESCRIPTOR_READ_ADDRESS_REG                      0x0
#define DESCRIPTOR_WRITE_ADDRESS_REG                     0x1
#define DESCRIPTOR_LENGTH_REG                            0x2
#define DESCRIPTOR_CONTROL_STANDARD_REG                  0x3


/* masks and offsets for the sequence number and programmable burst counts */
#define DESCRIPTOR_SEQUENCE_NUMBER_MASK                  0xFFFF
#define DESCRIPTOR_SEQUENCE_NUMBER_OFFSET                0
#define DESCRIPTOR_READ_BURST_COUNT_MASK                 0x00FF0000
#define DESCRIPTOR_READ_BURST_COUNT_OFFSET               16
#define DESCRIPTOR_WRITE_BURST_COUNT_MASK                0xFF000000
#define DESCRIPTOR_WRITE_BURST_COUNT_OFFSET              24


/* masks and offsets for the read and write strides */
#define DESCRIPTOR_READ_STRIDE_MASK                      0xFFFF
#define DESCRIPTOR_READ_STRIDE_OFFSET                    0
#define DESCRIPTOR_WRITE_STRIDE_MASK                     0xFFFF0000
#define DESCRIPTOR_WRITE_STRIDE_OFFSET                   16


/* masks and offsets for the bits in the descriptor control field */
#define DESCRIPTOR_CONTROL_TRANSMIT_CHANNEL_MASK         0xFF
#define DESCRIPTOR_CONTROL_TRANSMIT_CHANNEL_OFFSET       0
#define DESCRIPTOR_CONTROL_GENERATE_SOP_MASK             (1 << 8)
#define DESCRIPTOR_CONTROL_GENERATE_SOP_OFFSET           8
#define DESCRIPTOR_CONTROL_GENERATE_EOP_MASK             (1 << 9)
#define DESCRIPTOR_CONTROL_GENERATE_EOP_OFFSET           9
#define DESCRIPTOR_CONTROL_PARK_READS_MASK               (1 << 10)
#define DESCRIPTOR_CONTROL_PARK_READS_OFFSET             10
#define DESCRIPTOR_CONTROL_PARK_WRITES_MASK              (1 << 11)
#define DESCRIPTOR_CONTROL_PARK_WRITES_OFFSET            11
#define DESCRIPTOR_CONTROL_END_ON_EOP_MASK               (1 << 12)
#define DESCRIPTOR_CONTROL_END_ON_EOP_OFFSET             12
#define DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_MASK    (1 << 14)
#define DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_OFFSET  14
#define DESCRIPTOR_CONTROL_EARLY_TERMINATION_IRQ_MASK    (1 << 15)
#define DESCRIPTOR_CONTROL_EARLY_TERMINATION_IRQ_OFFSET  15
/* the read master will use this as the transmit error, the dispatcher will use
this to generate an interrupt if any of the error bits are asserted by the
write master */
#define DESCRIPTOR_CONTROL_ERROR_IRQ_MASK                (0xFF << 16)
#define DESCRIPTOR_CONTROL_ERROR_IRQ_OFFSET              16
#define DESCRIPTOR_CONTROL_EARLY_DONE_ENABLE_MASK        (1 << 24)
#define DESCRIPTOR_CONTROL_EARLY_DONE_ENABLE_OFFSET      24
/* at a minimum you always have to write '1' to this bit as it commits the
descriptor to the dispatcher */
#define DESCRIPTOR_CONTROL_GO_MASK                       (1 << 31)
#define DESCRIPTOR_CONTROL_GO_OFFSET                     31

/* ---------------------------------------------------------------------------- */

ALT_STATUS_CODE init_mSGDMA(volatile uint32_t *msgdma_csr_add);
void start_mSGDMA(volatile uint32_t *msgdma_csr_add, uint32_t id);
int msgdma_is_idle(volatile uint32_t *msgdma_csr_add);
void stampa_sgdma_int(void);



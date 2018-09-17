/**************************************************************************//**
 * @file     main.c
 * @version  V3.00
 * $Date: 16/10/25 4:29p $
 * @brief
 *           Use PDMA to implement Ping-Pong buffer by scatter-gather mode(memory to memory).
 *
 * @note
 * Copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 *
 ******************************************************************************/
#include <stdio.h>
#include "NUC029xGE.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Macro, type and constant definitions                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
#define PLL_CLOCK           72000000

uint32_t PDMA_TEST_COUNT = 50;
uint32_t g_au32SrcArray0[1] = {0x55555555};
uint32_t g_au32SrcArray1[1] = {0xAAAAAAAA};
uint32_t g_au32DestArray[1];
uint32_t volatile g_u32IsTestOver = 0;
uint32_t volatile g_u32TransferredCount = 0;
uint32_t g_u32DMAConfig = 0;
static uint32_t s_u32TableIndex = 0;

typedef struct dma_desc_t
{
    uint32_t ctl;
    uint32_t src;
    uint32_t dest;
    uint32_t offset;
} DMA_DESC_T;

DMA_DESC_T DMA_DESC[2]; /* Descriptor table */

/**
 * @brief       DMA IRQ
 *
 * @param       None
 *
 * @return      None
 *
 * @details     The DMA default IRQ, declared in startup_NUC029xGE.s.
 */
void PDMA_IRQHandler(void)
{
    /* Check channel transfer done status */
    //if (PDMA_GET_TD_STS() == PDMA_TDSTS_TDIF4_Msk)
    {
        /* Reload PDMA Descriptor table configuration after transmission finished */
        DMA_DESC[s_u32TableIndex].ctl = g_u32DMAConfig;
        s_u32TableIndex ^= 1;
        /* When finished a descriptor table then g_u32TransferredCount increases 1 */
        g_u32TransferredCount++;

        /* Check if PDMA has finished PDMA_TEST_COUNT tasks */
        if(g_u32TransferredCount >= PDMA_TEST_COUNT)
        {
            /* Set PDMA into idle state by Descriptor table */
            DMA_DESC[0].ctl &= ~PDMA_DSCT_CTL_OPMODE_Msk;
            DMA_DESC[1].ctl &= ~PDMA_DSCT_CTL_OPMODE_Msk;
            g_u32IsTestOver = 1;

            /* Clear table empty flag of channel 4 */
            PDMA_CLR_EMPTY_FLAG(PDMA_SCATSTS_TEMPTYF4_Msk);
        }
        /* Clear transfer done flag of channel 4 */
        PDMA_CLR_TD_FLAG(PDMA_TDSTS_TDIF4_Msk);
    }
}

void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable HIRC clock (Internal RC 22.1184MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);

    /* Waiting for HIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    /* Select HCLK clock source as HIRC and and HCLK clock divider as 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /* Enable HXT clock (external XTAL 12MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

    /* Waiting for HXT clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(PLL_CLOCK);

    /* Waiting for PLL clock ready */
    CLK_WaitClockReady(CLK_STATUS_PLLSTB_Msk);

    /* Switch HCLK clock source to PLL */
//    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL,CLK_CLKDIV0_HCLK(1));

    /* Enable IP clock */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(PDMA_MODULE);

    /* Select UART module clock source as HXT and UART module clock divider as 1 */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set PD multi-function pins for UART0 RXD and TXD */
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD0MFP_Msk | SYS_GPD_MFPL_PD1MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD0MFP_UART0_RXD | SYS_GPD_MFPL_PD1MFP_UART0_TXD);
}

void UART0_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Reset UART module */
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
}

/*---------------------------------------------------------------------------------------------------------*/
/* MAIN function                                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
int main(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Init System, IP clock and multi-function I/O */
    SYS_Init();

    /* Lock protected registers */
    /* If user want to write protected register, please issue SYS_UnlockReg() to unlock protected register. */
    SYS_LockReg();

    /* Init UART for printf */
    UART0_Init();

    printf("\n\nCPU @ %dHz\n", SystemCoreClock);
    printf("+-----------------------------------------------------------------------+ \n");
    printf("|    NUC029xGE PDMA Driver Ping-Pong Buffer Sample Code (Scatter-gather)     | \n");
    printf("+-----------------------------------------------------------------------+ \n");

    /* This sample will transfer data by looped around two descriptor tables from two different source to the same destination buffer in sequence.
       And operation sequence will be table 1 -> table 2-> table 1 -> table 2 -> table 1 -> ... -> until PDMA configuration doesn't be reloaded. */

    /*--------------------------------------------------------------------------------------------------
      PDMA transfer configuration:

        Channel = 4
        Operation mode = scatter-gather mode
        First scatter-gather descriptor table = DMA_DESC[0]
        Request source = PDMA_MEM(memory to memory)

        Transmission flow:

                                            loop around
                                      PDMA_TEST_COUNT/2 times
           ------------------------                             -----------------------
          |                        | ------------------------> |                       |
          |  DMA_DESC[0]           |                           |  DMA_DESC[1]          |
          |  (Descriptor table 1)  |                           |  (Descriptor table 2) |
          |                        | <-----------------------  |                       |
           ------------------------                             -----------------------

        Note: The configuration of each table in SRAM need to be reloaded after transmission finished.
    --------------------------------------------------------------------------------------------------*/

    /* Open Channel 4 */
    PDMA_Open(1 << 4);

    /* Enable Scatter Gather mode, assign the first scatter-gather descriptor table is table 1,
       and set transfer mode as memory to memory */
    PDMA_SetTransferMode(4, PDMA_MEM, TRUE, (uint32_t)&DMA_DESC[0]);


    /* Scatter-Gather descriptor table configuration in SRAM */
    g_u32DMAConfig = \
                     (0 << PDMA_DSCT_CTL_TXCNT_Pos) | /* Transfer count is 1 */ \
                     PDMA_WIDTH_32 |  /* Transfer width is 32 bits(one word) */ \
                     PDMA_SAR_FIX |   /* Source increment size is fixed(no increment) */ \
                     PDMA_DAR_FIX |   /* Destination increment size is fixed(no increment) */ \
                     PDMA_REQ_BURST | /* Transfer type is burst transfer type */ \
                     PDMA_BURST_1 |   /* Burst size is 128. No effect in single transfer type */ \
                     PDMA_OP_SCATTER; /* Operation mode is scatter-gather mode */
    /*-----------------------------------------------------------------------------------------------------------------------------------------------------------
       Note: PDMA_REQ_BURST is only supported in memory-to-memory transfer mode.
             PDMA transfer type should be set as PDMA_REQ_SINGLE in memory-to-peripheral and peripheral-to-memory transfer mode,
             then above code will be modified as follows:
             g_u32DMAConfig = (0 << PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_WIDTH_32 | PDMA_SAR_FIX | PDMA_DAR_FIX | PDMA_BURST_1 | PDMA_REQ_SINGLE | PDMA_OP_SCATTER;
    -----------------------------------------------------------------------------------------------------------------------------------------------------------*/

    /*------------------------------------------------------------------------------------------------------
      Descriptor table 1 configuration:

             g_au32SrcArray0               transfer 1 times    g_au32DestArray
             ---------------------------   ----------------->  ---------------------------
            |            [0]            |                     |            [0]            |
             ---------------------------                       ---------------------------
             \                         /                       \                         /
                   32bits(one word)                                  32bits(one word)

        Operation mode = scatter-gather mode
        Next descriptor table = DMA_DESC[1](Descriptor table 2)
        transfer done and table empty interrupt = enable

        Transfer count = 1
        Transfer width = 32 bits(one word)
        Source address = g_au32SrcArray0
        Source address increment size = fixed address(no increment)
        Destination address = au8DestArray0
        Destination address increment size = fixed address(no increment)
        Transfer type = burst transfer

        Total transfer length = 1 * 32 bits
    ------------------------------------------------------------------------------------------------------*/
    DMA_DESC[0].ctl = g_u32DMAConfig;
    /* Configure source address */
    DMA_DESC[0].src = (uint32_t)g_au32SrcArray0; /* Ping-Pong buffer 1 */
    /* Configure destination address */
    DMA_DESC[0].dest = (uint32_t)&g_au32DestArray[0];
    /* Configure next descriptor table address */
    DMA_DESC[0].offset = (uint32_t)&DMA_DESC[1] - (PDMA->SCATBA); /* next operation table is table 2 */

    /*------------------------------------------------------------------------------------------------------
      Descriptor table 2 configuration:

             g_au32SrcArray1               transfer 1 times    g_au32DestArray
             ---------------------------   ----------------->  ---------------------------
            |            [0]            |                     |            [0]            |
             ---------------------------                       ---------------------------
             \                         /                       \                         /
                   32bits(one word)                                  32bits(one word)

        Operation mode = scatter-gather mode
        Next descriptor table = DMA_DESC[0](Descriptor table 1)
        transfer done and table empty interrupt = enable

        Transfer count = 1
        Transfer width = 32 bits(one word)
        Source address = g_au32SrcArray1
        Source address increment size = fixed address(no increment)
        Destination address = au8DestArray0
        Destination address increment size = fixed address(no increment)
        Transfer type = burst transfer

        Total transfer length = 1 * 32 bits
    ------------------------------------------------------------------------------------------------------*/
    DMA_DESC[1].ctl = g_u32DMAConfig;
    /* Configure source address */
    DMA_DESC[1].src = (uint32_t)g_au32SrcArray1; /* Ping-Pong buffer 2 */
    /* Configure destination address */
    DMA_DESC[1].dest = (uint32_t)&g_au32DestArray[0];
    /* Configure next descriptor table address */
    DMA_DESC[1].offset = (uint32_t)&DMA_DESC[0] - (PDMA->SCATBA); /* next operation table is table 1 */


    /* Enable transfer done interrupt */
    PDMA_EnableInt(4, PDMA_INT_TRANS_DONE);
    NVIC_EnableIRQ(PDMA_IRQn);
    g_u32IsTestOver = 0;

    /* Start PDMA operatin */
    PDMA_Trigger(4);

    while(1)
    {
        if(g_u32IsTestOver == 1)
        {
            g_u32IsTestOver = 0;
            printf("test done...\n");

            /* Close PDMA channel */
            PDMA_Close();
        }
    }
}


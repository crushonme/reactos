/*
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * PROJECT:         ReactOS Virtual DOS Machine
 * FILE:            mouse32.c
 * PURPOSE:         VDM 32-bit compatible MOUSE.COM driver
 * PROGRAMMERS:     Aleksandar Andrejevic <theflash AT sdf DOT lonestar DOT org>
 */

/* INCLUDES *******************************************************************/

#define NDEBUG

#include "emulator.h"
#include "cpu/cpu.h"
#include "int32.h"

#include "mouse32.h"
#include "bios/bios.h"

#include "io.h"
#include "dos32krnl/dos.h"

/* PRIVATE VARIABLES **********************************************************/

static BOOLEAN DriverEnabled = TRUE;
static MOUSE_DRIVER_STATE DriverState;

/* PRIVATE FUNCTIONS **********************************************************/

static VOID PaintMouseCursor(VOID)
{
    if (Bda->VideoMode <= 3)
    {
        WORD Character;
        DWORD VideoAddress = TO_LINEAR(TEXT_VIDEO_SEG, Bda->VideoPage * Bda->VideoPageSize);

        EmulatorReadMemory(&EmulatorContext,
                           VideoAddress
                           + (DriverState.Position.Y * Bda->ScreenColumns
                           + DriverState.Position.X) * sizeof(WORD),
                           (LPVOID)&Character,
                           sizeof(WORD));

        DriverState.Character = Character;
        Character &= DriverState.TextCursor.ScreenMask;
        Character ^= DriverState.TextCursor.CursorMask;

        EmulatorWriteMemory(&EmulatorContext,
                            VideoAddress
                            + (DriverState.Position.Y * Bda->ScreenColumns
                            + DriverState.Position.X) * sizeof(WORD),
                            (LPVOID)&Character,
                            sizeof(WORD));
    }
    else
    {
        // TODO: NOT IMPLEMENTED
        UNIMPLEMENTED;
    }
}

static VOID EraseMouseCursor(VOID)
{
    if (Bda->VideoMode <= 3)
    {
        DWORD VideoAddress = TO_LINEAR(TEXT_VIDEO_SEG, Bda->VideoPage * Bda->VideoPageSize);

        EmulatorWriteMemory(&EmulatorContext,
                            VideoAddress
                            + (DriverState.Position.Y * Bda->ScreenColumns
                            + DriverState.Position.X) * sizeof(WORD),
                            (LPVOID)&DriverState.Character,
                            sizeof(WORD));
    }
    else
    {
        // TODO: NOT IMPLEMENTED
        UNIMPLEMENTED;
    }
}

static VOID CallMouseUserHandlers(USHORT CallMask)
{
#if 0
    USHORT i;
    USHORT AX, BX, CX, DX, SI, DI;

    /* Call handler 0 */
    if ((DriverState.Handler0.CallMask & CallMask) != 0 &&
         DriverState.Handler0.Callback != (ULONG)NULL)
    {
        /*
         * Set the parameters for the callback.
         * NOTE: In text modes, the row and column will be reported
         *       as a multiple of the cell size, typically 8x8 pixels.
         */

        AX = getAX();
        BX = getBX();
        CX = getCX();
        DX = getDX();
        SI = getSI();
        DI = getDI();

        setAX(CallMask);
        setBX(DriverState.ButtonState);
        setCX(DriverState.Position.X);
        setDX(DriverState.Position.Y);
        setSI(DriverState.MickeysPerCellHoriz);
        setDI(DriverState.MickeysPerCellVert);

        DPRINT1("Calling Handler0 %04X:%04X with CallMask 0x%04X\n",
                HIWORD(DriverState.Handler0.Callback),
                LOWORD(DriverState.Handler0.Callback),
                CallMask);

        /* Call the callback */
        RunCallback16(&DosContext, DriverState.Handler0.Callback);

        setAX(AX);
        setBX(BX);
        setCX(CX);
        setDX(DX);
        setSI(SI);
        setDI(DI);
    }

    for (i = 0; i < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]); ++i)
    {
        /* Call the suitable handlers */
        if ((DriverState.Handlers[i].CallMask & CallMask) != 0 &&
             DriverState.Handlers[i].Callback != (ULONG)NULL)
        {
            /*
             * Set the parameters for the callback.
             * NOTE: In text modes, the row and column will be reported
             *       as a multiple of the cell size, typically 8x8 pixels.
             */

            AX = getAX();
            BX = getBX();
            CX = getCX();
            DX = getDX();
            SI = getSI();
            DI = getDI();

            setAX(CallMask);
            setBX(DriverState.ButtonState);
            setCX(DriverState.Position.X);
            setDX(DriverState.Position.Y);
            setSI(DriverState.MickeysPerCellHoriz);
            setDI(DriverState.MickeysPerCellVert);

            DPRINT1("Calling Handler[%d] %04X:%04X with CallMask 0x%04X\n",
                    i,
                    HIWORD(DriverState.Handlers[i].Callback),
                    LOWORD(DriverState.Handlers[i].Callback),
                    CallMask);

            /* Call the callback */
            RunCallback16(&DosContext, DriverState.Handlers[i].Callback);

            setAX(AX);
            setBX(BX);
            setCX(CX);
            setDX(DX);
            setSI(SI);
            setDI(DI);
        }
    }
#endif
}

static VOID WINAPI BiosMouseService(LPWORD Stack)
{
    switch (getAX())
    {
        /* Reset Driver */
        case 0x00:
        {
            SHORT i;

            DriverEnabled = TRUE;
            DriverState.ShowCount = 0;
            DriverState.ButtonState = 0;

            /* Set the default text cursor */
            DriverState.TextCursor.ScreenMask = 0xFFFF; /* Display everything */
            DriverState.TextCursor.CursorMask = 0xFF00; /* ... but with inverted attributes */

            /* Set the default graphics cursor */
            DriverState.GraphicsCursor.HotSpot.X = 3;
            DriverState.GraphicsCursor.HotSpot.Y = 1;

            DriverState.GraphicsCursor.ScreenMask[0] = 0xC3FF;  // 1100001111111111
            DriverState.GraphicsCursor.ScreenMask[1] = 0xC0FF;  // 1100000011111111
            DriverState.GraphicsCursor.ScreenMask[2] = 0xC07F;  // 1100000001111111
            DriverState.GraphicsCursor.ScreenMask[3] = 0xC01F;  // 1100000000011111
            DriverState.GraphicsCursor.ScreenMask[4] = 0xC00F;  // 1100000000001111
            DriverState.GraphicsCursor.ScreenMask[5] = 0xC007;  // 1100000000000111
            DriverState.GraphicsCursor.ScreenMask[6] = 0xC003;  // 1100000000000011
            DriverState.GraphicsCursor.ScreenMask[7] = 0xC007;  // 1100000000000111
            DriverState.GraphicsCursor.ScreenMask[8] = 0xC01F;  // 1100000000011111
            DriverState.GraphicsCursor.ScreenMask[9] = 0xC01F;  // 1100000000011111
            DriverState.GraphicsCursor.ScreenMask[10] = 0xC00F; // 1100000000001111
            DriverState.GraphicsCursor.ScreenMask[11] = 0xC60F; // 1100011000001111
            DriverState.GraphicsCursor.ScreenMask[12] = 0xFF07; // 1111111100000111
            DriverState.GraphicsCursor.ScreenMask[13] = 0xFF07; // 1111111100000111
            DriverState.GraphicsCursor.ScreenMask[14] = 0xFF87; // 1111111110000111
            DriverState.GraphicsCursor.ScreenMask[15] = 0xFFCF; // 1111111111001111

            DriverState.GraphicsCursor.CursorMask[0] = 0x0000;  // 0000000000000000
            DriverState.GraphicsCursor.CursorMask[1] = 0x1C00;  // 0001110000000000
            DriverState.GraphicsCursor.CursorMask[2] = 0x1F00;  // 0001111100000000
            DriverState.GraphicsCursor.CursorMask[3] = 0x1F80;  // 0001111110000000
            DriverState.GraphicsCursor.CursorMask[4] = 0x1FE0;  // 0001111111100000
            DriverState.GraphicsCursor.CursorMask[5] = 0x1FF0;  // 0001111111110000
            DriverState.GraphicsCursor.CursorMask[6] = 0x1FF8;  // 0001111111111000
            DriverState.GraphicsCursor.CursorMask[7] = 0x1FE0;  // 0001111111100000
            DriverState.GraphicsCursor.CursorMask[8] = 0x1FC0;  // 0001111111000000
            DriverState.GraphicsCursor.CursorMask[9] = 0x1FC0;  // 0001111111000000
            DriverState.GraphicsCursor.CursorMask[10] = 0x19E0; // 0001100111100000
            DriverState.GraphicsCursor.CursorMask[11] = 0x00E0; // 0000000011100000
            DriverState.GraphicsCursor.CursorMask[12] = 0x0070; // 0000000001110000
            DriverState.GraphicsCursor.CursorMask[13] = 0x0070; // 0000000001110000
            DriverState.GraphicsCursor.CursorMask[14] = 0x0030; // 0000000000110000
            DriverState.GraphicsCursor.CursorMask[15] = 0x0000; // 0000000000000000

            /* Initialize the counters */
            DriverState.HorizCount = DriverState.VertCount = 0;

            for (i = 0; i < NUM_MOUSE_BUTTONS; i++)
            {
                DriverState.PressCount[i] = DriverState.ReleaseCount[i] = 0;
            }

            /* Initialize the resolution */
            DriverState.MickeysPerCellHoriz = 8;
            DriverState.MickeysPerCellVert = 16;

            /* Return mouse information */
            setAX(0xFFFF);  // Hardware & driver installed
            setBX(NUM_MOUSE_BUTTONS);

            break;
        }

        /* Show Mouse Cursor */
        case 0x01:
        {
            DriverState.ShowCount++;
            if (DriverState.ShowCount > 0) PaintMouseCursor();

            break;
        }

        /* Hide Mouse Cursor */
        case 0x02:
        {
            DriverState.ShowCount--;
            if (DriverState.ShowCount <= 0) EraseMouseCursor();

            break;
        }

        /* Return Position And Button Status */
        case 0x03:
        {
            setBX(DriverState.ButtonState);
            setCX(DriverState.Position.X);
            setDX(DriverState.Position.Y);
            break;
        }

        /* Position Mouse Cursor */
        case 0x04:
        {
            POINT Point;

            Point.x = getCX();
            Point.y = getDX();

            ClientToScreen(GetConsoleWindow(), &Point);
            SetCursorPos(Point.x, Point.y);

            break;
        }

        /* Return Button Press Data */
        case 0x05:
        {
            WORD Button = getBX();

            setAX(DriverState.ButtonState);
            setBX(DriverState.PressCount[Button]);
            setCX(DriverState.LastPress[Button].X);
            setDX(DriverState.LastPress[Button].Y);

            /* Reset the counter */
            DriverState.PressCount[Button] = 0;

            break;
        }

        /* Return Button Release Data */
        case 0x06:
        {
            WORD Button = getBX();

            setAX(DriverState.ButtonState);
            setBX(DriverState.ReleaseCount[Button]);
            setCX(DriverState.LastRelease[Button].X);
            setDX(DriverState.LastRelease[Button].Y);

            /* Reset the counter */
            DriverState.ReleaseCount[Button] = 0;

            break;

        }

        /* Define Graphics Cursor */
        case 0x09:
        {
            PWORD MaskBitmap = (PWORD)SEG_OFF_TO_PTR(getES(), getDX());

            DriverState.GraphicsCursor.HotSpot.X = getBX();
            DriverState.GraphicsCursor.HotSpot.Y = getCX();

            RtlMoveMemory(DriverState.GraphicsCursor.ScreenMask,
                          MaskBitmap,
                          sizeof(DriverState.GraphicsCursor.ScreenMask));

            RtlMoveMemory(DriverState.GraphicsCursor.CursorMask,
                          &MaskBitmap[16],
                          sizeof(DriverState.GraphicsCursor.CursorMask));

            break;
        }

        /* Define Text Cursor */
        case 0x0A:
        {
            USHORT BX = getBX();

            if (BX == 0x0000)
            {
                /* Define software cursor */
                DriverState.TextCursor.ScreenMask = getCX();
                DriverState.TextCursor.CursorMask = getDX();
            }
            else if (BX == 0x0001)
            {
                /* Define hardware cursor */
                DPRINT1("Defining hardware cursor is unimplemented\n");
                UNIMPLEMENTED;
                // CX == start scan line
                // DX == end scan line
            }
            else
            {
                DPRINT1("Invalid BX value 0x%04X\n", BX);
            }

            break;
        }

        /* Read Motion Counters */
        case 0x0B:
        {
            setCX(DriverState.HorizCount);
            setDX(DriverState.VertCount);

            /* Reset the counters */
            DriverState.HorizCount = DriverState.VertCount = 0;

            break;
        }

        /* Define Interrupt Subroutine Parameters, compatible MS MOUSE v1.0+ */
        case 0x0C:
        {
            DriverState.Handler0.CallMask = getCX();
            DriverState.Handler0.Callback = MAKELONG(getDX(), getES()); // Far pointer to the callback
            DPRINT1("Define callback 0x%04X, %04X:%04X\n",
                    DriverState.Handler0.CallMask,
                    HIWORD(DriverState.Handler0.Callback),
                    LOWORD(DriverState.Handler0.Callback));
            break;
        }

        /* Define Mickey/Pixel Ratio */
        case 0x0F:
        {
            DriverState.MickeysPerCellHoriz = getCX();
            DriverState.MickeysPerCellVert  = getDX();
            break;
        }

        /* Exchange Interrupt Subroutines, compatible MS MOUSE v3.0+ (see function 0x0C) */
        case 0x14:
        {
            USHORT OldCallMask = DriverState.Handler0.CallMask;
            ULONG  OldCallback = DriverState.Handler0.Callback;

            DriverState.Handler0.CallMask = getCX();
            DriverState.Handler0.Callback = MAKELONG(getDX(), getES()); // Far pointer to the callback

            /* Return old callmask in CX and callback vector in ES:DX */
            setCX(OldCallMask);
            setES(HIWORD(OldCallback));
            setDX(LOWORD(OldCallback));

            break;
        }

        /* Return Driver Storage Requirements */
        case 0x15:
        {
            setBX(sizeof(MOUSE_DRIVER_STATE));
            break;
        }

        /* Save Driver State */
        case 0x16:
        {
            *((PMOUSE_DRIVER_STATE)SEG_OFF_TO_PTR(getES(), getDX())) = DriverState;
            break;
        }

        /* Restore Driver State */
        case 0x17:
        {
            DriverState = *((PMOUSE_DRIVER_STATE)SEG_OFF_TO_PTR(getES(), getDX()));
            break;
        }

        /* Set Alternate Mouse User Handler, compatible MS MOUSE v6.0+ */
        case 0x18:
        {
            /*
             * Up to three handlers can be defined by separate calls to this
             * function, each with a different combination of shift states in
             * the call mask; calling this function again with a call mask of
             * 0000h undefines the specified handler (official documentation);
             * specifying the same call mask and an address of 0000h:0000h
             * undefines the handler (real life).
             * See Ralf Brown: http://www.ctyme.com/intr/rb-5981.htm
             * for more information.
             */

            USHORT i;
            USHORT CallMask = getCX();
            ULONG  Callback = MAKELONG(getDX(), getES()); // Far pointer to the callback
            BOOLEAN Success = FALSE;

            DPRINT1("Define v6.0+ callback 0x%04X, %04X:%04X\n",
                    CallMask, HIWORD(Callback), LOWORD(Callback));

            if (CallMask == 0x0000)
            {
                /*
                 * Find the handler entry corresponding to the given
                 * callback and undefine it.
                 */
                for (i = 0; i < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]); ++i)
                {
                    if (DriverState.Handlers[i].Callback == Callback)
                    {
                        /* Found it, undefine the handler */
                        DriverState.Handlers[i].CallMask = 0x0000;
                        DriverState.Handlers[i].Callback = (ULONG)NULL;
                        Success = TRUE;
                        break;
                    }
                }
            }
            else if (Callback == (ULONG)NULL)
            {
                /*
                 * Find the handler entry corresponding to the given
                 * callmask and undefine it.
                 */
                for (i = 0; i < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]); ++i)
                {
                    if (DriverState.Handlers[i].CallMask == CallMask)
                    {
                        /* Found it, undefine the handler */
                        DriverState.Handlers[i].CallMask = 0x0000;
                        DriverState.Handlers[i].Callback = (ULONG)NULL;
                        Success = TRUE;
                        break;
                    }
                }
            }
            else
            {
                /*
                 * Try to find a handler entry corresponding to the given
                 * callmask to redefine it, otherwise find an empty handler
                 * entry and set the new handler in there.
                 */

                USHORT EmptyHandler = 0xFFFF; // Invalid handler

                for (i = 0; i < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]); ++i)
                {
                    /* Find the first empty handler */
                    if (EmptyHandler == 0xFFFF &&
                        DriverState.Handlers[i].CallMask == 0x0000 &&
                        DriverState.Handlers[i].Callback == (ULONG)NULL)
                    {
                        EmptyHandler = i;
                    }

                    if (DriverState.Handlers[i].CallMask == CallMask)
                    {
                        /* Found it, redefine the handler */
                        DriverState.Handlers[i].CallMask = CallMask;
                        DriverState.Handlers[i].Callback = Callback;
                        Success = TRUE;
                        break;
                    }
                }

                /*
                 * If we haven't found anything and we found
                 * an empty handler, set it.
                 */
                if (!Success && EmptyHandler != 0xFFFF
                    /* && EmptyHandler < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]) */)
                {
                    DriverState.Handlers[EmptyHandler].CallMask = CallMask;
                    DriverState.Handlers[EmptyHandler].Callback = Callback;
                    Success = TRUE;
                }
            }

            /* If we failed, set error code */
            if (!Success) setAX(0xFFFF);

            break;
        }

        /* Return User Alternate Interrupt Vector, compatible MS MOUSE v6.0+ */
        case 0x19:
        {
            USHORT i;
            USHORT CallMask = getCX();
            ULONG  Callback;
            BOOLEAN Success = FALSE;

            /*
             * Find the handler entry corresponding to the given callmask.
             */
            for (i = 0; i < sizeof(DriverState.Handlers)/sizeof(DriverState.Handlers[0]); ++i)
            {
                if (DriverState.Handlers[i].CallMask == CallMask)
                {
                    /* Found it */
                    Callback = DriverState.Handlers[i].Callback;
                    Success = TRUE;
                    break;
                }
            }

            if (Success)
            {
                /* Return the callback vector in BX:DX */
                setBX(HIWORD(Callback));
                setDX(LOWORD(Callback));
            }
            else
            {
                /* We failed, set error code */
                setCX(0x0000);
            }

            break;
        }

        /* Disable Mouse Driver */
        case 0x1F:
        {
            setES(0x0000);
            setBX(0x0000);

            DriverEnabled = FALSE;
            break;
        }

        /* Enable Mouse Driver */
        case 0x20:
        {
            DriverEnabled = TRUE;
            break;
        }

        default:
        {
            DPRINT1("BIOS Function INT 33h, AX = 0x%04X NOT IMPLEMENTED\n", getAX());
        }
    }
}

/* PUBLIC FUNCTIONS ***********************************************************/

VOID MouseBiosUpdatePosition(PCOORD NewPosition)
{
    SHORT DeltaX = NewPosition->X - DriverState.Position.X;
    SHORT DeltaY = NewPosition->Y - DriverState.Position.Y;

    if (!DriverEnabled) return;

    DriverState.HorizCount += (DeltaX * (SHORT)DriverState.MickeysPerCellHoriz) / 8;
    DriverState.VertCount  += (DeltaY * (SHORT)DriverState.MickeysPerCellVert ) / 8;

    if (DriverState.ShowCount > 0)
    {
        EraseMouseCursor();
        DriverState.Position = *NewPosition;
        PaintMouseCursor();
    }

    /* Call the mouse handlers */
    CallMouseUserHandlers(0x0001); // We use MS MOUSE v1.0+ format
}

VOID MouseBiosUpdateButtons(WORD ButtonState)
{
    USHORT i;
    USHORT CallMask = 0x0000; // We use MS MOUSE v1.0+ format

    if (!DriverEnabled) return;

    for (i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        BOOLEAN OldState = (DriverState.ButtonState >> i) & 1;
        BOOLEAN NewState = (ButtonState >> i) & 1;

        if (NewState > OldState)
        {
            /* Mouse press */
            DriverState.PressCount[i]++;
            DriverState.LastPress[i] = DriverState.Position;

            CallMask |= (1 << (2 * i + 1));
        }
        else if (NewState < OldState)
        {
            /* Mouse release */
            DriverState.ReleaseCount[i]++;
            DriverState.LastRelease[i] = DriverState.Position;

            CallMask |= (1 << (2 * i + 2));
        }
    }

    DriverState.ButtonState = ButtonState;

    /* Call the mouse handlers */
    CallMouseUserHandlers(CallMask);
}

BOOLEAN DosMouseInitialize(VOID)
{
    /* Clear the state */
    ZeroMemory(&DriverState, sizeof(DriverState));

    /* Initialize the interrupt handler */
    RegisterDosInt32(BIOS_MOUSE_INTERRUPT, BiosMouseService);

    return TRUE;
}

VOID DosMouseCleanup(VOID)
{
    if (DriverState.ShowCount > 0) EraseMouseCursor();
}

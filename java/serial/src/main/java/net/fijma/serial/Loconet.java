package net.fijma.serial;

import java.util.Arrays;
import java.util.stream.Collectors;

public class Loconet {

    public final Event<String> decoded = new Event<>();
    private int[] bs = new int[128];
    private int length=0;
    private int remaining = 0;

    private int opcodeLength(int x) {
        // if x denotes opcode (msb set), that is, start of instruction, return length of instruction else zero
        // 1000 0000, 0x80 (length 2)
        // 1010 0000, 0xA0 (length 4)
        // 1100 0000, 0xC0 (length 6)
        // 1110 0000, 0xE0 (length N, variable)
        switch (x & 0xE0) {
            case 0x80:
                return 2;
            case 0xA0:
                return 4;
            case 0xC0:
                return 6;
            case 0xE0:
                return 255; // var
        }
        return 0; // no opcode
    }

    private String hexString(int buffer[], int length) {
        return Arrays.stream(buffer).limit(length).mapToObj((b)->String.format("%02x", b)).collect(Collectors.joining(" "));
    }

    public void pushByte(int b) {

        // if the next byte starts a new instruction, instructionLength != 0
        int opcodeLength = opcodeLength(b);

        // if we expect start of new instruction, but did not get it, discard this byte
        if (remaining == 0 && opcodeLength == 0) {
            // System.out.println("skipping lone byte, " + b);
            return;
        }

        // if this byte starts a new instruction, while previous one is incomplete,
        // discard the current instruction and start a new one
        if (remaining > 0 && opcodeLength > 0) {
            // System.out.println("skipping unclosed bytes " + hexString(bs, length));
            length = 0;
            remaining = 0;
        }

        // happy!
        if (opcodeLength > 0) {
            assert remaining == 0;
            remaining = opcodeLength;
        }

        if (remaining == 254) {
            // this is the second byte of a variabele length, eg, the length byte
            remaining = b-1; // one for the instruction byte we already have
        }
        bs[length++] = b;
        remaining--;

        if (remaining == 0) {
            // complete, decode current instruction
            decoded.trigger(decode(bs, length));
            length = 0;
        }
    }

    private String opcodeName(int x) {
        // see also:
        // https://wiki.rocrail.net/doku.php?id=loconet:ln-pe-en
        // https://klaus.merkert.info/eisenbahn/digital/loconet/
        switch (x) {
            case 0x85:
                return "OPC_IDLE";
            case 0x83:
                return "OPC_GPON";
            case 0x82:
                return "OPC_GPOFF";
            case 0x81:
                return "OPC_BUSY";
            case 0xA0:
                return "OPC_LOCO_SPD";
            case 0xA1:
                return "OPC_LOCO_DIF";
            case 0xBF:
                return "OPC_LOCO_ADR";
            case 0xBE:
                return "OPC_LOCO_ADR_EXT";
            case 0xBC:
                return "OPC_SW_STATE";
            case 0xBB:
                return "OPC_RQ_SL_DATA";
            case 0xBA:
                return "OPC_MOVE_SLOTS";
            case 0xB5:
                return "OPC_SLOT_STAT1";
            case 0xB4:
                return "OPC_LONG_ACK";
            case 0xEF:
                return "OPC_WR_SL_DATA";
            case 0xED:
                return "OPC_IMM_PACKET"; // meaning of params not further split-out
            case 0xE7:
                return "OPC_RD_SL_DATA";
            case 0xE6:
                return "OPC_RD_SL_DATA_EXT";
            case 0xE5:
                return "OPC_PEER_XFR"; // not yet split out further params
            // never seen in reality and in documentation
            default:
                return "UNKNOWN_OPCODE";
        }
    }

    /*
        UNKNOWN_OPCODE[0xBF][bf 00 16 56]
        OPC_RD_SL_DATA[0xE7](SLOT=8,STAT=32,ADR=22, ETC)[e7 0e 08 20 16 00 00 07 00 00 00 00 00 2f]

        UNKNOWN_OPCODE[0xBF][bf 01 16 57]
        OPC_RD_SL_DATA[0xE7](SLOT=9,STAT=32,ADR=22, ETC)[e7 0e 09 20 16 00 00 07 00 01 00 00 00 2f]


        UNDOCUMENTED_OPCODE[0xBE][be 00 16 57]
        PROG_ABORT[0xE6][e6 15 00 08 20 16 00 07 00 00 00 00 00 00 52 11 00 00 00 00 76]

        UNDOCUMENTED_OPCODE[0xBE][be 01 16 56]
        PROG_ABORT[0xE6][e6 15 00 09 20 16 01 07 00 00 00 00 00 00 52 11 00 00 00 00 76]

        OPC_LOCO_ADR_EXT(ADR=0x00,0x51=81) [be 00 51 10]
        OPC_RD_SL_DATA_EXT(SLOT=6,STAT=32,ADR=0x00,0x51=81, ETC) [e6 15 00 06 20 51 00 07 00 00 00 00 00 00 52 11 00 00 00 00 3f]
        OPC_MOVE_SLOTS(SRC=6,DST=6) [ba 06 06 45]
        OPC_RD_SL_DATA(SLOT=6,STAT=48,ADR=0x00,0x51=81, ETC) [e7 0e 06 30 51 00 00 07 00 00 00 00 00 76]
        OPC_LOCO_SPD(SLOT=6,SPD=13) [a0 06 0d 54]

        */

    private static String formatAddress(int low, int high) {
        return String.format("0x%02X", high) +
                String.format(",0x%02X", low) + "=" + (high*128+low);
    }

    private String decode(int buffer[], int length) {
        // we can assume length of buffer is correct, as we read exactly the required amount of bytes before calling this function

        StringBuilder sb = new StringBuilder(opcodeName(buffer[0]));

        switch (buffer[0]) {
            // 2 byte instructions:
            case 0x85:
                // FORCE IDLE STATE
                break;
            case 0x83:
                // GLOBAL POWER ON
                break;
            case 0x82:
                // GLOBAL POWER OFF
                break;
            case 0x81:
                // MASTER BUSY
                break;
            // 4  byte instructions:
            case 0xA0:
                sb.append("(SLOT=").append(buffer[1]).
                        append(",SPD=").append(buffer[2]).append(")");
                break;
            case 0xA1:
                sb.append("(SLOT=").append(buffer[1]).
                        append(",DIR=").append((buffer[2] & 0x20) == 0x20 ? "BACKWARDS" : "FORWARDS").
                        append(",F0F4F3F2F1=").append(Integer.toBinaryString(buffer[2] & 0x1F)).append(")");
                break;
            case 0xBF:
            case 0xBE:
                sb.append("(ADR=").append(formatAddress(buffer[2], buffer[1])).append(")");
                break;
            case 0xBC:
                sb.append("(SW1=").append(String.format("0x%02X", buffer[1])).
                        append(",SW2=").append(String.format("0x%02X", buffer[2])).append(")");
                break;
            case 0xBB:
                sb.append("(SLOT=").append(buffer[1]).
                        append(",ZERO=").append(String.format("0x%02X", buffer[2])).append(")");
                break;
            case 0xBA:
                sb.append("(SRC=").append(buffer[1]).
                        append(",DST=").append(buffer[2]).append(")");
                break;
            case 0xB5:
                sb.append("(SLOT=").append(buffer[1]).
                        append(",STAT1=").append(String.format("0x%02X", buffer[2])).append(")");
                break;
            case 0xB4:
                sb.append("(LOPC=").append(opcodeName(buffer[1] | 0x80)).
                        append(",ACK1=").append(String.format("0x%02X", buffer[2])).append(")");
                break;
            case 0xEF:
            case 0xE7:
                if (buffer[1] == 0x0E) {
                    sb.append("(SLOT=").append(buffer[2]).
                            append(",STAT=").append(buffer[3]).
                            append(",ADR=").append(formatAddress(buffer[4], buffer[9])).append(", ETC)");
                } else {
                    sb.append("(unexpected length=").append(buffer[1]).append(")");
                }
                break;
            case 0xE6:
                if (buffer[1] == 0x15) {
                    sb.append("(SLOT=").append(buffer[3]).
                            append(",STAT=").append(buffer[4]).
                            append(",ADR=").append(formatAddress(buffer[5], buffer[6])).append(", ETC)");
                } else {
                    sb.append("(unexpected length=").append(buffer[1]).append(")");
                }
                break;
            default:
        }

        sb.append(" [").append(hexString(buffer, length)).append("]");


        // calculate checksum and compare to actual
        int checksum = buffer[0] & 0xFF;
        for (int i=1; i<length-1; ++i) {
            checksum = checksum ^ (buffer[i] & 0xFF);
        }
        checksum = ~checksum & 0xFF;
        if (checksum != buffer[length-1]) {
            sb.append(" invalid checksum, expected:").append(String.format("%02X", buffer[length-1])).append(", actual:").append(String.format("%02X", checksum));
        }

        return sb.toString();
    }
}

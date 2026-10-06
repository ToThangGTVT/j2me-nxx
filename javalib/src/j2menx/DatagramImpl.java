package j2menx;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.UTFDataFormatException;
import javax.microedition.io.Datagram;

// Gói UDP: bộ đệm + con trỏ đọc/ghi (DataInput / DataOutput)
public class DatagramImpl implements Datagram {
    byte[] buf;
    int offset;
    int length;
    int pos;            // vị trí đọc/ghi tương đối offset
    String address;

    public DatagramImpl(byte[] buf, int length, String address) {
        this.buf = buf;
        this.length = length;
        this.address = address;
    }

    public String getAddress() { return address; }
    public byte[] getData() { return buf; }
    public int getLength() { return length; }
    public int getOffset() { return offset; }

    public void setAddress(String addr) throws IOException {
        if (addr == null || !addr.startsWith("datagram://")) {
            throw new IllegalArgumentException("Dia chi khong hop le: " + addr);
        }
        address = addr;
    }

    public void setAddress(Datagram reference) {
        address = reference.getAddress();
    }

    public void setLength(int len) {
        if (len < 0 || offset + len > buf.length) {
            throw new IllegalArgumentException();
        }
        length = len;
    }

    public void setData(byte[] buffer, int off, int len) {
        if (off < 0 || len < 0 || off + len > buffer.length) {
            throw new IllegalArgumentException();
        }
        buf = buffer;
        offset = off;
        length = len;
        pos = 0;
    }

    public void reset() {
        pos = 0;
        length = 0;
    }

    // --- DataInput
    private int next() throws IOException {
        if (pos >= length) {
            throw new EOFException();
        }
        return buf[offset + pos++] & 0xff;
    }

    public void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }

    public void readFully(byte[] b, int off, int len) throws IOException {
        if (pos + len > length) {
            throw new EOFException();
        }
        System.arraycopy(buf, offset + pos, b, off, len);
        pos += len;
    }

    public int skipBytes(int n) {
        int k = Math.min(n, length - pos);
        pos += k;
        return k;
    }

    public boolean readBoolean() throws IOException { return next() != 0; }
    public byte readByte() throws IOException { return (byte) next(); }
    public int readUnsignedByte() throws IOException { return next(); }
    public short readShort() throws IOException { return (short) ((next() << 8) | next()); }
    public int readUnsignedShort() throws IOException { return (next() << 8) | next(); }
    public char readChar() throws IOException { return (char) ((next() << 8) | next()); }
    public int readInt() throws IOException { return (next() << 24) | (next() << 16) | (next() << 8) | next(); }
    public long readLong() throws IOException { return ((long) readInt() << 32) | (readInt() & 0xffffffffL); }
    public float readFloat() throws IOException { return Float.intBitsToFloat(readInt()); }
    public double readDouble() throws IOException { return Double.longBitsToDouble(readLong()); }
    public String readUTF() throws IOException { return DataInputStream.readUTF(this); }

    // --- DataOutput: ghi tiếp sau dữ liệu đã có, tăng length
    private void put(int b) throws IOException {
        if (offset + pos >= buf.length) {
            throw new IOException("Datagram day");
        }
        buf[offset + pos++] = (byte) b;
        if (pos > length) {
            length = pos;
        }
    }

    public void write(int b) throws IOException { put(b); }
    public void write(byte[] b) throws IOException { write(b, 0, b.length); }

    public void write(byte[] b, int off, int len) throws IOException {
        for (int i = 0; i < len; i++) {
            put(b[off + i]);
        }
    }

    public void writeBoolean(boolean v) throws IOException { put(v ? 1 : 0); }
    public void writeByte(int v) throws IOException { put(v); }
    public void writeShort(int v) throws IOException { put(v >> 8); put(v); }
    public void writeChar(int v) throws IOException { put(v >> 8); put(v); }
    public void writeInt(int v) throws IOException { put(v >> 24); put(v >> 16); put(v >> 8); put(v); }
    public void writeLong(long v) throws IOException { writeInt((int) (v >>> 32)); writeInt((int) v); }
    public void writeFloat(float v) throws IOException { writeInt(Float.floatToIntBits(v)); }
    public void writeDouble(double v) throws IOException { writeLong(Double.doubleToLongBits(v)); }

    public void writeChars(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) {
            writeChar(s.charAt(i));
        }
    }

    public void writeUTF(String s) throws IOException {
        java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
        new java.io.DataOutputStream(bo).writeUTF(s);
        byte[] b = bo.toByteArray();
        if (b.length > 65537) {
            throw new UTFDataFormatException();
        }
        write(b, 0, b.length);
    }
}

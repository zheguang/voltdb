/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.voltcore.utils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel.MapMode;
import java.util.Arrays;
import java.util.concurrent.ConcurrentHashMap;

import org.voltdb.utils.VoltFile;
import org.xerial.snappy.SnappyInputStream;
import org.xerial.snappy.SnappyOutputStream;

public class OnDemandBinaryLogger {
    private static class Stuff {
        DataOutputStream dos;
        ByteBuffer count;
    }

    public static ConcurrentHashMap<String, Stuff> m_files = new ConcurrentHashMap<String, Stuff>();
    public static String path;

    public static void flush() {
        for (Stuff s : m_files.values()) {
            synchronized (s) {
                try {
                    if (s.count != null) {
                        s.dos.flush();
                        s.count = null;
                        s.dos = null;
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private static Stuff getStream(final String name) throws IOException {
        Stuff s = m_files.get(name);
        if (s == null) {
            File f = new VoltFile(path, name);
            f.delete();
            RandomAccessFile ras = new RandomAccessFile(f, "rw");
            ras.seek(8);
            SnappyOutputStream sos = new SnappyOutputStream(new FileOutputStream(ras.getFD()));
            DataOutputStream dos = new DataOutputStream(sos);
            s = new Stuff();
            s.dos = dos;
            s.count = ras.getChannel().map(MapMode.READ_WRITE, 0, 8);
            s.count.order(ByteOrder.nativeOrder());
            m_files.put(name, s);
        }
        return s;
    }

    public static void log(final String name, long value) {
        try {
            Stuff s = getStream(name);
            synchronized (s) {
                if (s.count != null) {
                    s.dos.writeLong(value);
                    s.count.putLong(0, s.count.getLong(0) + 1);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static long[] filesToArray(String files[]) throws Exception {
        long count = 0;
        int qq = 0;
        long counts[] = new long[files.length];
        for (String file : files) {
            FileInputStream fis = new FileInputStream(file);
            counts[qq] = fis.getChannel().map(MapMode.READ_ONLY, 0, 8).order(ByteOrder.nativeOrder()).getLong();
            count += counts[qq++];
            fis.close();
        }

        final long retval[] = new long[(int)count];

        qq = 0;
        int zz = 0;
        for (String file : files) {
            FileInputStream fis = new FileInputStream(file);
            fis.getChannel().position(8);
            SnappyInputStream sis = new SnappyInputStream(fis);

            DataInputStream dis = new DataInputStream(sis);
            for (int ii = 0; ii < counts[qq]; ii++) {
                retval[zz++] = dis.readLong();
            }
            qq++;
        }
        Arrays.sort(retval);
        return retval;
    }

    public static void main(String args[]) throws Exception {
        final long arrayTwo[] = filesToArray(new String[] { "/home/aweisberg/hashmismatch/spproceduretasklog_site_4294967307.blog" });
        final long arrayOne[] = filesToArray(new String[] { "/home/aweisberg/hashmismatch/polltasklog_site_4294967307.blog" });
//        final long arrayOne[] = filesToArray(new String[] { "/home/aweisberg/hashmismatch/runfragmenttasklog_site_10.blog", "/home/aweisberg/hashmismatch/runspproceduretasklog_site_10.blog"  });
//        final long arrayTwo[] = filesToArray(new String[] { "/home/aweisberg/hashmismatch/polltasklog_site_10.blog" });
        if (arrayTwo.length > arrayOne.length) {
            System.out.println(arrayTwo.length + " was greater than " + arrayOne.length);
        }

        int didNotFindCount = 0;
        int foundCount = 0;
        int zz = 0;
        long lastToFind = Long.MIN_VALUE;
        for (int ii = 0; ii < arrayTwo.length; ii++) {
            int zzStart = zz;
            final long toFind = arrayTwo[ii];
            if (toFind == lastToFind) continue;
            lastToFind = toFind;
            boolean found = false;
            while (zz < arrayOne.length) {
                if (toFind == arrayOne[zz]) {
                    found = true;
                    foundCount++;
                    break;
                }
                zz++;
            }
            if (!found) {
                zz = zzStart;
                if (toFind > 3619633595678719L) {
                    System.out.println("Couldn't find " + toFind + " position " + ii);
                }
                didNotFindCount++;
            }
        }
        System.out.println("Found " + foundCount + " did not find " + didNotFindCount);
    }
//
//  public static void main(String args[]) throws Exception {
//      FileInputStream fis = new FileInputStream(new File("/home/aweisberg/hashmismatch/polltasklog_site_10.blog"));
//      fis.getChannel().position(8);
//
//      final long count = fis.getChannel().map(MapMode.READ_ONLY, 0, 8).order(ByteOrder.nativeOrder()).getLong();
//
//      SnappyInputStream sis = new SnappyInputStream(fis);
//
//      DataInputStream dis = new DataInputStream(sis);
//
//      FileInputStream fis2 = new FileInputStream(new File("/home/aweisberg/hashmismatch/fragmenttasklog_site_10.blog"));
//      fis2.getChannel().position(8);
//
//      final long count2 = fis2.getChannel().map(MapMode.READ_ONLY, 0, 8).order(ByteOrder.nativeOrder()).getLong();
//
//      SnappyInputStream sis2 = new SnappyInputStream(fis2);
//
//      DataInputStream dis2 = new DataInputStream(sis2);
//
//      if (count2 > count) {
//          System.out.println(count2 + " was greater than " + count);
//          return;
//      }
//
//      final long arrayOne[] = new long[(int)count];
//      for (int ii = 0; ii < count; ii++) {
//          arrayOne[ii] = dis.readLong();
//      }
//
//      final long arrayTwo[] = new long[(int)count2];
//      for (int ii = 0; ii < count2; ii++) {
//          arrayTwo[ii] = dis2.readLong();
//      }
//
//      int didNotFindCount = 0;
//      int foundCount = 0;
//      int zz = 0;
//      for (int ii = 0; ii < count2; ii++) {
//          int zzStart = zz;
//          final long toFind = arrayTwo[ii];
//          boolean found = false;
//          while (zz < count) {
//              if (toFind == arrayOne[zz]) {
//                  found = true;
//                  foundCount++;
//                  break;
//              }
//              zz++;
//          }
//          if (!found) {
//              zz = zzStart;
//              System.out.println("Couldn't find " + toFind + " position " + ii);
//              didNotFindCount++;
//          }
//      }
//      System.out.println("Found " + foundCount + " did not find " + didNotFindCount);
//  }

//    public static void main(String args[]) throws Exception {
//        final String op = args[0];
//        final String file = args[1];
//        int tail = 0;
//        if (op.equalsIgnoreCase("tail")) {
//            tail = Integer.valueOf(args[2]);
//        }
//
//        long txnid = 0;
//        if (op.equalsIgnoreCase("grep")) {
//            txnid = Long.valueOf(args[2]);
//        }
//
//        FileInputStream fis = new FileInputStream(file);
//        fis.getChannel().position(8);
//
//        final long count = fis.getChannel().map(MapMode.READ_ONLY, 0, 8).order(ByteOrder.nativeOrder()).getLong();
//
//        SnappyInputStream sis = new SnappyInputStream(fis);
//
//        DataInputStream dis = new DataInputStream(sis);
//
//        if (op.equalsIgnoreCase("tail")) {
//            long skip = count > tail ? count - tail : 0;
//            for (long ii = 0; ii < skip; ii++) {
//                dis.readLong();
//            }
//
//            for (int ii = 0; ii < tail; ii++) {
//                System.out.println(dis.readLong());
//            }
//        } else if (op.equalsIgnoreCase("grep")) {
//            for (long ii = 0; ii < count; ii++) {
//                if (dis.readLong() == txnid) {
//                    System.out.println(ii + ": " + txnid);
//                }
//            }
//        } else {
//            System.err.println("Unsupported operation " + op);
//        }
//
//    }


}

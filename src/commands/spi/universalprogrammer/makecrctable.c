  /* This program will write six C routines for the calculation of
   * the following CRC's. */
  
  /* The CRC polynomial.
   * These 4 values define the crc-polynomial.
   * If you change them, you must change crctab[]'s initial value to what is
   * printed by initcrctab() [see 'compile with -DMAKETAB' above].
   */
  
  /* This tables assumes CCITT is MSB first.  Swapped means LSB first.  In that
   * case the polynomial is also swapped
   */
  
  /* 16 bit crc's */
  /* Value used by:            CCITT    KERMIT   ARC      BINHEX    */
  /* the poly:                 0x1021   0x8408   0xA001   0x1021    */
  /* original:                 0x1021   0x1021   0x8005   0x1021    */
  /* init value:               -1       0        0        0         */
  /* swapped:                  no       yes      yes      no        */
  /* bits in CRC:              16       16       16       16        */
  /* ARC used by LHARC, ZOO, STUFFIT                                */
  /* BINHEX used by XMODEM, PACKIT                                  */
  
  /* 32 bit crc's */
  /* Value used by:            CCITT32        ZIP                   */
  /* the poly:                 0x04c11db7     0xedb88320            */
  /* original:                 0x04c11db7     0x04c11db7            */
  /* init value:               -1             -1                    */
  /* swapped                   no             yes                   */
  /* bits in CRC:              32             32                    */
  /* ZIP used by COMPACTOR                                          */
  
  #include <stdio.h>
  #include <stdint.h>
  
  extern void exit();
  extern char *strcat();
  
  static void initcrctab();
  
  main()
  {
    initcrctab("ccitt", 0x1021, 0xffff, 0, 16);
    initcrctab("kermit", 0x8408, 0, 1, 16);
    initcrctab("arc", 0xa001, 0, 1, 16);
    initcrctab("binhex", 0x1021, 0, 0, 16);
    initcrctab("ccitt32",0x04c11db7,0xffffffff,0,32);
    initcrctab("zip",0xedb88320,0xffffffff,1,32);
    exit(0);
    /*NOTREACHED*/
  }
  
  static void initcrctab(name, poly, init, swapped, bits)
  char *name;
  int poly, init, swapped, bits;
  {
    register  int b, i;
    uint16_t v;
    uint32_t vv;
    FILE *fd;
    char buf[20];
  
    buf[0] = 0;
    (void)strcat(buf, name);
    (void)strcat(buf, ".c");
    if((fd = fopen(buf, "w")) == NULL) {
      (void)fprintf(stderr, "Cannot open %s for writing\n", buf);
      exit(1);
    }
    (void)fprintf(fd, "uint32_t %s_crcinit = %d;\n", name, init);
    (void)fprintf(fd, "\n");
    if(bits == 16) {
      (void)fprintf(fd, "static uint16_t crctab[256] = {\n");
    } else {
      (void)fprintf(fd, "static uint32_t crctab[256] = {\n");
    }
    (void)fprintf(fd, "    ");
    if(bits == 16) {
      for(b = 0; b < 256; ++b) {
        if(swapped) {
          for(v = b, i = 8; --i >= 0;)
            v = v & 1 ? (v>>1)^poly : v>>1;
        } else {
          for(v = b<<8, i = 8; --i >= 0;)
            v = v & 0x8000 ? (v<<1)^poly : v<<1;
        }
        (void)fprintf(fd, "0x%.4x,", v & 0xffff);
        if((b&7) == 7) {
          (void)fprintf(fd, "\n");
          if(b != 255) (void)fprintf(fd, "    ");
        } else {
          (void)fprintf(fd, " ");
        }
      }
    } else {
      for(b = 0; b < 256; ++b) {
        if(swapped) {
          for(vv = b, i = 8; --i >= 0;)
            vv = vv & 1 ? (vv>>1)^poly : vv>>1;
        } else {
          for(vv = b<<24, i = 8; --i >= 0;)
            vv = vv & 0x80000000 ? (vv<<1)^poly : vv<<1;
        }
        (void)fprintf(fd, "0x%.8x,", vv & 0xffffffff);
        if((b&3) == 3) {
          (void)fprintf(fd, "\n");
        if(b != 255) (void)fprintf(fd, "    ");
        } else {
          (void)fprintf(fd, " ");
        }
      }
    }
    (void)fprintf(fd, "};\n");
    (void)fprintf(fd, "\n");
    (void)fprintf(fd, "uint32_t %s_updcrc(icrc, icp, icnt)\n", name);
    (void)fprintf(fd, "    uint32_t icrc;\n");
    (void)fprintf(fd, "    uint8_t *icp;\n");
    (void)fprintf(fd, "    int icnt;\n");
    (void)fprintf(fd, "{\n");
    if(bits == 16) {
      (void)fprintf(fd, "#define M1 0xff\n");
      (void)fprintf(fd, "#define M2 0xff00\n");
    } else {
      (void)fprintf(fd, "#define M1 0xffffff\n");
      (void)fprintf(fd, "#define M2 0xffffff00\n");
    }
    (void)fprintf(fd, "    register uint32_t crc = icrc;\n");
    (void)fprintf(fd, "    register uint8_t *cp = icp;\n");
    (void)fprintf(fd, "    register int cnt = icnt;\n");
    (void)fprintf(fd, "\n");
    (void)fprintf(fd, "    while(cnt--) {\n");
  
    if(bits == 16) {
      if (swapped) {
        (void)fprintf(fd,
            "\tcrc=((crc>>8)&M1)^crctab[(crc&0xff)^*cp++];\n");
      } else {
        (void)fprintf(fd,
            "\tcrc=((crc<<8)&M2)^crctab[((crc>>8)&0xff)^*cp++];\n");
      }
    } else {
      if(swapped) {
        (void)fprintf(fd,
            "\tcrc=((crc>>8)&M1)^crctab[(crc&0xff)^*cp++];\n");
      } else {
        (void)fprintf(fd,
            "\tcrc=((crc<<8)&M2)^crctab[((crc>>24)&0xff)^*cp++];\n");
      }
    }
    (void)fprintf(fd, "    }\n");
    (void)fprintf(fd, "\n");
    (void)fprintf(fd, "    return(crc);\n");
    (void)fprintf(fd, "}\n");
    (void)fprintf(fd, "\n");
    (void)fclose(fd);
  }

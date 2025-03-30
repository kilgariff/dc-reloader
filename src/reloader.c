/*
   This file is made up of parts from the Sylverant PSO Patcher
   Copyright (C) 2013 Lawrence Sebald

   With modifications from Ross Kilgariff
   Copyright (C) 2025 Ross Kilgariff

   Some code is originally from KallistiOS
   Copyright (C) 2000 Megan Potter

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define CMD_PIOREAD 16  /**< \brief Read via PIO */
#define CMD_GETTOC2 19  /**< \brief Read TOC */
#define CMD_INIT    24  /**< \brief Initialize the drive */
#define CMD_GETSES  35  /**< \brief Get session */
#define ERR_OK          0   /**< \brief No error */
#define ERR_NO_DISC     1   /**< \brief No disc in drive */
#define ERR_DISC_CHG    2   /**< \brief Disc changed, but not reinitted yet */
#define ERR_SYS         3   /**< \brief System error */
#define ERR_ABORTED     4   /**< \brief Command aborted */
#define ERR_NO_ACTIVE   5   /**< \brief System inactive? */
#define NO_ACTIVE   0   /**< \brief System inactive? */
#define PROCESSING  1   /**< \brief Processing command */
#define COMPLETED   2   /**< \brief Command completed successfully */
#define ABORTED     3   /**< \brief Command aborted before completion */
#define TOC_LBA(n) ((n) & 0x00ffffff)
#define TOC_CTRL(n) ( ((n) & 0xf0000000) >> 28 )
#define TOC_TRACK(n) ( ((n) & 0x00ff0000) >> 16 )

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

static uint32 sector_buffer[512];

typedef struct {
    unsigned long entry[99];          /**< \brief TOC space for 99 tracks */
    unsigned long first;              /**< \brief Point A0 information (1st track) */
    unsigned long last;               /**< \brief Point A1 information (last track) */
    unsigned long leadout_sector;     /**< \brief Point A2 information (leadout) */
} CDROM_TOC;

static void memcpy(void *p1, const void *p2, uint32 count) {
    uint8 *b1 = (uint8 *)p1, *b2 = (uint8 *)p2;

    while(count--)
        *b1++ = *b2++;
}

/* GD-Rom BIOS calls... named mostly after Marcus' code. None have more
   than two parameters; R7 (fourth parameter) needs to describe
   which syscall we want. */

#define MAKE_SYSCALL(rs, p1, p2, idx) \
    unsigned int *syscall_bc = (unsigned int*)0x8c0000bc; \
    int (*syscall)() = (int (*)())(*syscall_bc); \
    rs syscall((p1), (p2), 0, (idx));

/* Reset system functions */
static void gdc_init_system() {
    MAKE_SYSCALL(/**/, 0, 0, 3);
}

/* Submit a command to the system */
static int gdc_req_cmd(int cmd, void *param) {
    MAKE_SYSCALL(return, cmd, param, 0);
}

/* Check status on an executed command */
static int gdc_get_cmd_stat(int f, void *status) {
    MAKE_SYSCALL(return, f, status, 1);
}

/* Execute submitted commands */
static void gdc_exec_server() {
    MAKE_SYSCALL(/**/, 0, 0, 2);
}

/* Check drive status and get disc type */
static int gdc_get_drv_stat(void *param) {
    MAKE_SYSCALL(return, param, 0, 4);
}

/* Set disc access mode */
static int gdc_change_data_type(void *param) {
    MAKE_SYSCALL(return, param, 0, 10);
}

/* The CD access mutex */
static int sector_size = 2048;   /* default 2048, 2352 for raw data reading */

/* Command execution sequence */
int cdrom_exec_cmd(int cmd, void *param) {
    int status[4] = {0};
    int f, n;

    /* Submit the command and wait for it to finish */
    f = gdc_req_cmd(cmd, param);

    do {
        gdc_exec_server();
        n = gdc_get_cmd_stat(f, status);
    }
    while(n == PROCESSING);

    if(n == COMPLETED)
        return ERR_OK;
    else if(n == ABORTED)
        return ERR_ABORTED;
    else if(n == NO_ACTIVE)
        return ERR_NO_ACTIVE;
    else {
        switch(status[0]) {
            case 2:
                return ERR_NO_DISC;
            case 6:
                return ERR_DISC_CHG;
            default:
                return ERR_SYS;
        }
    }
}

/* Re-init the drive, e.g., after a disc change, etc */
int cdrom_reinit() {
    int rv = ERR_OK;
    int r = -1, cdxa;
    unsigned int  params[4];
    int timeout;

    /* Try a few times; it might be busy. If it's still busy
       after this loop then it's probably really dead. */
    timeout = 10 * 1000;

    while(timeout > 0) {
        r = cdrom_exec_cmd(CMD_INIT, 0);

        if(r == 0) break;

        if(r == ERR_NO_DISC) {
            rv = r;
            goto exit;
        }
        else if(r == ERR_SYS) {
            rv = r;
            goto exit;
        }

        timeout--;
    }

    if(timeout <= 0) {
        rv = r;
        goto exit;
    }

    /* Check disc type and set parameters */
    gdc_get_drv_stat(params);
    cdxa = params[1] == 32;
    params[0] = 0;                      /* 0 = set, 1 = get */
    params[1] = 8192;                   /* ? */
    params[2] = cdxa ? 2048 : 1024;     /* CD-XA mode 1/2 */
    params[3] = sector_size;            /* sector size */

    if(gdc_change_data_type(params) < 0) {
        rv = ERR_SYS;
        goto exit;
    }

exit:
    return rv;
}

/* Initialize: assume no threading issues */
int cdrom_init() {
    unsigned int p;
    volatile unsigned int *react = (unsigned int*)0xa05f74e4,
                     *bios = (unsigned int*)0xa0000000;

    /* Reactivate drive: send the BIOS size and then read each
       word across the bus so the controller can verify it. */
    *react = 0x1fffff;

    for(p = 0; p < 0x200000 / 4; p++) {
        (void)bios[p];
    }

    /* Reset system functions */
    gdc_init_system();

    /* Do an initial initialization */
    cdrom_reinit();

    return 0;
}

/* Read the table of contents */
int cdrom_read_toc(CDROM_TOC *toc_buffer, int session) {
  struct {
      int session;
      void    *buffer;
  } params;
  int rv;

  params.session = session;
  params.buffer = toc_buffer;
  rv = cdrom_exec_cmd(CMD_GETTOC2, &params);

  return rv;
}

/* Enhanced Sector reading: Choose mode to read in. */
int cdrom_read_sectors_ex(void *buffer, int sector, int cnt) {
  struct {
      int sec, num;
      void *buffer;
      int is_test;
  } params;
  int rv = ERR_OK;
  unsigned buf_addr = (unsigned) buffer;

  params.sec = sector;    /* Starting sector */
  params.num = cnt;       /* Number of sectors */
  params.is_test = 0;     /* Enable test mode */
  params.buffer = buffer;

  if(buf_addr & 0x01) {
      // dbglog(DBG_ERROR, "cdrom_read_sectors_ex: Unaligned memory for PIO (2-byte).\n");
      return ERR_SYS;
  }
  rv = cdrom_exec_cmd(CMD_PIOREAD, &params);

  return rv;
}

static int ntohlp(unsigned char *ptr) {
  /* Convert the long word pointed to by ptr from big endian */
  return (ptr[0]<<24)|(ptr[1]<<16)|(ptr[2]<<8)|ptr[3];
}

static int fncompare(const char *fn1, int fn1len, const char *fn2, int fn2len) {
  /* Compare two filenames, disregarding verion number on fn2 if neccessary */
  while(fn2len--)
    if(!fn1len--)
      return *fn2 == ';';
    else if(*fn1++ != *fn2++)
      return 0;
  return fn1len == 0;
}

unsigned int find_datatrack(CDROM_TOC *toc) {
    int i, first, last;
    
    first = TOC_TRACK(toc->first);
    last = TOC_TRACK(toc->last);
    
    for(i = first; i <= last; ++i) {
        if(TOC_CTRL(toc->entry[i - 1]) == 4) {
            return TOC_LBA(toc->entry[i - 1]);
        }
    }
    
    return 0;
}

static int find_root(unsigned int *psec, unsigned int *plen) {
    /* Find location and length of root directory.
       Plain ISO9660 only.                         */
    static CDROM_TOC toc;
    unsigned int sec;

    cdrom_reinit();
    cdrom_read_toc(&toc, 0);
    sec = find_datatrack(&toc);
    cdrom_read_sectors_ex((char *)sector_buffer, sec+16, 1);

    /* Need to add 150 to LBA to get physical sector number */
    *psec = ntohlp(((unsigned char *)sector_buffer)+156+6) + 150;
    *plen = ntohlp(((unsigned char *)sector_buffer)+156+14);

    return 0;
}

static int low_find(unsigned int sec, unsigned int dirlen, int isdir,
		    unsigned int *psec, unsigned int *plen,
		    const char *fname, int fnlen) {
  /* Find a named entry in a directory */

  /* sec and dirlen points out the extent of the directory */

  /* psec and plen points to variables that will receive the extent
     of the file if found                                           */

  isdir = (isdir? 2 : 0);
  while(dirlen>0) {
    unsigned int i;
    unsigned char *rec = (unsigned char *)sector_buffer;
    cdrom_read_sectors_ex((char *)sector_buffer, sec, 1);
    for(i=0; i<2048 && i<dirlen && rec[0] != 0; i += rec[0], rec += rec[0]) {
      if((rec[25]&2) == isdir && fncompare(fname, fnlen, (char *)rec+33,
                                           rec[32])) {
        /* Entry found.  Copy start sector and length.  Add 150 to LBA. */
        *psec = ntohlp(rec+6)+150;
        *plen = ntohlp(rec+14);
        return 0;
      }
    }
    /* Not found, proceed to next sector */
    sec++;
    dirlen -= (dirlen>2048? 2048 : dirlen);
  }
  /* End of directory.  Entry not found. */
  return -3;
}

/* A file handle. */
static struct {
  unsigned int sec0;  /* First sector                     */
  unsigned int loc;   /* Current read position (in bytes) */
  unsigned int len;   /* Length of file (in bytes)        */
} fh;

int open(const char *path, int path_len) {
  unsigned int sec, len;

  /* Find the root directory */
  find_root(&sec, &len);

  /* Locate the file in the resulting directory */
  low_find(sec, len, 0, &sec, &len, path, path_len);

  /* Fill in the file handle */
  fh.sec0 = sec;
  fh.loc = 0;
  fh.len = len;
  return 0;
}

int pread(void *buf, unsigned int nbyte, unsigned int offset) {
  int r, t;

  /* If the read position is beyond the end of the file,
     return an empty read                                */
  if(offset>=fh.len)
    return 0;

  /* If the full read would span beyond the EOF, shorten the read */
  if(offset+nbyte > fh.len)
    nbyte = fh.len - offset;

  /* Read whole sectors directly into buf if possible */
  if(nbyte>=2048 && !(offset & 2047))
  {
    if((r = cdrom_read_sectors_ex(buf, fh.sec0 + (offset>>11), nbyte>>11)))
      return r;
    else {
      t = nbyte & ~2047;;
      buf = ((char *)buf) + t;
      offset += t;
      nbyte &= 2047;
    }
  }
  else
  {
    t = 0;
  }

  /* If all data has now been read, return */
  if(!nbyte)
    return t;

  /* Need to read parts of sectors */
  if((offset & 2047)+nbyte > 2048) {
    /* If more than one sector is involved, split the read
       up and recurse                                      */
    if((r = pread(buf, 2048-(offset & 2047), offset))<0)
      return r;
    else {
      t += r;
      buf = ((char *)buf) + r;
      offset += r;
      nbyte -= r;
    }
    if((r = pread(buf, nbyte, offset))<0)
      return r;
    else
      t += r;
  } else {
    /* Just one sector.  Read it and copy the relevant part. */
    if((r = cdrom_read_sectors_ex((char *)sector_buffer, fh.sec0+(offset>>11), 1)))
      return r;
    memcpy(buf, ((char *)sector_buffer)+(offset&2047), nbyte);
    t += nbyte;
  }
  return t;
}

int read(void *buf, unsigned int nbyte) {
  /* Use pread to read at the current position */
  int r = pread(buf, nbyte, fh.loc);
  /* Update current position */
  if(r>0)
    fh.loc += r;
  return r;
}

static uint8 *bin = (uint8 *)(0xac010000);

__attribute__ ((noreturn)) void go(unsigned int addr);

int boot_the_menu()
{
    int cur = 0, rsz;
    char const * filename = "MAIN.BIN";
    int filename_len = 8;

    cdrom_init();

    /* Read the binary in. This reads directly into the correct address. */
    open(filename, filename_len);

    while((rsz = read(bin + cur, 2048)) > 0) {
        cur += rsz;
    }

    go((unsigned int) bin);
}

int main()
{
    while (1)
    {
        boot_the_menu();
    }
    return 0;
}
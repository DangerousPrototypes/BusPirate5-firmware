#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "storage.h"

#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5

#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

uint32_t flash_read(void);
void flash_write_32(const uint32_t data, uint8_t count);
void flash_write(const uint32_t data);
void flash_start(void);
void flash_stop(void);
void flash_read_n(uint8_t *data, uint8_t count);

void flash_not_found()
{
    printf("not found\r\n");
}

typedef struct __attribute__((packed)) ptp_head_struct {
    uint32_t signature:32;
    uint8_t revision_minor:8;
    uint8_t revision_major:8;
    uint8_t headers_count:8;
} ptp_head_t;

typedef struct __attribute__((packed)) ptp_record_struct {
    uint8_t id:8;
    uint8_t revision_minor:8;
    uint8_t revision_major:8;
    uint8_t length_dwords:8;
    uint32_t address:24;
} ptp_record_t;

typedef struct __attribute__((packed)) ptp_jedec_struct {
    //table part 1
    uint8_t erase_size:2; //Block / Sector Erase sizes, 01:4KB erase, 11:not supported 4KB erase, other reserve.
    bool write_granularity:1; //0:1Byte,1:64Byte or larger
    bool volatile_status_register_block_protect_bits:1;
    bool volatile_status_register_write_enable_instruction_select:1; //0:50h, 1:06h
    uint8_t unused_1:3;
    uint8_t erase_instruction_4k:8;
    bool fast_read_112:1;
    uint8_t address_bytes:2;  //00=3, 01=3 or 4, 10 =4bytes
    bool dtr_clocking:1; //Double Transfer Rate clocking
    bool fast_read_122:1; 
    bool fast_read_144:1; 
    bool fast_read_114:1; 
    bool unused_2:1;
    uint8_t unused_3:8;
    //table part 2
    uint32_t density:32; //2 gigabits or less: bit-31 is set to 0b. 30:0 defines the size in bits 2^n.
    //table part 3
    uint8_t fast_read_144_wait_states:4; //0/4/8 
    uint8_t fast_read_144_mode_clocks:4; //0 or 2
    uint8_t fast_read_144_read_instruction:8;
    uint8_t fast_read_114_wait_states:4; //0/4/8 
    uint8_t fast_read_114_mode_clocks:4; //0 or 2
    uint8_t fast_read_114_read_instruction:8;   
    //table part 4
    uint8_t fast_read_112_wait_states:4; //0/4/8 
    uint8_t fast_read_112_mode_clocks:4; //0 or 2
    uint8_t fast_read_112_read_instruction:8;
    uint8_t fast_read_122_wait_states:4; //0/4/8 
    uint8_t fast_read_122_mode_clocks:4; //0 or 2
    uint8_t fast_read_122_read_instruction:8;       
    //table part 5
    bool fast_read_222:1;
    uint8_t unused_4:3;
    bool fast_read_444:1;
    uint8_t unused_5:3;
    uint32_t unused_6:24;
    //table part 6
    uint16_t unused_7:16;
    uint8_t fast_read_222_wait_states:4; //0/4/8 
    uint8_t fast_read_222_mode_clocks:4; //0 or 2
    uint8_t fast_read_222_read_instruction:8;    
    //table part 7
    uint16_t unused_8:16;
    uint8_t fast_read_444_wait_states:4; //0/4/8 
    uint8_t fast_read_444_mode_clocks:4; //0 or 2
    uint8_t fast_read_444_read_instruction:8; 
    //table part 8
    uint8_t erase_1_size:8; //Sector/block size=2N bytes(4) 0Ch:4KB;0Fh:32KB;10h:64KB
    uint8_t erase_1_instruction:8;
    uint8_t erase_2_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_2_instruction:8;    
    //table part 9
    uint8_t erase_3_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_3_instruction:8;
    uint8_t erase_4_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_4_instruction:8;    
} ptp_jedec_t;

typedef struct __attribute__((packed)) ptp_manuf_struct {
        uint16_t vcc_max :16;
        uint16_t vcc_min :16;
        bool reset_pin :1;
        bool hold_pin :1;
        bool deep_power_down_mode: 1;
        bool sw_reset:1;
        uint16_t sw_reset_instruction:8; //gcc doesn't like uint8_t bits across a boundary, using uint16
        bool program_suspend_resume:1;
        bool erase_suspend_resume:1;
        bool unused_1:1;
        bool wrap_read_mode:1;
        uint8_t wrap_read_instruction:8;
        uint8_t wrap_read_length:8;
        bool individual_block_lock:1;
        bool individual_block_lock_volatile:1;
        uint16_t individual_block_lock_instruction:8; //gcc doesn't like uint8_t bits across a boundary, using uint16
        bool individual_block_lock_volatile_default:1;
        bool secured_otp:1;
        bool read_lock:1;
        bool permanent_lock:1;
        bool unused_2:1;
        bool unused_3:1;
        uint16_t unused_4:16;
        uint32_t unused_5:32;
    } ptp_manuf_t;

void flash_probe()
{
    printf("Probing:\r\n Resume ID (0xAB): ");  
    //DP 0xB9: deep power down and then RDP 0xAB, 3 dummy bytes, 1 RES ID byte (release and read ID)
    //deep sleep command
    flash_start();
    flash_write(0xab);
    flash_stop();
    //resume and read ID
    busy_wait_ms(10);
    flash_start();
    flash_write_32(0xab000000, 4);
    uint8_t RESID = flash_read();
    flash_stop();

    if(RESID!=0x00 && RESID!=0xff)
    {
        printf("%02x\r\n", RESID);
    }
    else
    {
        flash_not_found();
    }

    printf(" REMS ID (0x90): ");
    //0x90: REMS  Read Electronic Manufacturer ID & Device ID (REMS)
    // 0x90, 0x00:3, 1 Manuf ID, 1 Device ID
    flash_start();
    flash_write_32(0x90000000,4);
    uint8_t REMS_MANUFID=flash_read();
    uint8_t REMS_DEVID=flash_read();
    flash_stop();
    if(REMS_MANUFID!=0x00 && REMS_MANUFID!=0xff) //TODO: manuf ID has a checksum bit or something, list of man and dev ids?
    {
        printf(" Manufacturer ID: %02x, Device ID: %02x\r\n", REMS_MANUFID, REMS_DEVID);
    }
    else
    {
        flash_not_found();
    }

    printf(" Read ID (0x9f): ");
    //0x9f: RDID  Read Identification (RDID)
    // 0x9f, 1 manuf ID, 1 memory type, 1 capacity
    flash_start();
    flash_write(0x9f);
    uint8_t RDID_MANUFID=flash_read();
    uint8_t RDID_MEMTYPE=flash_read();
    uint8_t RDID_MEMCAP=flash_read();
    flash_stop();
    if(RDID_MANUFID!=0x00 && RDID_MANUFID!=0xff)//TODO: is there a standard coding?
    {
        printf(" Manufacturer ID: %02x, Type: %02x, Capacity: %02x\r\n", RDID_MANUFID, RDID_MEMTYPE, RDID_MEMCAP);
    }
    else
    {
        flash_not_found();
    }
    

    //now grab Serial Flash Discoverable Parameter (SFDP)
    // 0x5a 3 byte address, dummy byte, read first 24bytes
    printf(" Read SFDP (0x5a): ");
    flash_start();
    flash_write_32(0x5a000000, 4);
    flash_write(0xff); //dummy byte
    uint8_t sfdp[36];
    flash_read_n(sfdp,8);
    flash_stop();

    ptp_head_t *ptp_head;
    ptp_head = (ptp_head_t *)&sfdp;
               
    //check code
    if(ptp_head->signature == 0x50444653)
    {
        printf("found 0x50444653\r\n");
    }
    else
    {
        flash_not_found();
        return;
    }

    printf(" Version: %d.%d\r\n", ptp_head->revision_major, ptp_head->revision_minor);
 
    uint8_t param_table_pointers=ptp_head->headers_count+1;
    printf(" Number of headers: %d\r\n\r\n", param_table_pointers); //n+1 parameter headers

    //loop over table pointers, usually 2?
    for(uint8_t i=0; i<param_table_pointers; i++)
    {
        printf("**Param Table %d**\r\n", i);
        flash_start();
        uint32_t address=0x5a000000 + 8 + (i*8);
        flash_write_32(address, 4);
        flash_write(0xff); //dummy byte
        flash_read_n(sfdp,8);
        flash_stop();
        const char ptp_jedec[]="JEDEC";
        const char ptp_manuf[]="manufacturer";
        ptp_record_t *ptp_rec;
        ptp_rec = (ptp_record_t *)&sfdp;

        uint8_t ptp_id = ptp_rec->id;
        #define PTP_JEDEC 0
        printf(" Type: %s (%02x)\r\n",ptp_id==PTP_JEDEC?ptp_manuf:ptp_manuf, ptp_id); //table of manuf IDs?
        printf(" Version: %d.%d\r\n", ptp_rec->revision_major, ptp_rec->revision_minor);
        uint8_t ptp_length=ptp_rec->length_dwords * 4;
        printf(" Length: %d bytes\r\n",ptp_length);
        uint32_t ptp_address=ptp_rec->address;
        printf(" Address: 0x%06x\r\n",ptp_address);
        printf(" Fetching table...\r\n");  

        if(ptp_length>count_of(sfdp))
        {
            printf(" Error: record too long to fetch! Skipping...\r\n");
            continue;
        }    
          
        flash_start();
        flash_write_32(0x5a000000 + ptp_address, 4);
        flash_write(0xff); //dummy byte
        flash_read_n(sfdp,ptp_length);
        flash_stop();





        switch(ptp_id)
        {
            case PTP_JEDEC:
                for(uint i=0; i<ptp_length; i++) printf("%02x ", sfdp[i]);
                printf("\r\n");
                ptp_jedec_t *ptp_j;
                ptp_j = (ptp_jedec_t *)&sfdp;
                printf("Block/sector 4K erase: %d\r\n", ptp_j->erase_size);
                printf("4K erase instruction: %02x\r\n", ptp_j->erase_instruction_4k);
                printf("Address bytes: %d\r\n", ptp_j->address_bytes);
                printf("Density: %d\r\n", ptp_j->density); //todo: math
                printf("1-1-2 fast read: %d\r\n", ptp_j->fast_read_112);
                if(ptp_j->fast_read_112)
                {
                    printf("1-1-2 fast read instruction: %02x\r\n", ptp_j->fast_read_112_read_instruction);
                }
                printf("1-2-2 fast read: %d\r\n", ptp_j->fast_read_122);
                if(ptp_j->fast_read_122)
                {
                    printf("1-2-2 fast read instruction: %02x\r\n", ptp_j->fast_read_122_read_instruction);
                }
                printf("1-4-4 fast read: %d\r\n", ptp_j->fast_read_144);
                if(ptp_j->fast_read_144)
                {
                    printf("1-4-4 fast read instruction: %02x\r\n", ptp_j->fast_read_144_read_instruction);
                }
                printf("1-1-4 fast read: %d\r\n", ptp_j->fast_read_114);
                if(ptp_j->fast_read_114)
                {
                    printf("1-1-4 fast read instruction: %02x\r\n", ptp_j->fast_read_114_read_instruction);
                }           
                printf("2-2-2 fast read: %d\r\n", ptp_j->fast_read_222);
                if(ptp_j->fast_read_222)
                {
                    printf("2-2-2 fast read instruction: %02x\r\n", ptp_j->fast_read_222_read_instruction);
                }    
                printf("4-4-4 fast read: %d\r\n", ptp_j->fast_read_444);
                if(ptp_j->fast_read_444)
                {
                    printf("4-4-4 fast read instruction: %02x\r\n", ptp_j->fast_read_444_read_instruction);
                }     

                printf("Erase 1 size: %d\r\n", ptp_j->erase_1_size); //todo: math
                printf("Erase 1 instruction: %02x\r\n", ptp_j->erase_1_instruction);     
                printf("Erase 2 size: %d\r\n", ptp_j->erase_2_size); //todo: math
                printf("Erase 2 instruction: %02x\r\n", ptp_j->erase_2_instruction);   
                printf("Erase 3 size: %d\r\n", ptp_j->erase_3_size); //todo: math
                printf("Erase 3 instruction: %02x\r\n", ptp_j->erase_3_instruction);   
                printf("Erase 4 size: %d\r\n", ptp_j->erase_4_size); //todo: math
                printf("Erase 4 instruction: %02x\r\n", ptp_j->erase_4_instruction);                                                                                                                  


                break;
            default:
                for(uint i=0; i<ptp_length; i++) printf("%02x ", sfdp[i]);
                printf("\r\n");  
                ptp_manuf_t *ptp;  
                ptp = (ptp_manuf_t *)&sfdp;

                printf(" Vcc: max %04xmV, min %04xmV\r\n", ptp->vcc_max, ptp->vcc_min);
                printf(" HW pins: #Reset %d, #Hold %d\r\n", ptp->reset_pin, ptp->hold_pin);
                printf(" Deep Power Down (DPDM): %d\r\n", ptp->deep_power_down_mode);
                printf(" SW reset: %d, opcode %02x\r\n", ptp->sw_reset, ptp->sw_reset_instruction);
                printf(" Suspend/Resume: Program %d, Erase %d\r\n", ptp->program_suspend_resume, ptp->erase_suspend_resume);
                printf(" Wrap Read mode: %d, opcode %02x, length %02x\r\n", ptp->wrap_read_mode, ptp->wrap_read_instruction, ptp->wrap_read_length);
                printf(" Individual block lock: %d, nonvolatile %d, opcode %02x, volatile default UNprotected %d\r\n", ptp->individual_block_lock, ptp->individual_block_lock_volatile, ptp->individual_block_lock_instruction, ptp->individual_block_lock_volatile_default);
                printf(" Secured OTP: %d\r\n", ptp->secured_otp);
                printf(" Read lock: %d\r\n", ptp->read_lock);
                printf(" Permanent lock: %d\r\n", ptp->permanent_lock);

                break;
        }

        

    }

      
 

    //version
    //JEDEC info and jump
    //Manuf info and jump
    // Read JEDEC location and length
    // Read Manuf location and length




}

void flash_set_cs(uint8_t cs)
{

	if(cs==M_SPI_SELECT) // 'start'
	{
		if(true) bio_put(M_SPI_CS, 0);
			else bio_put(M_SPI_CS, 1);
	}
	else			// 'stop' 
	{
		if(true) bio_put(M_SPI_CS, 1);
			else bio_put(M_SPI_CS, 0);
	}
}

uint8_t flash_xfer(const uint8_t out)
{
	uint8_t spi_in;
	spi_write_read_blocking(M_SPI_PORT, &out,&spi_in, 1);
	return spi_in;
}

void flash_start(void)
{
	flash_set_cs(M_SPI_SELECT);
}

void flash_stop(void)
{

	flash_set_cs(M_SPI_DESELECT);
}

void flash_write_32(const uint32_t data, uint8_t count)
{
    uint8_t sent=0;
    for(uint8_t i=4; i>0; i--)
    {
        flash_write(data>>(8*(i-1)));
        sent++;
        if(sent==count) return;
    }
}

void flash_write(uint32_t data)
{
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT))
	{
		tight_loop_contents();
	}

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS)
	{
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

void flash_read_n(uint8_t *data, uint8_t count)
{
    for(uint8_t i=0; i<count; i++)
    {
        data[i]=flash_read();
    }
}

uint32_t flash_read(void)
{
	while(!spi_is_writable(M_SPI_PORT));
	
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)0xff;

    while(!spi_is_readable(M_SPI_PORT));
	
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}


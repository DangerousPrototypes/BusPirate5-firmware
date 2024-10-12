all: main.elf

CFLAGS=-DF_CPU=16000000 -Wall -Wextra -O1 -mmcu=atmega328p

main.elf: *.c *.h
	avr-gcc $(CFLAGS) *.c -o main.elf

clean:
	rm -f main.elf

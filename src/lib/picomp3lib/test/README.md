# Off Target Test Harness
An off target test harness that can be run on a Raspberry Pi 4 or other Linux based system. Uses a minimal shim layer to emulate `FatFS`.  
By default the decoder is run for 5 seconds. This may result in the file being decoded multiple times. A wav file is created from the total output. How much faster than real time the decode runs, is reported.

To decode the entire mp3 file once only, define `NO_LOOP` in the `make` file

## To Build and Run
1. `make` from this (test) directory  
2. `.\build\decode input_mp3_file output_wav_file` 
### To Build using MS Visual Studio Code
1) Install the VSC Microsoft `Makefile Tool` extension  
2) Select the extensions settings (list installed extensions, click cog, select *Extension Settings* from menu)  
a) Set the `Makefile: Make Directory` to `./test`  
b) Set the `Makefile: Makefile Path` to `./test/Makefile`  
3) Set the Makefile Build target to build/decode 
4) Trigger build from the Makefile Extension build icon
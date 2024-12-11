#!/usr/bin/sh
# BusPirateSetup
# This script generates the file $HOME/.config/buspirate
# It lets you configure
#	1) If you have a BusPirate 5XL instead of a BusPirate 6
#	2) If you have a BusPirate 5 Rev 8 instead of the released BusPirate 5
#	3) The directory where new versions of the firmware are located
#   4) Which terminal emulation program you use

CONFIG=config
# You can re-run this program at any time to modify these values
# Optionally, you can edit ~/.config/buspirate/$CONFIG

# Time-stamp: <2024-12-04 09:18:21 (grymoire)>

# make sure there is a config file is there first
if [ ! -d "$HOME/.config" ]
then
    mkdir "$HOME/.config"
fi
if [ ! -d "$HOME/.config/buspirate" ]
then
    mkdir "$HOME/.config/buspirate"
fi
if [ ! -f "$HOME/.config/buspirate/${CONFIG}" ]
then
   touch "$HOME/.config/buspirate/${CONFIG}"
fi
   
   BP8=0
   printf "%s\n" "--------------------"
   printf "%s\n" "Selecting the default firmware for RP2040-based systems"
   printf "%s " "Do you want to use BusPirate5 Beta rev 8 firmware instead of the standard firmware? Y/N [N]"
   read -r ans
   case "$ans" in
       [Yy]* ) BP8=1;;
       *) ;;
   esac

   BP5XL=0
   printf "%s\n" "--------------------"
   printf "%s\n" "Selecting the default firmware for RP2350-based systems"
   printf "%s " "Do you want to use BusPirate 5XL firmware instead of BP6 firmware? Y/N [N]"
   read -r ans
   case "$ans" in
       [Yy]* ) BP5XL=1;;
       *) ;;
   esac

   printf "%s\n" "--------------------"
   printf "%s\n" "Selecting the default terminal software"
   printf "%s? " "m[inicom] | s[creen] | t[io] | p[utty] "
   read -r ans
   case "$ans" in
       [Mm]* )          # miniccom
           CMD="minicom -b \$BAUD -D \$PORT";
           ;;
       [Ss]* )          # screen
           CMD="screen \$PORT \$BAUD";
           ;;
       [Tt]* )          # tio
           CMD="tio -n --map INLCRNL,ODELBS -b \$BAUD \$PORT";
           ;;   
       [Pp]* )          # Putty
           # I'm picking a default font here - sorry
           CMD="putty \$PORT -serial -sercfg \$BAUD,8,n,1,N  -fn 'Monospace 12' ";
           ;;   
       * ) printf "I don't know how to configure this emulator '%s'\n" "$ans";
           exit;;
   esac

   printf "%s\n" "--------------------"
   printf "%s\n" "Selecting the default directory where you download/build *.uf2 files"
   printf "Default Directory (examples: . ~/Downloads etc.)? \n" 
   read -r ans
   FW="$ans"
   
   # Now I know how to build the config file

   # empty the file
   cp /dev/null "$HOME/.config/buspirate/${CONFIG}"

   if [ "$BP8" -eq 1 ]
   then
       printf "%s\n" "RP2040FIRMWARE=bus_pirate5_rev8.uf2" >>"$HOME/.config/buspirate/${CONFIG}"
   fi

   if [ "$BP5XL" -eq 1 ]
   then
       printf "%s\n" "RP2350FIRMWARE=bus_pirate5xl.uf2" >>"$HOME/.config/buspirate/${CONFIG}"
   fi

   printf "CMD=\"%s\"\n" "$CMD" >>"$HOME/.config/buspirate/${CONFIG}"
   printf "FW=\"%s\"\n" "$FW" >>"$HOME/.config/buspirate/${CONFIG}"   

   printf "%s\n" "Done"
   printf "%s\n" "You can edit ~/.config/buspirate/${CONFIG} manually if you wish"
    

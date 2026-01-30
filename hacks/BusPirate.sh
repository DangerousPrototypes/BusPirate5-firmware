#!/bin/sh
# Launch BusPirate using your choice of terminal emulators
# It checks for mounted file systems and warns you if they are missing
# If the BusPirate is in boot mode, it helps install new firmware,
# and prevents some errors like installing incompatible firmware
# There is a companion program - BusPirateSetup - that helps set things up.
# Run that program first

# Usage: BusPirate [-n] [-v] [terminal] [baud]
#       BusPirate -n       -- ignores the checks for mounted file system
#       BusPirate -v       -- echos the command, port, and baud rate
# Examples
#       BusPirate                             - uses the defaults
# 		BusPirate /dev/ttyACM2                - if it's on a different port
# 		BusPirate -n /dev/ttyACM0 11920       - Ignore mounted file system, specify al params
# To Install:
#    install BusPirateSetup.sh ~/bin/BusPirate

# Time-stamp: <2024-12-11 09:33:01 (grymoire)>


CONFIG=config # Configuration file in ~/.config/buspirate

connect() { # Connect to BusPirate
    # This function connects to the BP with your choice of terminal emulator
    # It evaluates the variable $CMD, which was set up by BusPirateSetup
    # You can override this variable here if you wish, and skip this setion
    #screen "$PORT" "$BAUD"
    #tio -n --map INLCRNL,ODELBS  -b "$BAUD" "$PORT"
    #minicom -p "$PORT" -b "$BAUD"
    # return
    # else we execute these commands
    if [ -z "$CMD" ]
    then
        printf "error: CMD not defined. Re-run BusPirateSetup\n"; exit
    fi

    if [ "$VERBOSE" -gt 0 ]
    then
           printf "Port: %s, Baud: %d\n" "$PORT" "$BAUD"
           printf "%s\n" "$CMD"
    fi

    eval "$CMD"

}

usage() {
    # shellcheck disable=SC2046
    printf "%s: ERROR: %s\n" $(basename "$0") "$@" 1>&2
    # shellcheck disable=SC2046
    printf "usage: %s [-n] [-v] [port] [baud]\n" $(basename "$0") 1>&2
    printf "          -n = Do not look for a /mnt/%s/BUS_PIRATE5 directory\n" "$USER" 1>&2
    printf "          -v = Print out command before executing"
	exit 1
}

setup() {
    # Check for config file
    if [ ! -f "$HOME/.config/buspirate/$CONFIG" ]
    then
        printf "%s\n%s " "First time use." "Do you want me to run BusPirateSetup?"
        read -r ans
        case $ans in
            [Yy]* )
                BusPirateSetup || printf "%s\n\t%s\n" "Install BusPirateSetup by typing" \
                                         "install BusPirateSetup.sh ~/bin/BusPirateSetup"
                exit
                ;;
            * )
                printf "%s\n" "Okay - I'll use the defaults"
                ;;
        esac

    fi
    }
check() {
    # Checks that the  Bus Pirate is mounted
    #Is it in bootloader mode and an BP5?
    for MOUNTLOCATION in "/media" "/run/media"
    do
      if [ -d "$MOUNTLOCATION/$USER/RPI-RP2" ]
      then
          if [ -f "$FW/$RP2040FIRMWARE" ]
          then
         		printf "Do you want me to install the new firmware %s? " "$FW/$RP2040FIRMWARE"
  	        read -r answer
              case "$answer" in
                  [Yy]* )
                      cp "$FW/$RP2040FIRMWARE" $MOUNTLOCATION/"$USER"/RPI-RP2
                      exit
                      ;;
                  * )
  	                printf "In boot mode - Unplug and replug the BusPirate\n"
                      exit
                      ;;
              esac
          else
              printf "%s\n" "I did not see the firmware located in $FW/$RP2040FIRMWARE"
  	        printf "%s\n" "Unplug and replug the BusPirate to get out of boot mode"
              exit
          fi

      elif [ -d "$MOUNTLOCATION/$USER/RP2350" ] # BusPirate 6 or 5xl
      then
          if [ -f "$FW/$RP2350FIRMWARE" ]
          then
              printf "Do you want me to install the new firmware %s? " "$FW/$RP2350FIRMWARE"
              read -r answer
              case "$answer" in
                  [Yy]* )
                      cp "$FW/$RP2350FIRMWARE" $MOUNTLOCATION/"$USER"/RP2350
                      exit
                      ;;
                  * )
  	                printf "In boot mode - Unplug and replug the BusPirate\n"
                      exit
                      ;;
              esac
          else

              printf "%s\n" "I did not see the firmware located in $FW/$RP2350FIRMWARE"
  	        printf "%s\n" "Unplug and replug the BusPirate to get out of boot mode"
              exit
          fi

      elif [ -d "$MOUNTLOCATION/$USER/BUS_PIRATE5/" ]
      then
          :
          # Looks good
          exit
  	  fi
    done
    printf "%s\n" "No file system mounted - Please plug in your Bus Pirate"
    exit 1
}



CHECK=1
VERBOSE=0

# Look for optional -n argument
if [ "$#" -gt 0 ]
then
    case "$1" in
        -n) CHECK=0; shift;;    # Disable mounted file systenm check
        -v) VERBOSE=1; shift;;  # enable verbose mode
        -*) usage "Unknown argument '$1'";;
    esac
fi

PORT=${1:-"/dev/ttyACM0"} # default port
BAUD=${2:-"11520"} # default baud rate - if you really want to change it

# setup
if [ ! -f "$HOME/.config/buspirate/${CONFIG}" ]
then
    setup
else
    # Default variables - if you don't specify them in the config file
    #Some variables you might need to change - if you have a config file, these values will be overwritten
    # by the values in the config file
    #RP2040FIRMWARE=bus_pirate5_rev8.uf2 # BusPirate 5 rev 8 prototype
    RP2040FIRMWARE=bus_pirate5_rev10.uf2
    #RP2350FIRMWARE=bus_pirate5xl.uf2
    RP2350FIRMWARE=bus_pirate6.uf2

    # Where would the firmware be located?
    FW=${BPFW:=.} # Use current directory

    # source the config file
    # shellcheck source=/dev/null
    . "$HOME/.config/buspirate/${CONFIG}"
fi



if [ "$CHECK" -eq 1 ]
then
    # check for mounted filesystems
    check
fi
connect

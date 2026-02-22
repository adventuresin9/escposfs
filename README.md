# escposfs

A Plan 9 sytle filesystem for ESC/POS receipt printers over USB.  Works on the Yirigui MHTP80.

## Install

Run 'mk' to just produce a .out file.  Run 'mk install' and it will install 'escposfs' into /usr/$user/bin/$objtype.

## How to use

Run 'usbtree' to see what endpoint your printer is on.

For example, run 'escposfs -u 6' if the printer is on endpoint 6.  It will search for the first Buld endpoint that can be writen to and use that to send print commands

By default, it post to /srv with 'escposfs' and mounts to '/mnt/escposfs'.  These can be changes with the -s and -m options.

Writing to the 'print' file will print.  Reading the 'ctl' file will display the current settings', and writing to it will change the settings.  The 'ctl' file can also take the 'cut' command to have the printer cut the paper.

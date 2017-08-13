# diibugger-cli

                        Command list:
                        
-----------------------------------------------------------------

exit

    Closes connection and exits

connect [ip]

     Connects to diibugger at a specified ip address if no ip is
     
     given, it will use one from ip.txt in the active directory
     
close

     Closes connection and waits
     
eval [statement]

     Evalutes a python statement
     
cmd [command]

     Evaluates a system command
     
read [address] [length]

     Prints out certain bytes in hex and ascii to the console
     
     aliases: preview, r
     
dump [address] [length] [filename]

     Dumps a certain area of memory to file
     
word [address] [value]

     Writes a uint32 to an address
     
     aliases: ww, writeword, int
     
float [address] [value]

     Writes a float to an address
     
hex [address] [hex]

     Writes bytes to a specified address
     
     aliases: bytes, w, writebytes
     
ppc [address] [length]

     Print disassembled code from the specified region
     
threads

     List thread info
     
stack

     Prints a stack trace
     
     aliases: stacktrace, trace
     
stackdump [stackNum] [filename]

     Dumps entire thread's stack to file (Note: get stacknum from "threads")
     
     aliases: sd
     
registers (or reg)

     Print registers
     
update (or u)

     Checks for exceptions
     
     this is run after every other command as well
     
breakpoints (or bps)

     List breakpoints
     
breapoint [address] (or bp)

     Toggle breakpoint
     
continue (or c)

     Continue past breakpoint
     
step (or s)

     Step from breakpoint to next line

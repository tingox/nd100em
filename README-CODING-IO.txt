To code for nd100em this document gives some information on
how to add functionality easily to the emulator.

IO functionality is basically done in two steps. The main
cpu threads executes IOX calls and thus for a device you
need a function for these IOX calls that return suitable
values or sets variables for the device. The actual device
then runs as a separate thread and does the real work.
To avoid race conditions we use a semaphore whenever we
access information in the common device structure created for
that device.

To add a new thread there is basically 2 functions we need
to consider:

pthread_t add_thread(void *funcpointer, bool is_jointype);
void start_threads(void);

The first one is the one actually initializing and starting the
thread, and the second is the main point where the emulator starts
all needed threads. This is where we would add a new device thread.
add_thread is called with two parameters:
* a pointer to our function, which from now on will run as a separate thread.
* and a boolean variable that if true tells the emulator the thread
  be waited for to join when it quits. If zero it is instead killed.

For example, the main cpu routine thread addition looks like this:

thread_id = add_thread(&cpu_thread,1);
if (debug) fprintf(debugfile,"Added thread id: %d as cpu_thread\n",thread_id);
if (debug) fflush(debugfile);

The main functionality for the cpu thread is done in another way.
Basically we have one array of 64k containing functionpointers:

void (*ioarr[65536])(ushort);

They are default initialised to a default function that fails
the IOX call after ~ 10uSec, as per real ND100 behaviour:

void Default_IO(ushort ioadd);

The routine that adds new IOX "device" functions is:

void Setup_IO_Handlers ();

We can take an example from currently added devices, the
onboard rtc on the cpu clock. It resides at IOX 10-13 octal,
and to add that one we have in the above routine:

IO_Handler_Add(8,11,&RTC_IO,NULL);

The parameters are start IO address, end IO adress (decimal),
pointer to function that handles device, and pointer to device
data structure. Last one is however not implemented, but thats
the way we will go.

We also have then the structure:

void (*iodata[65536]);

This if for the future IO device handling, as it allows reusing
the same function for multiple identical devices.
Since the function called for IOX is passed the IOX number as its
only parameter, every device can thus find it's own datastructure.



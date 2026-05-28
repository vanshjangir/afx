### AFX - Async Function Execution
Simple preemptive scheduler to execute functions asynchronously (for x86_64 linux only). Extremely fragile and entirely useless, obviously not intended to be used anywhere. Works for CPU-bound tasks and some specific non blocking syscalls. Silently breaks if something is blocking the thread. Implementation details are [here](https://vanshjangir.github.io/blog/1_afx.html) and [here](https://vanshjangir.github.io/blog/2_afx_part2.html).

### Compilation
* STACK_SIZE is the default size of the dedicated stack used by every async function if not passed while calling the function. If segfaults or core dumps are occuring for no obvious reasons, try increasing the STACK_SIZE. Optional to pass, default is 4096.
* UNIQUE_FD_STATES is the maximum number of unique states that can be stored for different file descriptors. Optional to pass, default is 10007.
* MAX_MS_TO_CLEAN is the maximum number of milliseconds since the last clean up. Optional to pass, default is 1 minute (1000*60 ms).
* `-fno-omit-frame-pointer` flag needs to be passed, as maintaining frame pointer is essential.
```
gcc -fno-omit-frame-pointer -DSTACK_SIZE=<stack_size> -DUNIQUE_FD_STATES=<number_of_states> -DMAX_MS_TO_CLEAN=<max_milliseconds_for_cleanup> /path/to/afx.c yourfile.c -o yourfile
```

### Usage (also see examples):
```c
#include "/path/to/afx.h"
// declaration
async_dec(<return_type>, <function_name>(arg1, arg2, ...));

// definition
async(
    <return_type>, <function_name>, (arg1, arg2, ...), {
        //body
    }
)

// call
function_name(arg1, arg2, ...);                     //normal
afx(function_name(arg1, arg2, ...));                //async
afx(function_name(arg1, arg2, ...), <stack_size>);  //async with custom stack size
```

### Docs
* `async_dec` macro is used to declare an async function.
* `async` macro converts a normal function into an async function.
* `afx` macro runs an async function. An async function can be run like a normal function as well.
* `afx_recv`, `afx_send`, `afx_accept` and `afx_connect` are async alternatives for recv, send, accept and connect functions provided by socket api. In an async function normal blocking syscalls does not work, so these alternatives can be used. They accept the same type of arguments. When using async_connect, you can also just make the socket non-blocking and use normal connect, which will be a little bit faster.
* `async_sleep` and `async_usleep` are alternatives to sleep and usleep.
* `afx_yield()` is a function that will schedule the next async function immediately.
* Try not to use any blocking function calls in an async function, their behaviour is undefined and may cause errors. Blocking functions are basically skipped or cancelled in-between, if the scheduler schedules another async function.

### Refs
- [CS-APP](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/books/CSAPP_2016.pdf)
- [Co-routines in asm](https://www.youtube.com/watch?v=sYSP_elDdZw)
- [How preemptive scheduling works in Go](https://www.reddit.com/r/golang/comments/1k3zqo6/if_goroutines_are_preemptive_since_go_114_how_do)
- [Death by timers in go](https://www.youtube.com/watch?v=h0s8CWpIKdg)
- [How Go scheduler works](https://www.youtube.com/watch?v=-K11rY57K7k)
- [Loop preemption in Go 1.14](https://www.youtube.com/watch?v=1I1WmeSjRSw)

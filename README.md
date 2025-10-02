### AFX - Async Function Execution
Simple preemptive scheduler to execute functions asynchronously (for x86_64 linux only). Extremely fragile and entirely useless, obviously not intended to be used anywhere. Works for CPU-bound tasks and some specific non blocking syscalls. Silently breaks if something is blocking the thread. Implementation details are [here](https://vanshjangir.github.io/blog/1_afx.html) and [here](https://vanshjangir.github.io/blog/2_afx_part2.html).

### Compilation
* STACK_SIZE is the size of the dedicated stack used by every async function, default size is 8KB. If segfaults or core dumps are occuring for no obvious reasons (the reason is my incompetency), try increasing the STACK_SIZE. Optional to pass, but if passed then make sure that it is a multiple of the page size on your OS, I don't really know the reason for that but it works.
* NUM_FUNC is the number of maximum async function that can be stored, by default is 10007. Optional to pass.
* `-fno-omit-frame-pointer` flag needs to be passed, as maintaining frame pointer is essential.
```
gcc -fno-omit-frame-pointer -DSTACK_SIZE=<stack_size> -DNUM_FUNC=<number_of_functions> /path/to/afx.c yourfile.c -o yourfile
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
afx(function_name(arg1, arg2, ...));    //async
function_name(arg1, arg2, ...);         //normal
```

### Docs
* `async_dec` macro is used to declare an async function.
* `async` macro converts a normal function into an async function.
* `afx` macro runs an async function. An async function can be run like a normal function as well.
* `afx_recv`, `afx_send`, `afx_accept` and `afx_connect` are async alternatives for recv, send, accept and connect functions provided by socket api. In an async function normal blocking syscalls does not work, so these alternatives can be used. They accept the same type of arguments. When using async_connect, you can also just make the socket non-blocking and use normal connect, which will be a little bit faster.
* `async_sleep` and `async_usleep` are alternatives to sleep and usleep.
* `afx_yield()` is a function that will schedule the next async function immediately.
* Try not to use any blocking syscalls in an async function, their behaviour is undefined and may cause errors. These syscalls are basically skipped or cancelled in-between, if the scheduler schedules another function.

### Refs
- [CS-APP](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/books/CSAPP_2016.pdf)
- [Co-routines in asm](https://www.youtube.com/watch?v=sYSP_elDdZw)
- [How preemptive scheduling works in Go](https://www.reddit.com/r/golang/comments/1k3zqo6/if_goroutines_are_preemptive_since_go_114_how_do)
- [Death by timers in go](https://www.youtube.com/watch?v=h0s8CWpIKdg)
- [How Go scheduler works](https://www.youtube.com/watch?v=-K11rY57K7k)
- [Loop preemption in Go 1.14](https://www.youtube.com/watch?v=1I1WmeSjRSw)

### AFX - Async Function Execution
Simple preemptive scheduler to execute functions asynchronously(x86_64 only). Works for cpu bound tasks only, i.e. anything that makes a thread go to sleep will not work reliably. Implementation details [here](https://vanshjangir.github.io/devlog/1_afx.html).

### Compilation
STACK_SIZE is the size of the dedicated stack used by every async function/routine. If not passed, it will be 4KB. Smaller STACK_SIZE can lead to segfaults.
```
gcc -DSTACK_SIZE=<stack_size> /path/to/afx.c yourfile.c -o yourfile
```

### Usage (also see examples):
```c
#include "/path/to/afx.h"

// definition
async(
    <return_type>, <function_name>, (arg1, arg2, ..), {
        //body
    }
)

// call
afx(function_name(arg1, arg2, ...));
```

### Refs:
- [CS-APP](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/books/CSAPP_2016.pdf)
- [Co-routines in asm](https://www.youtube.com/watch?v=sYSP_elDdZw)
- [How preemptive scheduling works in Go](https://www.reddit.com/r/golang/comments/1k3zqo6/if_goroutines_are_preemptive_since_go_114_how_do)
- [Death by timers in go](https://www.youtube.com/watch?v=h0s8CWpIKdg)
- [How Go scheduler works](https://www.youtube.com/watch?v=-K11rY57K7k)
- [Loop preemption in Go 1.14](https://www.youtube.com/watch?v=1I1WmeSjRSw)

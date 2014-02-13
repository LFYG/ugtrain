#!/bin/bash

# The issue is that code address and stack offset for mallocs can differ
# from distribution to distribution. But the thing which remains constant
# is often the way how the code internally works.

# chromium-bsu 0.9.14.1 or 0.9.15

# We already know that chromium-bsu is a 64-bit C++ application. Therefore,
# objects are initialized most likely by the "_Znwm" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (288 or
# 0x120) for the object in which the NUMBER OF LIVES is stored.
objdump -xD `which chromium-bsu` | grep "_Znwm" -B 2 -A 1 | grep -A 3 0x120

# Should return something like this:
#  411086:       bf 20 01 00 00          mov    $0x120,%edi
#  41108b:       48 89 1d fe 14 24 00    mov    %rbx,0x2414fe(%rip)        # 652590 <_ZTIPc+0x2f0>
#  411092:       e8 31 36 ff ff          callq  4046c8 <_Znwm@plt>
#  411097:       48 89 c7                mov    %rax,%rdi

# This shows us that 0x411097 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end addresses at 0x0 (NULL) as we already know the unique
# code address and malloc size.

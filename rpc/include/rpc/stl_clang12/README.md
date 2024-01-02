This is code cherry picked from https://github.com/microsoft/STL commit 086b072f which is just before their minimum allowed version of clang went from 12 to 13
If you ever need to patch this to a later version I would recommend forking https://github.com/microsoft/STL branching off commit 086b072f and pasting in this code.  Then merge in whatever commit on main that you want and fix the damage.

Most changes involved specifying explictly setting the namespace to std and tweeking some #defines normally specified in VCruntime.  

https://github.com/microsoft/STL is designed by Microsoft to make clang and visual C++ to compile on windows with the same stl implementation.

The downside is that some windows intrinsics are not available on linux or sgx and so alternatives are required:  
_InterlockedCompareExchange is swapped for __sync_val_compare_and_swap
_InterlockedIncrement is swapped for __sync_add_and_fetch
_InterlockedDecrement is swapped for __sync_sub_and_fetch

Also __iso_volatile_load32 seems to have no equivelant and may be something that does not apply to x64 processors as they have guarantees of synchronisity between cores where other chipset do not.
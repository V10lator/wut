/*
 * Copyright (C) 2015-2020 exjam <james.benton2@gmail.com>
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <coreinit/dynload.h>
#include <coreinit/memheap.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/memory.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>

static MEMHeapHandle sCustomHeap = NULL;
static uint32_t sCustomHeapAddr = 0;
static uint32_t sCustomHeapSize = 0;

static void *
CustomAllocFromDefaultHeap(uint32_t size)
{
   return MEMAllocFromExpHeapEx(sCustomHeap, size, 4);
}

static void *
CustomAllocFromDefaultHeapEx(uint32_t size,
                             int32_t alignment)
{
   return MEMAllocFromExpHeapEx(sCustomHeap, size, alignment);
}

static void
CustomFreeToDefaultHeap(void* ptr)
{
   MEMFreeToExpHeap(sCustomHeap, ptr);
}

static OSDynLoad_Error
CustomDynLoadAlloc(int32_t size,
                   int32_t align,
                   void **outAddr)
{
   if (!outAddr) {
      return OS_DYNLOAD_INVALID_ALLOCATOR_PTR;
   }

   if (align >= 0 && align < 4) {
      align = 4;
   } else if (align < 0 && align > -4) {
      align = -4;
   }

   if (!(*outAddr = MEMAllocFromDefaultHeapEx(size, align))) {
      return OS_DYNLOAD_OUT_OF_MEMORY;
   }

   return OS_DYNLOAD_OK;
}

static void
CustomDynLoadFree(void *addr)
{
   MEMFreeToDefaultHeap(addr);
}

void
__preinit_user(MEMHeapHandle *mem1,
               MEMHeapHandle *foreground,
               MEMHeapHandle *mem2)
{
   uint32_t addr, size;

   MEMAllocFromDefaultHeap = CustomAllocFromDefaultHeap;
   MEMAllocFromDefaultHeapEx = CustomAllocFromDefaultHeapEx;
   MEMFreeToDefaultHeap = CustomFreeToDefaultHeap;

   if (OSGetForegroundBucket(NULL, NULL)) {
      OSGetMemBound(OS_MEM1, &addr, &size);
      *mem1 = MEMCreateFrmHeapEx((void *)addr, size, 0);

      OSGetForegroundBucketFreeArea(&addr, &size);
      *foreground = MEMCreateFrmHeapEx((void *)addr, size, 0);
   }

   OSGetMemBound(OS_MEM2, &addr, &size);
   sCustomHeap = MEMCreateExpHeapEx((void *)addr, size, MEM_HEAP_FLAG_USE_LOCK);
   sCustomHeapAddr = addr;
   sCustomHeapSize = size;
   *mem2 = sCustomHeap;

   OSDynLoad_SetAllocator(CustomDynLoadAlloc, CustomDynLoadFree);
   OSDynLoad_SetTLSAllocator(CustomDynLoadAlloc, CustomDynLoadFree);
}


void
heapBlockVisitor(void *block,
                 MEMHeapHandle heap,
                 void *context)
{
   WHBLogPrintf("Block 0x%08X, size 0x%08X, group 0x%04X, direction %d",
                (uint32_t)block,
                MEMGetSizeForMBlockExpHeap(block),
                MEMGetGroupIDForMBlockExpHeap(block),
                MEMGetAllocDirForMBlockExpHeap(block));
}

int
main(int argc, char **argv)
{
   WHBProcInit();
   WHBLogConsoleInit();
   WHBLogPrintf("sCustomHeapAddr = 0x%08X", sCustomHeapAddr);
   WHBLogPrintf("sCustomHeapSize = 0x%08X", sCustomHeapSize);

   WHBLogPrintf("MEMGetTotalFreeSizeForExpHeap = 0x%08X", MEMGetTotalFreeSizeForExpHeap(sCustomHeap));
   WHBLogPrintf("MEMGetAllocatableSizeForExpHeapEx = 0x%08X", MEMGetAllocatableSizeForExpHeapEx(sCustomHeap, 4));

   MEMVisitAllocatedForExpHeap(sCustomHeap, heapBlockVisitor, NULL);

   while(WHBProcIsRunning()) {
      WHBLogConsoleDraw();
      OSSleepTicks(OSMillisecondsToTicks(100));
   }

   WHBLogConsoleFree();
   WHBProcShutdown();
   return 0;
}

#ifndef __BBB_TYPES_H__
#define __BBB_TYPES_H__

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

//! @name Test results
typedef enum _test_return
{
    TEST_NOT_STARTED     = -3, // present in the menu, but not run
    TEST_NOT_IMPLEMENTED = -2, // present in the menu, but not functional
    TEST_FAILED          = -1,
    TEST_PASSED          = 0,
    TEST_BYPASSED        = 2,  // user elected to exit the test before it was run   
    TEST_NOT_PRESENT     = 3,  // not present in the menu.
    TEST_CONTINUE        = 4   // proceed with the test. opposite of TEST_BYPASSED  
} test_return_t;

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef FAIL
#define FAIL 1
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#endif

Title: sprintf() APIs

Description:

This test verifies that sprintf() and its variants operate as expected.

--------------------------------------------------------------------------------

Building and Running Project:

This project outputs to the console.  It can be built and executed
on QEMU as follows:

    make run

--------------------------------------------------------------------------------

Troubleshooting:

Problems caused by out-dated project information can be addressed by
issuing one of the following commands then rebuilding the project:

    make clean          # discard results of previous builds
                        # but keep existing configuration info
or
    make pristine       # discard results of previous builds
                        # and restore pre-defined configuration info

--------------------------------------------------------------------------------
Sample Output:

tc_start() - Test sprintf APIs

===================================================================
Testing sprintf() with integers ....
Testing snprintf() ....
Testing vsprintf() ....
Testing vsnprintf() ....
Testing sprintf() with strings ....
Testing sprintf() with misc options ....
Testing sprintf() with doubles ....
===================================================================
PASS - RegressionTask.
===================================================================
PROJECT EXECUTION SUCCESSFUL

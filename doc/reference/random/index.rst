.. _random_reference:

Random
######

The random API subsystem provides random number generation APIs in both
cryptographically and non-cryptographically secure instances. Which
random API to use is based on the cryptographic requirements of the
random number. The non-cryptographic APIs will return random values
much faster if non-cryptographic values are needed.

The cryptographically secure random functions shall be compliant to the
FIPS 140-2 [NIST02]_ recommended algorithms. Hardware based random-number
generators (RNG) can be used on platforms with appropriate hardware support.
Platforms without hardware RNG support shall use the `CTR-DRBG algorithm
<https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90Ar1.pdf>`_.
The algorithm can be provided by `TinyCrypt <https://01.org/tinycrypt>`_
or `mbedTLS <https://tls.mbed.org/ctr-drbg-source-code>`_ depending on
your application performance and resource requirements.

  .. note::

    The CTR-DRBG generator needs an entropy source to establish and
    maintain the cryptographic security of the PRNG.

.. _random_kconfig:

Kconfig Options
***************

These options can be found in the following path :zephyr_file:`subsys/random/Kconfig`.

:option:`CONFIG_TEST_RANDOM_GENERATOR`
 For testing, this option permits random number APIs to return values
 that are not truly random.

The random number generator choice group allows selection of the RNG
source function for the system via the RNG_GENERATOR_CHOICE choice group.
An override of the default value can be specified in the SOC or board
.defconfig file by using:

.. code-block:: none

   choice RNG_GENERATOR_CHOICE
	   default XOROSHIRO_RANDOM_GENERATOR
   endchoice

The random number generators available include:

:option:`CONFIG_X86_TSC_RANDOM_GENERATOR`
 enables number generator based on timestamp counter of x86 boards,
 obtained with rdtsc instruction.

:option:`CONFIG_TIMER_RANDOM_GENERATOR`
 enables number generator based on system timer clock. This number
 generator is not random and used for testing only.

:option:`CONFIG_ENTROPY_DEVICE_RANDOM_GENERATOR`
 enables a random number generator that uses the enabled hardware
 entropy gathering driver to generate random numbers.

:option:`CONFIG_XOROSHIRO_RANDOM_GENERATOR`
 enables the Xoroshiro128+ pseudo-random number generator, that uses the
 entropy driver as a seed source.

The CSPRNG_GENERATOR_CHOICE choice group provides selection of the
cryptographically secure random number generator source function. An
override of the default value can be specified in the SOC or board
.defconfig file by using:

.. code-block:: none

   choice CSPRNG_GENERATOR_CHOICE
	   default CTR_DRBG_CSPRNG_GENERATOR
   endchoice

The cryptographically secure random number generators available include:

:option:`CONFIG_HARDWARE_DEVICE_CS_GENERATOR`
 enables a cryptographically secure random number generator using the
 hardware random generator driver

:option:`CONFIG_CTR_DRBG_CSPRNG_GENERATOR`
 enables the CTR-DRBG pseudo-random number generator. The CTR-DRBG is
 a FIPS140-2 recommended cryptographically secure random number generator.

Personalization data can be provided in addition to the entropy source
to make the initialization of the CTR-DRBG as unique as possible.

:option:`CONFIG_CS_CTR_DRBG_PERSONALIZATION`
 CTR-DRBG Initialization Personalization string

.. _random_api:

API Reference
*************

.. doxygengroup:: random_api
   :project: Zephyr

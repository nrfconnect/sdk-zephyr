#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

if(FIRST_BOILERPLATE_EXECUTION)
  get_property(
    partition_manager_config_files
    GLOBAL PROPERTY
    PARTITION_MANAGER_CONFIG_FILES
    )

  if(partition_manager_config_files)
    # Partition manager is enabled because we have populated config
    # files.

    execute_process(
      COMMAND
      ${PYTHON_EXECUTABLE}
      ${ZEPHYR_BASE}/scripts/partition_manager.py
      --input ${partition_manager_config_files}
      --app-pm-config-dir ${PROJECT_BINARY_DIR}/include/generated
      )

    # Make Partition Manager configuration available in CMake
    import_kconfig(PM_ ${PROJECT_BINARY_DIR}/include/generated/pm.config)

    # Create a dummy target that we can add properties to for
    # extraction in generator expressions.
    add_custom_target(partition_manager)

    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_SLOT_SIZE
      ${PM_MCUBOOT_PARTITIONS_PRIMARY_SIZE}
      )
    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_HEADER_SIZE
      ${PM_MCUBOOT_PAD_SIZE}
      )
    set_property(
      TARGET partition_manager
      PROPERTY MCUBOOT_SECONDARY_ADDRESS
      ${PM_MCUBOOT_PARTITIONS_SECONDARY_ADDRESS}
      )

    get_property(
      hex_files_to_merge
      GLOBAL PROPERTY
      PM_HEX_FILES_TO_MERGE
      )
    if (hex_files_to_merge AND NOT PM_MCUBOOT_ADDRESS)
      # Unfortunately, we can not re-use the merging functionality of the
      # zephyr build system when using the partition manager. This because it
      # is not known until all sub-images has been included whether or not
      # some special handling is required (e.g. with mcuboot).

      get_property(
        hex_files_to_merge_targets
        GLOBAL PROPERTY
        PM_HEX_FILES_TO_MERGE_TARGETS
        )

      set(merged_hex ${PROJECT_BINARY_DIR}/merged.hex)
      add_custom_command(
        OUTPUT ${merged_hex}
        COMMAND
        ${PYTHON_EXECUTABLE}
        ${ZEPHYR_BASE}/scripts/mergehex.py
        -o ${PROJECT_BINARY_DIR}/merged.hex
        ${hex_files_to_merge}
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
        DEPENDS
        ${hex_files_to_merge_targets}
        kernel_elf
        )
      add_custom_target(pm_mergehex ALL DEPENDS ${merged_hex})

      set(ZEPHYR_RUNNER_CONFIG_KERNEL_HEX "${merged_hex}"
        CACHE STRING "Path to merged image in Intel Hex format" FORCE)

    elseif (PM_SPM_ADDRESS AND PM_MCUBOOT_ADDRESS)
      # Special handling needed to merge before signing.
      set(merged_to_sign_hex ${PROJECT_BINARY_DIR}/merged_to_sign.hex)
      add_custom_command(
        OUTPUT ${PROJECT_BINARY_DIR}/merged_to_sign.hex
        COMMAND
        ${PYTHON_EXECUTABLE}
        ${ZEPHYR_BASE}/scripts/mergehex.py
        -o ${merged_to_sign_hex}
        ${PM_SPM_BUILD_DIR}/zephyr.hex
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
        DEPENDS
        spm_kernel_elf
        kernel_elf
        )
      add_custom_target(merged_to_sign_target DEPENDS ${merged_to_sign_hex})

      set_property(
        TARGET partition_manager
        PROPERTY MCUBOOT_TO_SIGN
        ${merged_to_sign_hex}
        )
      set_property(
        TARGET partition_manager
        PROPERTY MCUBOOT_TO_SIGN_DEPENDS
        merged_to_sign_target
        )
    endif()
  endif()
endif()
